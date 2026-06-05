#include "recycle.h"
#include "sensors.h"
#include "stepper.h"
#include "neopixel.h"

void Servo_SetAngle(uint8_t deg);
void Servo_Release(void);

/* ── Tunables ────────────────────────────────────────────────────────── */
#define SERVO_CENTRE   90
#define SERVO_BOTTLE   30      /* tilt toward the bottle basket (left)  */
#define SERVO_CAN     150      /* tilt toward the can basket (right)    */
#define BELT_HZ      1500      /* TP6600 belt step rate                 */
#define MOVE_TIMEOUT_MS 8000   /* safety: abort a stuck stage           */

/* IR roles (1-based): 1 entry, 2 belt-start, 3 camera, 4 bottle, 5 can */

static RecycleEmit   s_emit;
static RecycleState  s_st;
static bool          s_bottleFull, s_canFull;
static Verdict       s_pendV;
static RejectReason  s_pendR, s_rejReason;
static uint32_t      s_t;             /* stage start tick */

static void emit(const char *l){ if (s_emit) s_emit(l); }
static void belt_fwd(void){ Stepper_Jog(DRV_TP, true,  BELT_HZ); }
static void belt_rev(void){ Stepper_Jog(DRV_TP, false, BELT_HZ); }
static void belt_stop(void){ Stepper_Abort(DRV_TP); }
static void stage(RecycleState s){ s_st = s; s_t = HAL_GetTick(); }

void Recycle_Init(RecycleEmit emit_cb)
{
    s_emit = emit_cb;
    s_st = RC_OFF;
    s_bottleFull = s_canFull = false;
    s_pendV = V_NONE; s_pendR = REJ_NONE;
    Servo_SetAngle(SERVO_CENTRE);
}

void Recycle_Arm(bool on)
{
    if (on) {
        Servo_SetAngle(SERVO_CENTRE);
        belt_stop();
        s_pendV = V_NONE;
        stage(RC_IDLE);
        emit("EVT,READY\n");
    } else {
        belt_stop();
        Neo_Off();
        Servo_SetAngle(SERVO_CENTRE);
        stage(RC_OFF);
    }
}

void Recycle_SetBaskets(bool bottleFull, bool canFull)
{
    s_bottleFull = bottleFull;
    s_canFull    = canFull;
}

void Recycle_Verdict(Verdict v, RejectReason reason)
{
    s_pendV = v;
    s_pendR = reason;
}

static void begin_reject(RejectReason r)
{
    s_rejReason = r;
    belt_rev();
    stage(RC_REJECTING);
}

RecycleState Recycle_State(void){ return s_st; }

void Recycle_Poll(void)
{
    switch (s_st) {

    case RC_OFF:
        break;

    case RC_IDLE:
        if (Sensors_SlotInserted(1)) {              /* something entered */
            emit("EVT,ENTRY\n");
            Neo_On();                               /* light the camera area */
            if (s_bottleFull && s_canFull) {        /* nowhere to put it */
                emit("EVT,REJECT,BOTH_FULL\n");
                begin_reject(REJ_NOT_RECYCLABLE);   /* push straight back */
            } else {
                belt_fwd();
                stage(RC_ENTERING);
            }
        }
        break;

    case RC_ENTERING:
        if (Sensors_Slot(2)) { emit("EVT,BELT\n"); stage(RC_ON_BELT); }
        else if (HAL_GetTick() - s_t > MOVE_TIMEOUT_MS) begin_reject(REJ_NOT_RECYCLABLE);
        break;

    case RC_ON_BELT:
        if (Sensors_Slot(3)) {                      /* reached the camera */
            belt_stop();
            s_pendV = V_NONE;
            emit("EVT,CAMERA\n");                   /* Pi: classify now */
            stage(RC_AT_CAMERA);
        } else if (HAL_GetTick() - s_t > MOVE_TIMEOUT_MS) begin_reject(REJ_NOT_RECYCLABLE);
        break;

    case RC_AT_CAMERA:
        if (s_pendV == V_BOTTLE) {
            s_pendV = V_NONE;
            if (s_bottleFull) { emit("EVT,REJECT,BOTTLE_FULL\n"); begin_reject(REJ_BOTTLE_FULL); }
            else { emit("EVT,ACCEPT,BOTTLE\n"); Servo_SetAngle(SERVO_BOTTLE); belt_fwd(); stage(RC_TILT_BOTTLE); }
        } else if (s_pendV == V_CAN) {
            s_pendV = V_NONE;
            if (s_canFull) { emit("EVT,REJECT,CAN_FULL\n"); begin_reject(REJ_CAN_FULL); }
            else { emit("EVT,ACCEPT,CAN\n"); Servo_SetAngle(SERVO_CAN); belt_fwd(); stage(RC_TILT_CAN); }
        } else if (s_pendV == V_REJECT) {
            s_pendV = V_NONE;
            emit("EVT,REJECT,NOT_RECYCLABLE\n");
            begin_reject(REJ_NOT_RECYCLABLE);
        }
        /* else: keep waiting for the Pi's verdict (no camera timeout —
         * classification can take a moment). */
        break;

    case RC_TILT_BOTTLE:
        if (Sensors_SlotInserted(4)) {
            emit("EVT,DROPPED,BOTTLE\n");
            belt_stop(); Servo_SetAngle(SERVO_CENTRE);
            stage(RC_IDLE);
        } else if (HAL_GetTick() - s_t > MOVE_TIMEOUT_MS) { belt_stop(); Servo_SetAngle(SERVO_CENTRE); stage(RC_IDLE); }
        break;

    case RC_TILT_CAN:
        if (Sensors_SlotInserted(5)) {
            emit("EVT,DROPPED,CAN\n");
            belt_stop(); Servo_SetAngle(SERVO_CENTRE);
            stage(RC_IDLE);
        } else if (HAL_GetTick() - s_t > MOVE_TIMEOUT_MS) { belt_stop(); Servo_SetAngle(SERVO_CENTRE); stage(RC_IDLE); }
        break;

    case RC_REJECTING:
        /* run the belt backwards until the item is back at the entry. */
        if (Sensors_SlotInserted(1) || Sensors_Slot(1)) {
            belt_stop(); Servo_SetAngle(SERVO_CENTRE);
            const char *why = (s_rejReason == REJ_BOTTLE_FULL) ? "EVT,REJECTED,BOTTLE_FULL\n"
                            : (s_rejReason == REJ_CAN_FULL)    ? "EVT,REJECTED,CAN_FULL\n"
                                                               : "EVT,REJECTED,NOT_RECYCLABLE\n";
            emit(why);
            stage(RC_IDLE);
        } else if (HAL_GetTick() - s_t > MOVE_TIMEOUT_MS) {
            belt_stop(); emit("EVT,REJECTED,NOT_RECYCLABLE\n"); stage(RC_IDLE);
        }
        break;
    }
}
