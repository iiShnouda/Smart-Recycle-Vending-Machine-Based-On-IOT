#include "vend.h"
#include "stepper.h"
#include "shiftreg.h"
#include "bsp.h"

static VendEmit      s_emit;
static volatile bool s_busy;
static uint8_t       s_slot;

void Vend_Init(VendEmit emit)
{
    s_emit = emit;
    s_busy = false;
    SR595_Write(0x00);                  /* all gates closed */
    BSP_TMC_Enable(false);
}

bool Vend_Dispense(uint8_t slot0)
{
    if (s_busy || slot0 >= 8) return false;
    s_slot = slot0;
    s_busy = true;

    Stepper_SelectMotor(slot0);         /* 1. open this slot's gate */
    BSP_TMC_Enable(true);               /* 2. energise the TMC      */
    Stepper_Move(DRV_TMC, VEND_STEPS_PER_REV, VEND_STEP_HZ); /* one rev */
    return true;
}

bool Vend_Busy(void) { return s_busy; }

void Vend_Poll(void)
{
    if (!s_busy) return;
    if (Stepper_Busy(DRV_TMC)) return;  /* still turning */

    /* 3. revolution done — stop, close the gate. */
    BSP_TMC_Enable(false);
    SR595_Write(0x00);                  /* deselect = gate closed */

    /* 4. report it so the Pi can dispense the next cart item. */
    if (s_emit) {
        char line[24];
        int n = 0;
        const char *p = "EVT,DISPENSED,";
        while (p[n]) { line[n] = p[n]; ++n; }
        line[n++] = (char)('0' + s_slot);
        line[n++] = '\n';
        line[n]   = 0;
        s_emit(line);
    }
    s_busy = false;
}
