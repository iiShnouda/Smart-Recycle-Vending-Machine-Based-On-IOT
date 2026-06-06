# SRVM Controller V2.1 — STM32F411 firmware

HAL/CubeIDE driver layer for the SRVM Controller Board V2.1 (Black Pill,
STM32F411CEU6). Drop these into your CubeIDE project's `Core/Inc` and
`Core/Src`, then wire the four hooks below.

## ⚠️ Fix these in CubeMX first (vs the pinout you shared)
- **PB2 → `GPIO_Output`** (TP6600 DIR; you had it as Input — a direction
  line must drive).
- **PB3/PB4/PB5** for the 74HC165: hardware SPI3 is **SCK=PB3, MISO=PB4**.
  Set **PB3 = SPI3_SCK, PB4 = SPI3_MISO, PB5 = GPIO_Output** (latch).
- **TIM2 / TIM3** prescaler so the counter ticks at **1 MHz** (1 µs) — the
  stepper speed math assumes this.
- **TIM4** (servo): 1 MHz tick, ARR = 19999 (20 ms / 50 Hz frame).
- Enable **SPI2** (master, 8-bit, the 595) and **SPI3** (master, RX, the 165),
  **USART2** (TMC), **USB_DEVICE / CDC** (link to the Pi). *(INA219 removed in
  the 2026-06-06 board rev — **I2C1 is no longer used** and can be disabled.)*

## What's implemented (covers every pin)
| File | Pins / peripheral |
|---|---|
| `pin_map.h` | **every** MCU pin, from the netlist |
| `bsp.c` | relays ×3, IR ×5, reed, TMC EN/DIR/DIAG/INDEX, TP6600 EN/DIR |
| `shiftreg.c` | 74HC595 motor-mux (SPI2) + 74HC165 read (SPI3) |
| `hx711_bank.c` | parallel 8-channel load-cell read via the 165 |
| `stepper.c` | TMC (TIM2, muxed) + TP6600 (TIM3) step generation |
| `servo.c` | door servo (TIM4_CH3) |
| `app.c` | main loop + USB-CDC command protocol to the Pi |

## Four hooks to wire in your `main.c` / usb glue
```c
/* main.c */
App_Init();                 /* after MX_*_Init() */
while (1) { App_Task(); }

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim) {
    Stepper_OnPulse(htim);  /* counts emitted steps */
}

/* usbd_cdc_if.c — CDC_Receive_FS() */
App_OnRx(Buf, *Len);

/* provide the transmit side */
void App_Send(const char *s){ CDC_Transmit_FS((uint8_t*)s, strlen(s)); }
```

## Wire the NeoPixel DMA-complete too
```c
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim) {
    Stepper_OnPulse(htim);     /* TIM2 / TIM3 step counting */
    Neo_OnDmaDone(htim);       /* TIM1 WS2812 DMA done       */
}
```

## Command protocol (USB-CDC, newline-terminated)
**Pi → board:** `PING` · `WEIGH` · `IR` · `DOOR` · `RELAY n 0|1` ·
`SERVO deg` · `DISPENSE motor0_7 steps [hz]` · `AUGER steps [hz]` · `STOP` ·
`RECYCLE 0|1` · `BASKETS bottleFull canFull` · `VERDICT BOTTLE|CAN|REJECT`

**board → Pi (events):** `BOOT,...` · `EVT,DOOR,0|1` ·
`EVT,READY` · `EVT,ENTRY` · `EVT,BELT` · `EVT,CAMERA` ·
`EVT,ACCEPT,BOTTLE|CAN` · `EVT,DROPPED,BOTTLE|CAN` ·
`EVT,REJECT,<why>` · `EVT,REJECTED,<why>`

## Recycle lane flow (recycle.c)
1. Pi: user picks Recycle → `RECYCLE 1` (+ `BASKETS` fullness).
2. IR1 entry → board lights NeoPixel, runs belt → `EVT,ENTRY`.
3. IR2 → `EVT,BELT`; IR3 (camera) → belt stops, `EVT,CAMERA`.
4. Pi classifies, replies `VERDICT BOTTLE|CAN|REJECT`.
5. Accept → servo tilts to that basket, belt feeds, IR4/IR5 confirms
   `EVT,DROPPED,...`. Reject (or full basket) → belt reverses to IR1,
   `EVT,REJECTED,<why>`.
6. Pi tallies, awards points → `RECYCLE 0` (NeoPixel + camera off).

## Still optional
- **`tmc2209.c`** — USART2 single-wire config (microsteps/current/
  StealthChop). The mux + STEP/DIR already work without it.
