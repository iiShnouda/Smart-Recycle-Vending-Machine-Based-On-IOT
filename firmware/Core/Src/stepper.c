#include "stepper.h"
#include "pin_map.h"
#include "bsp.h"
#include "shiftreg.h"

extern TIM_HandleTypeDef htim2;     /* TMC step  — CH1 (PA5) */
extern TIM_HandleTypeDef htim3;     /* TP6600    — CH4 (PB1) */

/* Counter ticks at 1 MHz (see CubeMX note in stepper.h). */
#define TICK_HZ 1000000u

typedef struct {
    TIM_HandleTypeDef *htim;
    uint32_t           channel;
    volatile int32_t   remaining;   /* steps still to emit */
    volatile bool      busy;
} Axis;

static Axis s_axis[2];

void Stepper_Init(void)
{
    s_axis[DRV_TMC] = (Axis){ &htim2, TIM_CHANNEL_1, 0, false };
    s_axis[DRV_TP ] = (Axis){ &htim3, TIM_CHANNEL_4, 0, false };
    BSP_TMC_Enable(false);
    BSP_TP_Enable(false);
}

void Stepper_SelectMotor(uint8_t idx0)
{
    SR595_SelectMotor(idx0);            /* route TMC coils to motor idx0 */
}

static void set_speed(Axis *a, uint32_t hz)
{
    if (hz == 0) hz = 1;
    uint32_t arr = (TICK_HZ / hz);
    if (arr < 2) arr = 2;
    __HAL_TIM_SET_AUTORELOAD(a->htim, arr - 1);
    __HAL_TIM_SET_COMPARE(a->htim, a->channel, arr / 2);   /* 50% duty */
    __HAL_TIM_SET_COUNTER(a->htim, 0);
}

void Stepper_Move(StepperDrv d, int32_t steps, uint32_t speed_hz)
{
    Axis *a = &s_axis[d];
    if (a->busy || steps == 0) return;

    bool cw = steps > 0;
    if (d == DRV_TMC) BSP_TMC_Dir(cw); else BSP_TP_Dir(cw);
    if (d == DRV_TMC) BSP_TMC_Enable(true); else BSP_TP_Enable(true);

    a->remaining = (steps < 0) ? -steps : steps;
    a->busy      = true;
    set_speed(a, speed_hz);
    HAL_TIM_PWM_Start_IT(a->htim, a->channel);
}

bool Stepper_Busy(StepperDrv d) { return s_axis[d].busy; }

void Stepper_Abort(StepperDrv d)
{
    Axis *a = &s_axis[d];
    HAL_TIM_PWM_Stop_IT(a->htim, a->channel);
    a->remaining = 0;
    a->busy      = false;
    if (d == DRV_TMC) BSP_TMC_Enable(false); else BSP_TP_Enable(false);
}

/* One PWM period elapsed == one step pulse emitted. */
void Stepper_OnPulse(TIM_HandleTypeDef *htim)
{
    for (int d = 0; d < 2; ++d) {
        Axis *a = &s_axis[d];
        if (!a->busy || htim != a->htim) continue;
        if (a->remaining > 0) a->remaining--;
        if (a->remaining == 0) {
            HAL_TIM_PWM_Stop_IT(a->htim, a->channel);
            a->busy = false;
            /* leave the driver enabled briefly for holding torque; the
             * app can disable it. For the dispense axis we release: */
            if (d == DRV_TP) BSP_TP_Enable(false);
        }
    }
}
