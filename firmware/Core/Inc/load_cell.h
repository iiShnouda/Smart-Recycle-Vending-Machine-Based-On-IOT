/*
 * load_cell.h — weight layer over the 8-channel HX711 bank.
 *
 * Per cell: a tare offset (raw counts at zero load) and a scale factor
 * (counts per gram, from a known-mass calibration). grams = (raw - offset)
 * / scale. Persist offsets+scales on the Pi side and push them back at
 * boot, or call the tare/calibrate helpers here.
 */
#ifndef LOAD_CELL_H
#define LOAD_CELL_H

#include <stdint.h>
#include <stdbool.h>
#include "pin_map.h"      /* HX711_COUNT */

void    LoadCell_Init(void);

/* Capture the zero offset (average of `samples` reads). */
void    LoadCell_Tare(uint8_t idx0, uint8_t samples);
void    LoadCell_TareAll(uint8_t samples);

/* Calibration: counts-per-gram. Either set it directly, or call
 * Calibrate() with a known mass currently on the cell to compute it. */
void    LoadCell_SetScale(uint8_t idx0, float counts_per_gram);
bool    LoadCell_Calibrate(uint8_t idx0, float known_grams, uint8_t samples);

/* Readings (each triggers one fresh bank read of all 8 cells). */
float   LoadCell_Grams(uint8_t idx0);
int     LoadCell_ReadAllGrams(float grams[HX711_COUNT]);
int32_t LoadCell_Raw(uint8_t idx0);           /* last raw count */

#endif /* LOAD_CELL_H */
