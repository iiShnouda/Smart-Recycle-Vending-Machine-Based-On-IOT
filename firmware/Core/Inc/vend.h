/*
 * vend.h — vending dispense sequence (TMC2209 + 74HC595 motor mux).
 *
 * One dispense = one slot:
 *   1. open the slot's gate  (select it on the 595 -> MOSFETs route the
 *      TMC coils to that motor)
 *   2. enable the TMC, rotate one full revolution
 *   3. stop the TMC, close the gate (deselect on the 595)
 *   4. emit  EVT,DISPENSED,<slot>
 *
 * The Pi dispenses a multi-item cart by sending DISPENSE <slot> one at a
 * time, waiting for EVT,DISPENSED before the next — so the gate fully
 * opens/closes between products, exactly as specced.
 */
#ifndef VEND_H
#define VEND_H

#include <stdint.h>
#include <stdbool.h>

typedef void (*VendEmit)(const char *line);

#ifndef VEND_STEPS_PER_REV
#define VEND_STEPS_PER_REV 1600     /* 200 full steps × 8 microsteps */
#endif
#ifndef VEND_STEP_HZ
#define VEND_STEP_HZ       2000
#endif

void Vend_Init(VendEmit emit);
bool Vend_Dispense(uint8_t slot0);  /* 0..7; false if busy */
bool Vend_Busy(void);
void Vend_Poll(void);

#endif /* VEND_H */
