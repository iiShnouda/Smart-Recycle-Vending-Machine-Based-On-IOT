/* protocol.c -----------------------------------------------------------------
 * Table-driven dispatcher for the USB-CDC line protocol.
 *
 * Wire format the kiosk (Pi) speaks is mixed — some commands are colon-joined
 * ("DISPENSE:3", "RELAY:1:1") and some are space-joined ("RECYCLE 1",
 * "VERDICT BOTTLE", "BASKETS 1 0"). So the dispatcher accepts BOTH ':' and ' '
 * as the name/argument separator, and arguments may be split by either.
 *
 * Replies MUST start with a token the kiosk's Serial_Connection recognises as
 * a resolution, or it retries 3× then times out (this was the "every test
 * button fails / serial is spammy" bug):
 *     success  → "PONG" | "OK ..." | "Done ..."
 *     failure  → "Error ..."
 * Unsolicited events (door edges, recycle EVT,*) are fire-and-forget and use
 * none of those prefixes so the kiosk treats them as notifications.
 * --------------------------------------------------------------------------*/
#include "protocol.h"
#include "usbd_cdc_if.h"        /* CDC_Transmit_FS */

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>

#include "pin_map.h"
#include "servo.h"
#include "ir_sensors.h"
#include "reed.h"
#include "rtc_sync.h"
#include "motor_mux.h"
#include "hx711_bank.h"
#include "relays.h"

/* Stepper still uses the App/ task queue so STEP requests are non-blocking
 * from the dispatcher's point of view (the queue absorbs back-pressure).   */
#include "stepper_task.h"
#include "dispense.h"

/* ── Reply helper ──────────────────────────────────────────────────────────── */
void Protocol_Reply(const char *fmt, ...)
{
    char    buf[160];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf) - 2, fmt, ap);
    va_end(ap);
    if (n <= 0) return;
    if ((size_t)n >= sizeof(buf) - 2) n = (int)(sizeof(buf) - 2);
    buf[n++] = '\r';
    buf[n++] = '\n';
    CDC_Transmit_FS((uint8_t *)buf, (uint16_t)n);
}

/* ── Arg parsing (separator-agnostic: ':' ' ' ',' '\t') ────────────────────── */
static int is_sep(char c) { return c == ':' || c == ' ' || c == ',' || c == '\t'; }

/* Parse the next integer from *pp, skipping any leading separators. Advances
 * *pp past the number. *ok is set 0 if no digits were found. */
static long next_int(const char **pp, int *ok)
{
    const char *p = *pp;
    while (*p && is_sep(*p)) ++p;
    char *end = NULL;
    long v = strtol(p, &end, 10);
    if (ok) *ok = (end != p);
    *pp = (end && end != p) ? end : p;
    return v;
}

/* ── Per-command handlers ──────────────────────────────────────────────────── */
typedef void (*CmdHandler)(const char *args);

static void h_ping(const char *args) { (void)args; Protocol_Reply("PONG"); }

static void h_status(const char *args)
{
    (void)args;
    Protocol_Reply("OK STATUS:uptime=%lums,ir=0x%02X,door=%s,motor=%u",
                   (unsigned long)HAL_GetTick(),
                   IrSensors_State(),
                   Reed_DoorOpen() ? "open" : "closed",
                   (unsigned)MotorMux_Current());
}

static void h_servo_pulse(const char *args)
{
    int us = atoi(args);
    if (us < 500 || us > 2500) { Protocol_Reply("Error SERVO:range"); return; }
    Servo_SetPulseUs((uint16_t)us);
    Protocol_Reply("OK SERVO:%d", us);
}

static void h_servo_angle(const char *args)
{
    int deg = atoi(args);
    if (deg < 0 || deg > 180) { Protocol_Reply("Error ANGLE:range"); return; }
    Servo_SetAngle((uint8_t)deg);
    Protocol_Reply("OK ANGLE:%d", deg);
}

