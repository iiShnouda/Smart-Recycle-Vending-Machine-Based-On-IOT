/*
 * sensors.h — debounced digital sensors: 5 IR slot sensors + door reed.
 *
 * Call Sensors_Poll() on a steady tick (every ~5 ms). It debounces the
 * raw GPIO reads (BSP_IR_Mask / BSP_DoorClosed) and latches edge events
 * so the app can react cleanly instead of chattering on contact bounce.
 */
#ifndef SENSORS_H
#define SENSORS_H

#include <stdint.h>
#include <stdbool.h>

void    Sensors_Init(void);
void    Sensors_Poll(void);                 /* call every ~5 ms */

/* IR slot sensors (1-based, true = object present, debounced). */
bool    Sensors_Slot(uint8_t idx1);
uint8_t Sensors_SlotMask(void);             /* bit i = slot i+1 present */
/* One-shot: did slot idx1 just become occupied? clears on read. */
bool    Sensors_SlotInserted(uint8_t idx1);

/* Door reed (debounced). closed = magnet present. */
bool    Sensors_DoorClosed(void);
/* One-shot door edge: returns true once per change; *closed = new state. */
bool    Sensors_DoorChanged(bool *closed);

#endif /* SENSORS_H */
