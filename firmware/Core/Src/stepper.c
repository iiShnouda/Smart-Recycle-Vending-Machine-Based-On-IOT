#include "stepper.h"
#include "pin_map.h"
#include "bsp.h"
#include "shiftreg.h"

extern TIM_HandleTypeDef htim2;     /* TMC step  — CH1  (PA5)               */
extern TIM_HandleTypeDef htim1;     /* TP6600    — CH3N (PB1, advanced tmr) */

/* Counter ticks at 1 MHz (PSC = 95 @ 96 MHz). */
#define TICK_HZ 1000000u

typedef struct {
    TIM_HandleTypeDef *htim;
    uint32_t           channel;
    bool               comp;        /* true => complementary output (CHxN) */
    volatile int32_t   remaining;   /* steps still to emit (-1 = jog)      */
    volatile bool      busy;
} Axis;

static Axis s_axis[2];

/* TIM1_CH3N is a complementary channel, so it needs the *N* PWM calls
 * (HAL_TIMEx_PWMN_*), which also assert the advanced-timer main output
 * enable (MOE). The TMC axis on TIM2_CH1 uses the ordinary PWM calls. */
static void pwm_start(Axis *a)
{
    if (a->comp) HAL_TIMEx_PWMN_Start_IT(a->htim, a->channel);
    else         HAL_TIM_PWM_Start_IT  (a->htim, a->channel);
}
static void pwm_stop(Axis *a)
{
    if (a->comp) HAL_TIMEx_PWMN_Stop_IT(a->htim, a->channel);
    else         HAL_TIM_PWM_Stop_IT  (a->htim, a->channel);
}

void Stepper_Init(void)
{
    s_axis[DRV_TMC] = (Axis){ &htim2, TIM_CHANNEL_1, false, 0, false };
    s_axis[DRV_TP ] = (Axis){ &htim1, TIM_CHANNEL_3, true,  0, false };

    /* Step counting uses each timer's capture/compare IRQ. Enable the NVIC
     * lines here so it works even if CubeMX didn't tick them in NVIC.
     * (If you DO enable "TIM2 global" + "TIM1 capture compare" in CubeMX,
     *  define STEPPER_NO_IRQ_HANDLERS to drop the handlers below.) */
    HAL_NVIC_SetPriority(TIM2_IRQn,    6, 0); HAL_NVIC_EnableIRQ(TIM2_IRQn);
    HAL_NVIC_SetPriority(TIM1_CC_IRQn, 6, 0); HAL_NVIC_EnableIRQ(TIM1_CC_IRQn);

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
    if (d == DRV_TMC) BSP_TMC_Dir(cw);    else BSP_TP_Dir(cw);
    if (d == DRV_TMC) BSP_TMC_Enable(true); else BSP_TP_Enable(true);

    a->remaining = (steps < 0) ? -steps : steps;
    a->busy      = true;
    set_speed(a, speed_hz);
    pwm_start(a);
}

void Stepper_Jog(StepperDrv d, bool forward, uint32_t speed_hz)
{
    Axis *a = &s_axis[d];
    if (a->busy) Stepper_Abort(d);
    if (d == DRV_TMC) BSP_TMC_Dir(forward); else BSP_TP_Dir(forward);
    if (d == DRV_TMC) BSP_TMC_Enable(true);  else BSP_TP_Enable(true);
    a->remaining = -1;                  /* -1 = run forever (belt mode) */
    a->busy      = true;
    set_speed(a, speed_hz);
    pwm_start(a);
}

bool Stepper_Busy(StepperDrv d) { return s_axis[d].busy; }

void Stepper_Abort(StepperDrv d)
{
    Axis *a = &s_axis[d];
    pwm_stop(a);
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
        if (a->remaining < 0) continue;        /* jog: run forever */
        if (a->remaining > 0) a->remaining--;
        if (a->remaining == 0) {
            pwm_stop(a);
            a->busy = false;
            /* TMC keeps holding torque (app disables it); the dispense
             * axis releases as soon as the move completes. */
            if (d == DRV_TP) BSP_TP_Enable(false);
        }
    }
}

/* ── Capture/compare IRQ handlers for the step timers ──────────────────
 * Provided here so step counting works without any CubeMX NVIC config.
 * HAL_TIM_IRQHandler dispatches to HAL_TIM_PWM_PulseFinishedCallback, which
 * your main.c forwards to Stepper_OnPulse(). If you enable these IRQs in
 * CubeMX instead (it generates the same handlers), define the guard. */
#ifndef STEPPER_NO_IRQ_HANDLERS
void TIM2_IRQHandler(void)    { HAL_TIM_IRQHandler(&htim2); }
void TIM1_CC_IRQHandler(void) { HAL_TIM_IRQHandler(&htim1); }
#endif
