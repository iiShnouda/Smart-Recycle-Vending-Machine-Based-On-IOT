#!/usr/bin/env python3
# Spin a stepper via TMC2209 forever, flipping CW <-> CCW each revolution.

import RPi.GPIO as GPIO
import signal
import sys
import time

STEP_PIN = 20   # BCM 20, physical pin 38  -> TMC2209 STEP
DIR_PIN  = 21   # BCM 21, physical pin 40  -> TMC2209 DIR
EN_PIN   = 16   # BCM 16, physical pin 36  -> TMC2209 EN (active LOW)

MICROSTEPS    = 8                    # TMC2209 default w/ MS1=MS2=GND
STEPS_PER_REV = 200 * MICROSTEPS     # 200 full steps * microstepping
PULSE_DELAY   = 0.0005               # ~1 kHz step rate


def shutdown(*_):
    GPIO.output(EN_PIN, GPIO.HIGH)   # disable driver, motor goes free
    GPIO.cleanup()
    sys.exit(0)


signal.signal(signal.SIGTERM, shutdown)
signal.signal(signal.SIGINT,  shutdown)

GPIO.setwarnings(False)
GPIO.setmode(GPIO.BCM)
GPIO.setup(STEP_PIN, GPIO.OUT, initial=GPIO.LOW)
GPIO.setup(DIR_PIN,  GPIO.OUT, initial=GPIO.LOW)
GPIO.setup(EN_PIN,   GPIO.OUT, initial=GPIO.HIGH)

GPIO.output(EN_PIN, GPIO.LOW)        # enable driver
time.sleep(0.05)

direction = GPIO.HIGH
while True:
    GPIO.output(DIR_PIN, direction)
    time.sleep(0.001)                # DIR settle time before stepping
    for _ in range(STEPS_PER_REV):
        GPIO.output(STEP_PIN, GPIO.HIGH)
        time.sleep(PULSE_DELAY)
        GPIO.output(STEP_PIN, GPIO.LOW)
        time.sleep(PULSE_DELAY)
    direction = GPIO.LOW if direction == GPIO.HIGH else GPIO.HIGH
