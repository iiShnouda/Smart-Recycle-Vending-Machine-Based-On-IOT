/*
 * bsp.h — board support for the simple GPIO peripherals on V2.1:
 * relays, IR slot sensors, reed switch, and the two stepper drivers'
 * enable/direction lines. (The complex peripherals live in their own
 * modules: shiftreg, hx711_bank, tmc2209, neopixel.)
 */
#ifndef BSP_H
#define BSP_H

#include <stdint.h>
#include <stdbool.h>

void BSP_Init(void);                       /* safe initial pin states */

/* Relays (1..3). on=true energises. */
void BSP_Relay(uint8_t idx1, bool on);

/* IR slot sensors F1..F5. Returns true when an object is present.
 * The DOUT modules are active-low (low = object), so we invert. */
bool BSP_IR(uint8_t idx1);
uint8_t BSP_IR_Mask(void);                 /* bit i = sensor i+1 present */

/* Reed (door) — closed = magnet present = low. */
bool BSP_DoorClosed(void);

/* TMC2209 (DRV1) discrete lines. */
void BSP_TMC_Enable(bool en);              /* active-low EN handled here */
void BSP_TMC_Dir(bool cw);
bool BSP_TMC_Diag(void);                   /* stall/fault flag */
bool BSP_TMC_Index(void);                  /* index pulse level */

/* TP6600 (DRV2) discrete lines. */
void BSP_TP_Enable(bool en);
void BSP_TP_Dir(bool cw);

#endif /* BSP_H */
