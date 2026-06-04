/*
 * pin_map.h — SRVM Controller Board V2.1  (STM32F411CEU6, Black Pill)
 *
 * Single source of truth for every MCU pin, derived from the V2.1
 * netlist (Netlist_SRVM_controller_Schematic_2026-06-04). Use these
 * macros everywhere instead of bare GPIOx / GPIO_PIN_n so a future
 * board revision is a one-file change.
 *
 * NOTE on CubeMX (.ioc): make sure the generated MX_GPIO_Init /
 * peripheral init matches the modes below. Two pins to fix vs the
 * pinout you shared:
 *   - PB2  must be GPIO_Output  (TP6600 DIR — you had it as Input)
 *   - PB3/PB4/PB5: SPI3 is SCK=PB3, MISO=PB4; PB5 is the 165 latch
 *     (GPIO_Output). Your config looked like SCK/MISO were on PB4/PB5.
 */
#ifndef PIN_MAP_H
#define PIN_MAP_H

#include "main.h"          /* pulls in stm32f4xx_hal.h + GPIO defines */

/* ─── Stepper: TMC2209 (DRV1) — drives the 8 muxed coils ──────────────── */
#define TMC_DIR_PORT     GPIOA
#define TMC_DIR_PIN      GPIO_PIN_4      /* PA4  GPIO out                  */
#define TMC_STEP_PORT    GPIOA
#define TMC_STEP_PIN     GPIO_PIN_5      /* PA5  TIM2_CH1 (PWM step)       */
#define TMC_EN_PORT      GPIOA
#define TMC_EN_PIN       GPIO_PIN_6      /* PA6  GPIO out (active low EN)  */
#define TMC_INDEX_PORT   GPIOA
#define TMC_INDEX_PIN    GPIO_PIN_7      /* PA7  GPIO in  (index pulse)    */
#define TMC_DIAG_PORT    GPIOB
#define TMC_DIAG_PIN     GPIO_PIN_0      /* PB0  GPIO in  (stall/diag)     */
/* TMC UART = USART2 (PA2 TX / PA3 RX), single-wire bridged by R75 (1k).  */
#define TMC_UART         huart2

/* ─── Stepper: TP6600 (DRV2) — independent driver (dispense axis) ─────── */
#define TP_STEP_PORT     GPIOB
#define TP_STEP_PIN      GPIO_PIN_1      /* PB1  TIM3_CH4 (PWM step)       */
#define TP_DIR_PORT      GPIOB
#define TP_DIR_PIN       GPIO_PIN_2      /* PB2  GPIO out  (FIX in .ioc!)  */
#define TP_EN_PORT       GPIOB
#define TP_EN_PIN        GPIO_PIN_10     /* PB10 GPIO out  (active low EN) */

/* ─── 74HC595 output shift register — motor SELECT mux (SPI2) ─────────── */
#define SR595_SPI        hspi2           /* PB13 SCK, PB15 MOSI            */
#define SR595_LATCH_PORT GPIOB
#define SR595_LATCH_PIN  GPIO_PIN_14     /* PB14 GPIO out (RCLK/ST_CP)    */

/* ─── 74HC165 input shift register — reads the 8 HX711 DOUTs (SPI3) ───── */
#define SR165_SPI        hspi3           /* PB3 SCK, PB4 MISO             */
#define SR165_LATCH_PORT GPIOB
#define SR165_LATCH_PIN  GPIO_PIN_5      /* PB5 GPIO out (PL/SH-LD, low=load) */

/* ─── HX711 load-cell bank — 8 cells share one SCK; data via the 165 ──── */
#define HX711_SCK_PORT   GPIOA
#define HX711_SCK_PIN    GPIO_PIN_15     /* PA15 GPIO out (shared clock)   */
#define HX711_COUNT      8
/* Bit position of each HX711 DOUT inside the 165 byte (U9 inputs):
 * U1..U8 DOUT -> U9.11,12,13,14,3,4,5,6  =>  D0..D7 order below.        */

/* ─── INA219 power monitor (I2C1, PB6/PB7) ───────────────────────────── */
#define INA219_I2C       hi2c1
#define INA219_ADDR      (0x40 << 1)     /* 7-bit 0x40, HAL wants <<1      */

/* ─── Servo (door / flap) — PB8 TIM4_CH3 PWM ─────────────────────────── */
#define SERVO_TIM        htim4
#define SERVO_TIM_CH     TIM_CHANNEL_3

/* ─── NeoPixel (WS2812) — PA8 TIM1_CH1 + DMA, via 74HCT125 level shift ── */
#define NEO_TIM          htim1
#define NEO_TIM_CH       TIM_CHANNEL_1

/* ─── Relays (2N2222-driven 12 V switches; were "LED strip" channels) ── */
#define RELAY1_PORT      GPIOB
#define RELAY1_PIN       GPIO_PIN_9      /* PB9  (T18)  */
#define RELAY2_PORT      GPIOA
#define RELAY2_PIN       GPIO_PIN_10     /* PA10 (T25)  */
#define RELAY3_PORT      GPIOA
#define RELAY3_PIN       GPIO_PIN_9      /* PA9  (T26)  */

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

/* ─── Reed switch (door) — PB12 GPIO in, pull-up, closed = low ────────── */
#define REED_PORT GPIOB
#define REED_PIN  GPIO_PIN_12

#endif /* PIN_MAP_H */
