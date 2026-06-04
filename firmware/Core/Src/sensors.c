#include "sensors.h"
#include "bsp.h"
#include "pin_map.h"        /* IR_COUNT */

#define DEBOUNCE_N  3        /* consecutive equal polls to accept a change */

static uint8_t s_ir_stable;              /* debounced IR mask */
static uint8_t s_ir_cnt[IR_COUNT];       /* per-bit disagreement counter */
static uint8_t s_ir_inserted;            /* latched rising edges */

static bool    s_door_stable;
static uint8_t s_door_cnt;
static bool    s_door_event;
static bool    s_door_event_val;

void Sensors_Init(void)
{
    s_ir_stable   = BSP_IR_Mask();
    s_door_stable = BSP_DoorClosed();
    s_ir_inserted = 0;
    s_door_event  = false;
    for (int i = 0; i < IR_COUNT; ++i) s_ir_cnt[i] = 0;
    s_door_cnt = 0;
}

void Sensors_Poll(void)
{
    /* ── IR sensors ── */
    uint8_t raw = BSP_IR_Mask();
    for (int i = 0; i < IR_COUNT; ++i) {
        bool now = (raw >> i) & 1u;
        bool st  = (s_ir_stable >> i) & 1u;
        if (now != st) {
            if (++s_ir_cnt[i] >= DEBOUNCE_N) {
                if (now) { s_ir_stable |= (uint8_t)(1u << i);
                           s_ir_inserted |= (uint8_t)(1u << i); }   /* rising */
                else     { s_ir_stable &= (uint8_t)~(1u << i); }
                s_ir_cnt[i] = 0;
            }
        } else {
            s_ir_cnt[i] = 0;
        }
    }

    /* ── Door reed ── */
    bool d = BSP_DoorClosed();
    if (d != s_door_stable) {
        if (++s_door_cnt >= DEBOUNCE_N) {
            s_door_stable    = d;
            s_door_cnt       = 0;
            s_door_event     = true;
            s_door_event_val = d;
        }
    } else {
        s_door_cnt = 0;
    }
}

bool Sensors_Slot(uint8_t idx1)
{
    return (idx1 >= 1 && idx1 <= IR_COUNT) ? ((s_ir_stable >> (idx1 - 1)) & 1u) : false;
}

uint8_t Sensors_SlotMask(void) { return s_ir_stable; }

bool Sensors_SlotInserted(uint8_t idx1)
{
    if (idx1 < 1 || idx1 > IR_COUNT) return false;
    uint8_t m = (uint8_t)(1u << (idx1 - 1));
    if (s_ir_inserted & m) { s_ir_inserted &= (uint8_t)~m; return true; }
    return false;
}

bool Sensors_DoorClosed(void) { return s_door_stable; }

bool Sensors_DoorChanged(bool *closed)
{
    if (s_door_event) {
        s_door_event = false;
        if (closed) *closed = s_door_event_val;
        return true;
    }
    return false;
}
