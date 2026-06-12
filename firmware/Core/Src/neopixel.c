#include "neopixel.h"
#include "pin_map.h"
#include "main.h"

/*
 * WS2812 on PB9 — software bit-bang (this board has no level-shifter IC and
 * no spare DMA-capable timer on PB9). Data path: PB9 -> R14(330 Ω) -> strip;
 * the strip runs at ~4.3 V (1N4007 in series with +5 V) so a 3.3 V high is a
 * valid "1". Bit timing comes from the DWT cycle counter at 96 MHz.
 *
 * Show() runs with interrupts off for the duration of the frame
 * (NEO_NUM × 24 × 1.25 µs). Keep the strip short and refresh only on change.
 */

#define NEO_SET()  (NEO_PORT->BSRR = (uint32_t)(1u << NEO_BIT))
#define NEO_CLR()  (NEO_PORT->BSRR = (uint32_t)(1u << (NEO_BIT + 16)))

/* Cycle counts @ 96 MHz (1 µs = 96 cyc). WS2812B bit ≈ 1.25 µs total.
 *   "0": ~0.31 µs high / ~0.94 µs low   (T0H must stay < ~0.5 µs)
 *   "1": ~0.63 µs high / ~0.63 µs low   (T1H must stay > ~0.55 µs)        */
#define T0H 30u
#define T0L 90u
#define T1H 60u
#define T1L 60u

static uint8_t s_led[NEO_NUM][3];        /* G,R,B per pixel */

static void dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

static inline void wait_cyc(uint32_t c)
{
    uint32_t start = DWT->CYCCNT;
    while ((DWT->CYCCNT - start) < c) { /* spin */ }
}

void Neo_Init(void)
{
    dwt_init();
    NEO_CLR();
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
    __disable_irq();
    for (uint16_t i = 0; i < NEO_NUM; ++i) {
        for (int c = 0; c < 3; ++c) {
            uint8_t byte = s_led[i][c];
            for (int bit = 7; bit >= 0; --bit) {
                if (byte & (1u << bit)) {
                    NEO_SET(); wait_cyc(T1H);
                    NEO_CLR(); wait_cyc(T1L);
                } else {
                    NEO_SET(); wait_cyc(T0H);
                    NEO_CLR(); wait_cyc(T0L);
                }
            }
        }
    }
    __enable_irq();
    NEO_CLR();           /* hold low; >50 µs before the next Show() latches */
}

void Neo_On(void)  { Neo_Fill(255, 255, 255); Neo_Show(); }   /* bright white */
void Neo_Off(void) { Neo_Fill(0, 0, 0);       Neo_Show(); }

/* The DMA path is gone (bit-bang now). Keep this symbol so a main.c that
 * still routes the PWM-DMA-complete callback here continues to link. */
void Neo_OnDmaDone(TIM_HandleTypeDef *htim) { (void)htim; }
