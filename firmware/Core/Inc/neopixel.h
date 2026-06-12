/*
 * neopixel.h — WS2812 strip on PB9, software bit-bang (this board has no
 * level-shifter IC). Used to flood the inspection area with white light
 * while the camera classifies an item. See neopixel.c for the timing; PB9
 * must be a GPIO push-pull output at very-high speed (set in CubeMX).
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
