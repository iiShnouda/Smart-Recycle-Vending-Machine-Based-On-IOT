/*
 * neopixel.h — WS2812 strip on PA8 (TIM1_CH1 + DMA), via the 74HCT125
 * level shifter (3.3 V -> 5 V). Used to flood the inspection area with
 * white light while the camera classifies an item.
 *
 * CubeMX: TIM1 CH1 PWM, ARR = 124 (1.25 µs bit @ 800 kHz with a 100 MHz
 * timer clock), and enable the DMA request for TIM1_CH1 (memory->periph,
 * half-word, normal mode).
 */
#ifndef NEOPIXEL_H
#define NEOPIXEL_H

#include <stdint.h>

#ifndef NEO_NUM
#define NEO_NUM 30                 /* LEDs on the strip/ring */
#endif

void Neo_Init(void);
void Neo_Fill(uint8_t r, uint8_t g, uint8_t b);
void Neo_SetPixel(uint16_t i, uint8_t r, uint8_t g, uint8_t b);
void Neo_Show(void);               /* push the buffer out via DMA */
void Neo_On(void);                 /* solid white (camera lighting) */
void Neo_Off(void);                /* all off */
void Neo_OnDmaDone(TIM_HandleTypeDef *htim);   /* call from PWM DMA-complete */

#endif /* NEOPIXEL_H */
