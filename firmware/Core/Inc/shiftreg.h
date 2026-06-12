/*
 * shiftreg.h — 74HC595 (motor-select OUT) + 74HC165 (HX711-data IN).
 *
 *  74HC595 (U10, SPI2, 3.3 V): one 8-bit byte = which of the 8 motors is
 *    selected (each bit drives a 2N2222A). Bit n (0..7) high -> motor n+1.
 *
 *  74HC165 (U9, 5 V, GPIO bit-bang): one 8-bit byte = the 8 HX711 DOUT
 *    levels, sampled simultaneously. CLK/LATCH are open-drain pulled up to
 *    +5 V; read once per HX711 clock pulse for a parallel 8-channel read.
 */
#ifndef SHIFTREG_H
#define SHIFTREG_H

#include <stdint.h>

void    SR595_Init(void);                 /* deselect all motors */
void    SR595_Write(uint8_t bits);        /* latch a select byte  */
void    SR595_SelectMotor(uint8_t idx0);  /* idx0 = 0..7, one-hot  */

void    SR165_Init(void);
uint8_t SR165_Read(void);                 /* latch + shift in 8 bits */

#endif /* SHIFTREG_H */
