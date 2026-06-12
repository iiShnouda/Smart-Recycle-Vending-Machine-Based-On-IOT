#include "vend.h"
#include "stepper.h"
#include "shiftreg.h"
#include "bsp.h"

/* Let the motor-select relay physically close before the TMC draws current,
 * so it switches cold (no contact arcing). ~15-20 ms covers an SRD relay. */
#define VEND_RELAY_SETTLE_MS 20

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

    /* 1. RELAY first: route the TMC to this slot's motor while the TMC is
     *    still off, so the relay switches cold; then wait for it to close. */
    Stepper_SelectMotor(slot0);
    HAL_Delay(VEND_RELAY_SETTLE_MS);
    /* 2. TMC on + spin one revolution. 3+4 (TMC off, gate off) in Vend_Poll. */
    BSP_TMC_Enable(true);
    Stepper_Move(DRV_TMC, VEND_STEPS_PER_REV, VEND_STEP_HZ);
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