static void h_motor_select(const char *args)
{
    int m = atoi(args);
    if (m < 0 || m > 8) { Protocol_Reply("Error MOTOR:range"); return; }
    if (m == 0) MotorMux_None(); else MotorMux_Select((uint8_t)m);
    Protocol_Reply("OK MOTOR:%d", m);
}

static void h_relay(const char *args)
{
    /* RELAY:<idx 1..3>:<0|1>  (or space-joined "RELAY 1 1"). */
    const char *p = args;
    int ok1 = 0, ok2 = 0;
    long idx = next_int(&p, &ok1);
    long on  = next_int(&p, &ok2);
    if (!ok1 || !ok2 || !Relay_Set((uint8_t)idx, on != 0)) {
        Protocol_Reply("Error RELAY:args");
        return;
    }
    Protocol_Reply("OK RELAY:%ld:%ld", idx, on ? 1L : 0L);
}

static void h_dispense(const char *args)
{
    /* DISPENSE:<slot 1..8>[:<min_drop_g>]
     * Queues the request — the dispense task emits START / Done / Error
     * replies asynchronously as the state machine progresses.            */
    const char *p = args;
    int ok = 0;
    long slot     = next_int(&p, &ok);
    long min_drop = 0; { int ok2 = 0; long m = next_int(&p, &ok2); if (ok2) min_drop = m; }

    if (!ok || slot < 1 || slot > 8) {
        Protocol_Reply("Error DISPENSE:slot");
        return;
    }
    if (!Dispense_Request((uint8_t)slot, (int32_t)min_drop)) {
        Protocol_Reply("Error DISPENSE:%ld:BUSY", slot);
    }
    /* On a successful queue, no immediate reply — the task posts START
     * within a few ms and "Done/Error DISPENSE:…" when it finishes.       */
}

static void h_step(const char *args)
{
    /* STEP:<count>:<dir>[:<motor>] — if motor is supplied, route via mux.   */
    const char *p = args;
    int ok = 0;
    long steps = next_int(&p, &ok);
    long dir   = next_int(&p, NULL);
    int okm = 0;
    long motor = next_int(&p, &okm);

    if (!ok || steps <= 0 || steps > 1000000L) {
        Protocol_Reply("Error STEP:bad_count");
        return;
    }
    /* Hand the motor to the task — it selects the 595 mux right before the
     * move and holds it for the whole move, so a second motor pressed mid-move
     * just queues instead of stomping the running one. */
    uint8_t m = (okm && motor >= 1 && motor <= 8) ? (uint8_t)motor : 0;
    if (!StepperTask_Submit((uint32_t)steps, (uint8_t)(dir ? 1 : 0), m)) {
        Protocol_Reply("Error STEP:queue_full");
    }
    /* On success the stepper task emits "Done STEP" when motion completes,
     * then de-energises the driver + opens the mux.                       */
}

static void h_weigh(const char *args)
{
    /* WEIGH[:<cell 0..7>] — defaults to cell 0. Reports raw 24-bit value;
     * grams conversion belongs on the Pi side where per-cell calibration
     * tables live.                                                          */
    int cell = (*args) ? atoi(args) : 0;
    if (cell < 0 || cell > 7) { Protocol_Reply("Error WEIGH:cell"); return; }

    int32_t all[8] = {0};
    if (!Hx711Bank_ReadAllFiltered(all)) {
        Protocol_Reply("Error WEIGH:timeout");
        return;
    }
    Protocol_Reply("Done WEIGH:%d:%ld", cell, (long)all[cell]);
}

static void h_weigh_all(const char *args)
{
    (void)args;
    int32_t cells[8] = {0};
    if (!Hx711Bank_ReadAllFiltered(cells)) {
        Protocol_Reply("Error WEIGH_ALL:timeout");
        return;
    }
    /* "Done" prefix matters — Serial_Connection on the Pi uses it to
     * resolve the pending command. Without it, every WEIGH_ALL would be
     * retried 3× and then declared timed-out.                            */
    Protocol_Reply("Done WEIGH_ALL:%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld",
                   (long)cells[0], (long)cells[1], (long)cells[2], (long)cells[3],
                   (long)cells[4], (long)cells[5], (long)cells[6], (long)cells[7]);
}

