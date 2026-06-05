/*
 * recycle.h — recycle-lane state machine (the STM32 "hands").
 *
 * Physical flow the board sequences, with the Pi as the camera "brain":
 *   IR1 entry  -> camera + NeoPixel on, belt forward
 *   IR2 on belt-> keep feeding
 *   IR3 camera -> stop belt, ask the Pi to classify  (EVT,CAMERA)
 *   Pi replies VERDICT bottle/can/reject:
 *     accept bottle -> servo tilt left,  belt fwd -> IR4 drop -> centre
 *     accept can    -> servo tilt right, belt fwd -> IR5 drop -> centre
 *     reject        -> belt reverse until IR1 -> spit back out
 *   Full baskets: both full -> refuse entry; one full -> that type is
 *   auto-rejected at verdict time.
 *
 * The Pi arms/disarms the lane (only when the user picked Recycle), feeds
 * basket-full status, and sends the camera verdict. Everything is reported
 * back as EVT,... lines so the Qt counter window can tally live.
 */
#ifndef RECYCLE_H
#define RECYCLE_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    RC_OFF = 0, RC_IDLE, RC_ENTERING, RC_ON_BELT, RC_AT_CAMERA,
    RC_TILT_BOTTLE, RC_TILT_CAN, RC_REJECTING
} RecycleState;

typedef enum { V_NONE = 0, V_BOTTLE, V_CAN, V_REJECT } Verdict;
typedef enum { REJ_NONE = 0, REJ_BOTTLE_FULL, REJ_CAN_FULL, REJ_NOT_RECYCLABLE } RejectReason;

typedef void (*RecycleEmit)(const char *line);

void         Recycle_Init(RecycleEmit emit);
void         Recycle_Arm(bool on);                 /* user entered/left Recycle */
void         Recycle_SetBaskets(bool bottleFull, bool canFull);
void         Recycle_Verdict(Verdict v, RejectReason reason);   /* from the Pi */
void         Recycle_Poll(void);                   /* tick, after Sensors_Poll() */
RecycleState Recycle_State(void);

#endif /* RECYCLE_H */
