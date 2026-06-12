/*
 * servo.c — door/flap servo on PB6 (TIM4_CH1).
 *
 * Standard hobby servo: 50 Hz frame, 1.0–2.0 ms pulse. TIM4 ticks at 1 MHz
 * (PSC 95 @ 96 MHz) with ARR = 19999 (20 ms frame), so CCR in microseconds
 * = pulse width. The channel comes from SERVO_TIM_CH in pin_map.h.
 */
#include "main.h"
#include "pin_map.h"

extern TIM_HandleTypeDef SERVO_TIM;     /* htim4 */

void Servo_Init(void)
{
    /* Start silent (CCR=0) so the servo doesn't twitch at boot. */
    __HAL_TIM_SET_COMPARE(&SERVO_TIM, SERVO_TIM_CH, 0);
    HAL_TIM_PWM_Start(&SERVO_TIM, SERVO_TIM_CH);
}

/* angle 0..180 -> 1000..2000 us */
void Servo_SetAngle(uint8_t deg)
{
    if (deg > 180) deg = 180;
    uint32_t us = 1000u + (uint32_t)deg * 1000u / 180u;
    __HAL_TIM_SET_COMPARE(&SERVO_TIM, SERVO_TIM_CH, us);
}

void Servo_Release(void)         /* stop holding (no pulse) */
{
    __HAL_TIM_SET_COMPARE(&SERVO_TIM, SERVO_TIM_CH, 0);
}