static void h_ir_state(const char *args)
{
    (void)args;
    /* Bit N = IR(N+1) asserted, 5 sensors → 0x00..0x1F. */
    Protocol_Reply("OK IR:0x%02X", IrSensors_State());
}

static void h_diag_ir(const char *args)
{
    (void)args;
    Protocol_Reply("Done DIAG_IR:raw=0x%02X,deb=0x%02X", IrSensors_ReadRaw(), IrSensors_State());
}

static void h_raw_ir(const char *args)
{
    (void)args;
    Protocol_Reply("Done RAW_IR:0x%02X", IrSensors_ReadRaw());
}

static void h_door(const char *args)
{
    (void)args;
    Protocol_Reply("OK DOOR:%s", Reed_DoorOpen() ? "open" : "closed");
}

/* ── Recycle lane MOVED to the Arduino ──────────────────────────────────────
 * RECYCLE / VERDICT / BASKETS / CONVEYOR and the EVT,* events are no longer
 * handled here — the recycle conveyor (TP6600 → NEMA23) + the 5 IR sensors now
 * live on a separate Arduino. The STM32 only does the vending steppers, relays,
 * load cells and the door reed. */

static void h_set_time(const char *args)
{
    uint32_t epoch = (uint32_t)strtoul(args, NULL, 10);
    if (epoch == 0) { Protocol_Reply("Error SET_TIME:fmt"); return; }
    if (!RtcSync_SetEpoch(epoch)) { Protocol_Reply("Error SET_TIME:rtc"); return; }
    Protocol_Reply("OK SET_TIME:%lu", (unsigned long)epoch);
}

static void h_get_time(const char *args)
{
    (void)args;
    /* Raw epoch number — the Pi parses this verbatim, so no "OK" prefix. */
    Protocol_Reply("%lu", (unsigned long)RtcSync_GetEpoch());
}

/* ── Dispatch table ────────────────────────────────────────────────────────── */
typedef struct {
    const char *name;
    CmdHandler  fn;
} CmdEntry;

static const CmdEntry s_commands[] = {
    /* Diagnostics first — they're the chatty ones. */
    { "PING",          h_ping },
    { "STATUS",        h_status },

    /* Motion / actuators */
    { "SERVO",         h_servo_pulse },
    { "ANGLE",         h_servo_angle },
    { "MOTOR",         h_motor_select },
    { "STEP",          h_step },
    { "DISPENSE",      h_dispense },
    { "RELAY",         h_relay },
    /* CONVEYOR / RECYCLE / VERDICT / BASKETS moved to the Arduino. */

    /* Sensors / inputs */
    { "WEIGH",         h_weigh },
    { "WEIGH_ALL",     h_weigh_all },
    { "IR",            h_ir_state },
    { "IR_STATE",      h_ir_state },
    { "DIAG_IR",       h_diag_ir },
    { "RAW_IR",        h_raw_ir },
    { "DOOR",          h_door },

    /* Time */
    { "SET_TIME",      h_set_time },
    { "GET_TIME",      h_get_time },
};

void Protocol_Dispatch(const char *line)
{
    if (!line || !*line) return;

    /* The command name ends at the first separator (':' or ' ') or the NUL. */
    const char *sep = line;
    while (*sep && *sep != ':' && *sep != ' ') ++sep;
    const size_t name_len = (size_t)(sep - line);
    const char  *args     = *sep ? sep + 1 : "";

    for (size_t i = 0; i < sizeof(s_commands) / sizeof(s_commands[0]); ++i) {
        const CmdEntry *e = &s_commands[i];
        if (strlen(e->name) != name_len) continue;
        if (memcmp(e->name, line, name_len) != 0) continue;
        e->fn(args);
        return;
    }

    /* "Error" prefix → the Pi resolves it as a failure immediately instead
     * of retrying 3× and flooding the link. */
    Protocol_Reply("Error unknown:%.*s", (int)name_len, line);
}
