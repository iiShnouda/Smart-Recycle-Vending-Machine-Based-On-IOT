/*
 * pin_map.h — SRVM Controller Board V2  (STM32F411CEU6, Black Pill V3)
 *
 * Single source of truth for every MCU pin, reconciled against the
 * 2026-06-11 netlist (Netlist_SRVM_controller_Schematic_2026-06-11) and the
 * CubeMX test.ioc. Use these macros everywhere instead of bare GPIOx /
 * GPIO_PIN_n so a future board revision is a one-file change.
 *
 * Clock: HSE 25 MHz crystal -> PLL -> 96 MHz core, 48 MHz USB (CSS on).
 *
 * Removed vs older revs: INA219 (no I2C to the MCU) and the RCWL motion
 * sensor — no current/power telemetry, no motion input.
 *
 * Driver topology: the 8 motor-selects and the 3 relays are 2N2222A-driven
 * (NOT SCRs), with 1N4007 flyback diodes across the relay coils.
 */
#ifndef PIN_MAP_H
#define PIN_MAP_H

#include "main.h"          /* pulls in stm32f4xx_hal.h + GPIO defines */

/* ─── TMC2209 (DRV1) — the muxed stepper ──────────────────────────────── */
#define TMC_DIR_PORT     GPIOA
#define TMC_DIR_PIN      GPIO_PIN_4      /* PA4  GPIO out                  */
#define TMC_STEP_PORT    GPIOA
#define TMC_STEP_PIN     GPIO_PIN_5      /* PA5  TIM2_CH1 (PWM step)       */
#define TMC_EN_PORT      GPIOA
#define TMC_EN_PIN       GPIO_PIN_6      /* PA6  GPIO out (active-low EN)  */
#define TMC_INDEX_PORT   GPIOA
#define TMC_INDEX_PIN    GPIO_PIN_7      /* PA7  GPIO in  (index pulse)    */
#define TMC_DIAG_PORT    GPIOB
#define TMC_DIAG_PIN     GPIO_PIN_0      /* PB0  GPIO in  (stall/diag)     */
/* TMC UART = USART2 (PA2), single-wire half-duplex bridged by R17 (1k).   */
#define TMC_UART         huart2

/* ─── TP6600 (DRV2) — standalone dispense/auger axis ──────────────────── */
/* Opto-coupled inputs: EN/DIR are open-drain (sink the opto); STEP is the
 * complementary TIM1 channel (CH3N) so the firmware sinks it the same way. */
#define TP_STEP_PORT     GPIOB
#define TP_STEP_PIN      GPIO_PIN_1      /* PB1  TIM1_CH3N (PWM step)       */
#define TP_DIR_PORT      GPIOB
#define TP_DIR_PIN       GPIO_PIN_2      /* PB2  GPIO out, open-drain       */
#define TP_EN_PORT       GPIOB
#define TP_EN_PIN        GPIO_PIN_10     /* PB10 GPIO out, open-drain       */

/* ─── 74HC595 (U10, 3.3 V) — motor-select mux, SPI2 ───────────────────── */
#define SR595_SPI        hspi2           /* PB13 SCK, PB15 MOSI            */
#define SR595_LATCH_PORT GPIOB
#define SR595_LATCH_PIN  GPIO_PIN_14     /* PB14 GPIO out (RCLK/ST_CP)     */

/* ─── 74HC165 (U9, 5 V) — reads the 8 HX711 DOUTs, GPIO BIT-BANG ───────── */
/* CLK + LATCH are open-drain with 4.7 k pull-ups to +5 V (level-shift the
 * 3.3 V MCU up to the 5 V part). MISO/QH is read on a 5 V-tolerant input.   */
#define SR165_CLK_PORT   GPIOB
#define SR165_CLK_PIN    GPIO_PIN_3      /* PB3 GPIO out, open-drain (CLK) */
#define SR165_MISO_PORT  GPIOB
#define SR165_MISO_PIN   GPIO_PIN_4      /* PB4 GPIO in  (QH, 5V-tolerant) */
#define SR165_LATCH_PORT GPIOB
#define SR165_LATCH_PIN  GPIO_PIN_5      /* PB5 GPIO out, open-drain (PL)  */

/* ─── HX711 bank — 8 cells share one SCK; data read via the 165 ───────── */
#define HX711_SCK_PORT   GPIOA
#define HX711_SCK_PIN    GPIO_PIN_15     /* PA15 GPIO out, open-drain (5V) */
#define HX711_COUNT      8

/* ─── Servo (door/flap) — PB6 TIM4_CH1, 50 Hz, CCR = pulse width in µs ── */
#define SERVO_TIM        htim4
#define SERVO_TIM_CH     TIM_CHANNEL_1

/* ─── NeoPixel (WS2812) — PB9 GPIO software bit-bang ──────────────────── */
/* No level-shifter IC: PB9 -> R14(330 Ω) -> strip DIN. Drive the strip from
 * ~4.3 V (1N4007 in series with +5 V) so a 3.3 V high is a valid "1".      */
#define NEO_PORT         GPIOB
#define NEO_PIN          GPIO_PIN_9
#define NEO_BIT          9               /* for fast GPIOB->BSRR access    */

/* ─── Relays 1..3 (SRD-12VDC, 2N2222A-driven, 1N4007 flyback) ─────────── */
#define RELAY1_PORT      GPIOA
#define RELAY1_PIN       GPIO_PIN_9      /* PA9  -> Q9  -> RELAY1 (T26 gate) */
#define RELAY2_PORT      GPIOA
#define RELAY2_PIN       GPIO_PIN_10     /* PA10 -> Q10 -> RELAY2 (T25 gate) */
#define RELAY3_PORT      GPIOB
#define RELAY3_PIN       GPIO_PIN_8      /* PB8  -> Q11 -> RELAY3 (T18 gate) */

/* ─── IR slot sensors F1..F5 (active-low DOUT, digital) ───────────────── */
#define IR1_PORT  GPIOA
#define IR1_PIN   GPIO_PIN_1             /* PA1  IR_F1 */
#define IR2_PORT  GPIOA
#define IR2_PIN   GPIO_PIN_0             /* PA0  IR_F2 */
#define IR3_PORT  GPIOC
#define IR3_PIN   GPIO_PIN_15            /* PC15 IR_F3 */
#define IR4_PORT  GPIOC
#define IR4_PIN   GPIO_PIN_14            /* PC14 IR_F4 */
#define IR5_PORT  GPIOC
#define IR5_PIN   GPIO_PIN_13            /* PC13 IR_F5 */
#define IR_COUNT  5

/* ─── Reed switch (door) — PB12 GPIO in, closed = low ─────────────────── */
#define REED_PORT GPIOB
#define REED_PIN  GPIO_PIN_12

#endif /* PIN_MAP_H */
