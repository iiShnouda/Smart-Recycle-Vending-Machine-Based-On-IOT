/*
 * hx711_bank.h — read all 8 HX711 load cells in parallel.
 *
 * Topology (V2.1): every HX711 shares one SCK (PA15). Each HX711's DOUT
 * goes to a 74HC165 input (cell i -> 165 bit i). So one SCK pulse clocks
 * all 8 cells together, and one 165 read samples all 8 DOUT bits at once.
 * 24 SCK pulses -> a 24-bit signed reading per cell; a 25th pulse sets
 * gain 128 / channel A for the next conversion.
 */
#ifndef HX711_BANK_H
#define HX711_BANK_H

#include <stdint.h>
#include <stdbool.h>
#include "pin_map.h"

/* True when every cell's DOUT is low (all conversions ready). */
bool HX711_Bank_Ready(void);

/* Block until ready (or timeout_ms), then read all cells. Returns the
 * number of cells whose data was valid (8 on success). out[] gets the
 * signed 24-bit values sign-extended to int32. */
int  HX711_Bank_Read(int32_t out[HX711_COUNT], uint32_t timeout_ms);

#endif /* HX711_BANK_H */
