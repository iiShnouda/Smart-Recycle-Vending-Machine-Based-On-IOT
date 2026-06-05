#include "neopixel.h"
#include "pin_map.h"

extern TIM_HandleTypeDef NEO_TIM;       /* htim1 */

/* Duty values for a WS2812 "0" and "1" bit, assuming ARR = 124:
 *   T0H ≈ 0.32 us -> ~40 ticks,  T1H ≈ 0.64 us -> ~80 ticks. */
#define WS_LO        40
#define WS_HI        80
#define RESET_SLOTS  48             /* >50 us of low to latch */

static uint8_t  s_led[NEO_NUM][3];                  /* G,R,B per pixel */
static uint16_t s_dma[NEO_NUM * 24 + RESET_SLOTS];  /* PWM duties      */
static volatile uint8_t s_busy;

void Neo_Init(void)
{
    Neo_Off();
}

void Neo_SetPixel(uint16_t i, uint8_t r, uint8_t g, uint8_t b)
{
    if (i >= NEO_NUM) return;
    s_led[i][0] = g; s_led[i][1] = r; s_led[i][2] = b;   /* WS2812 = GRB */
}

void Neo_Fill(uint8_t r, uint8_t g, uint8_t b)
{
    for (uint16_t i = 0; i < NEO_NUM; ++i) Neo_SetPixel(i, r, g, b);
}

void Neo_Show(void)
{
    if (s_busy) return;
    uint32_t k = 0;
    for (uint16_t i = 0; i < NEO_NUM; ++i)
        for (int c = 0; c < 3; ++c)
            for (int bit = 7; bit >= 0; --bit)
                s_dma[k++] = (s_led[i][c] & (1u << bit)) ? WS_HI : WS_LO;
    for (int z = 0; z < RESET_SLOTS; ++z) s_dma[k++] = 0;

    s_busy = 1;
    HAL_TIM_PWM_Start_DMA(&NEO_TIM, NEO_TIM_CH, (uint32_t *)s_dma, k);
}

/* Wire this from HAL_TIM_PWM_PulseFinishedCallback (DMA-complete) in main.c. */
void Neo_OnDmaDone(TIM_HandleTypeDef *htim)
{
    if (htim == &NEO_TIM) {
        HAL_TIM_PWM_Stop_DMA(&NEO_TIM, NEO_TIM_CH);
        s_busy = 0;
    }
}

void Neo_On(void)  { Neo_Fill(255, 255, 255); Neo_Show(); }   /* bright white */
void Neo_Off(void) { Neo_Fill(0, 0, 0);       Neo_Show(); }
