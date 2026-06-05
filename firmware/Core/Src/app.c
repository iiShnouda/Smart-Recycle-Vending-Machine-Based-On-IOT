/*
 * app.c — main application: ties the drivers together + a line-based
 * command protocol over USB-CDC (the link to the Raspberry Pi kiosk).
 *
 * Call App_Init() once after CubeMX init, then App_Task() in the main
 * while(1). Feed received CDC bytes to App_OnRx(); App emits replies via
 * the weak App_Send() (wire it to CDC_Transmit_FS in your usbd glue).
 *
 * Commands (newline-terminated ASCII):
 *   PING                      -> PONG
 *   WEIGH                     -> W,<c0>,<c1>,...,<c7>     (raw HX711)
 *   IR                        -> IR,<mask>                (bit i = slot i)
 *   DOOR                      -> DOOR,0|1
 *   RELAY <n> <0|1>           -> OK
 *   SERVO <deg>               -> OK
 *   DISPENSE <motor0_7> <steps> [hz]  -> OK (TMC, muxed)
 *   AUGER <steps> [hz]        -> OK (TP6600)
 *   STOP                      -> OK (abort both axes)
 */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "main.h"
#include "bsp.h"
#include "hx711_bank.h"
#include "load_cell.h"
#include "stepper.h"
#include "sensors.h"
#include "neopixel.h"
#include "ina219.h"
#include "recycle.h"

void  Servo_Init(void);
void  Servo_SetAngle(uint8_t deg);

/* Wire this to CDC_Transmit_FS(...) in usbd_cdc_if.c. */
__weak void App_Send(const char *s) { (void)s; }

static char     s_line[96];
static uint16_t s_len;

static void reply(const char *s) { App_Send(s); }

void App_Init(void)
{
    BSP_Init();
    Stepper_Init();
    Servo_Init();
    Sensors_Init();
    Neo_Init();
    LoadCell_Init();
    INA219_Init();
    Recycle_Init(App_Send);          /* recycle events stream to the Pi */
    reply("BOOT,SRVM_V2.1\n");
}

static void handle(char *line)
{
    char cmd[16] = {0}, arg[16] = {0};
    int  a = 0, b = 0, c = 0;
    int  n = sscanf(line, "%15s %d %d %d", cmd, &a, &b, &c);
    sscanf(line, "%*s %15s", arg);          /* string 2nd token (verdicts) */

    if (!strcmp(cmd, "PING")) { reply("PONG\n"); return; }

    /* ── Recycle lane (driven by the Pi while in Recycle mode) ───────── */
    if (!strcmp(cmd, "RECYCLE") && n >= 2) { Recycle_Arm(a != 0); reply("OK\n"); return; }
    if (!strcmp(cmd, "BASKETS") && n >= 3) { Recycle_SetBaskets(a != 0, b != 0); reply("OK\n"); return; }
    if (!strcmp(cmd, "VERDICT")) {
        if      (!strcmp(arg, "BOTTLE")) Recycle_Verdict(V_BOTTLE, REJ_NONE);
        else if (!strcmp(arg, "CAN"))    Recycle_Verdict(V_CAN,    REJ_NONE);
        else                             Recycle_Verdict(V_REJECT, REJ_NOT_RECYCLABLE);
        reply("OK\n"); return;
    }
    if (!strcmp(cmd, "POWER")) {           /* INA219 snapshot */
        char o[64];
        snprintf(o, sizeof o, "POWER,%ld,%ld,%ld\n",
                 (long)INA219_BusVoltage_mV(), (long)INA219_Current_mA(),
                 (long)INA219_Power_mW());
        reply(o); return;
    }

    if (!strcmp(cmd, "WEIGH")) {
        int32_t w[HX711_COUNT];
        char out[96];
        if (HX711_Bank_Read(w, 200) == HX711_COUNT) {
            int o = snprintf(out, sizeof out, "W");
            for (int i = 0; i < HX711_COUNT; ++i)
                o += snprintf(out + o, sizeof out - o, ",%ld", (long)w[i]);
            snprintf(out + o, sizeof out - o, "\n");
            reply(out);
        } else reply("ERR,WEIGH_TIMEOUT\n");
        return;
    }
    if (!strcmp(cmd, "IR"))   { char o[16]; snprintf(o,sizeof o,"IR,%u\n",BSP_IR_Mask()); reply(o); return; }
    if (!strcmp(cmd, "DOOR")) { reply(BSP_DoorClosed()? "DOOR,1\n":"DOOR,0\n"); return; }

    if (!strcmp(cmd, "RELAY") && n >= 3) { BSP_Relay((uint8_t)a, b!=0); reply("OK\n"); return; }
    if (!strcmp(cmd, "SERVO") && n >= 2) { Servo_SetAngle((uint8_t)a);  reply("OK\n"); return; }

    if (!strcmp(cmd, "DISPENSE") && n >= 3) {     /* a=motor 0..7, b=steps, c=hz */
        Stepper_SelectMotor((uint8_t)a);
        Stepper_Move(DRV_TMC, b, c > 0 ? (uint32_t)c : 2000);
        reply("OK\n"); return;
    }
    if (!strcmp(cmd, "AUGER") && n >= 2) {         /* a=steps, b=hz */
        Stepper_Move(DRV_TP, a, b > 0 ? (uint32_t)b : 1500);
        reply("OK\n"); return;
    }
    if (!strcmp(cmd, "STOP")) { Stepper_Abort(DRV_TMC); Stepper_Abort(DRV_TP); reply("OK\n"); return; }

    reply("ERR,UNKNOWN\n");
}

void App_OnRx(const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; ++i) {
        char ch = (char)data[i];
        if (ch == '\n' || ch == '\r') {
            if (s_len) { s_line[s_len] = 0; handle(s_line); s_len = 0; }
        } else if (s_len < sizeof(s_line) - 1) {
            s_line[s_len++] = ch;
        }
    }
}

void App_Task(void)
{
    /* 5 ms cadence: debounce sensors, then run the recycle state machine. */
    static uint32_t tick = 0;
    if (HAL_GetTick() - tick < 5) return;
    tick = HAL_GetTick();

    Sensors_Poll();
    Recycle_Poll();

    /* Door edge events (reed lives on the STM32). */
    bool closed;
    if (Sensors_DoorChanged(&closed))
        reply(closed ? "EVT,DOOR,1\n" : "EVT,DOOR,0\n");
}
