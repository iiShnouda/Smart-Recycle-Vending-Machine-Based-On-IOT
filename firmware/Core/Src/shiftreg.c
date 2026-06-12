#include "shiftreg.h"
#include "pin_map.h"

/* 74HC595 still uses hardware SPI2. The 74HC165 is now GPIO bit-banged
 * (no SPI3 on this board) so its clock/latch can be open-drain + pulled to
 * +5 V — the level-shift trick that lets a 3.3 V MCU clock a 5 V part. */
extern SPI_HandleTypeDef SR595_SPI;     /* hspi2 */

/* Settle delay for the open-drain 5 V lines: WritePin(SET) only *releases*
 * the pin, and the 4.7 k pull-up's RC takes ~150 ns to reach a valid high.
 * ~16 NOPs @ 96 MHz ≈ 0.17 µs per half-edge — comfortably clear of that. */
static inline void sr_delay(void)
{
    for (volatile int i = 0; i < 16; ++i) __NOP();
}

/* ── 74HC595 : motor-select output (SPI2, 3.3 V) ────────────────────── */

void SR595_Init(void)
{
    SR595_Write(0x00);                  /* all motors deselected */
}

void SR595_Write(uint8_t bits)
{
    /* RCLK low, clock the byte out on SPI2, then pulse RCLK to latch. */
    HAL_GPIO_WritePin(SR595_LATCH_PORT, SR595_LATCH_PIN, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&SR595_SPI, &bits, 1, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(SR595_LATCH_PORT, SR595_LATCH_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(SR595_LATCH_PORT, SR595_LATCH_PIN, GPIO_PIN_RESET);
}

void SR595_SelectMotor(uint8_t idx0)
{
    SR595_Write(idx0 < 8 ? (uint8_t)(1u << idx0) : 0x00);
}

/* ── 74HC165 : 8 HX711 DOUTs, GPIO bit-bang (open-drain, 5 V) ────────── */
/* Open-drain semantics: WritePin(SET) releases (pull-up -> 5 V),
 * WritePin(RESET) drives low. Reads exactly like the old SPI3 path:
 * MSB first, one bit per CLK rising edge. */

void SR165_Init(void)
{
    HAL_GPIO_WritePin(SR165_CLK_PORT,   SR165_CLK_PIN,   GPIO_PIN_SET);  /* CLK idle high */
    HAL_GPIO_WritePin(SR165_LATCH_PORT, SR165_LATCH_PIN, GPIO_PIN_SET);  /* shift mode    */
}

uint8_t SR165_Read(void)
{
    uint8_t rx = 0;

    /* Capture the 8 parallel inputs: PL/SH-LD low briefly, then high. */
    HAL_GPIO_WritePin(SR165_LATCH_PORT, SR165_LATCH_PIN, GPIO_PIN_RESET);
    sr_delay();
    HAL_GPIO_WritePin(SR165_LATCH_PORT, SR165_LATCH_PIN, GPIO_PIN_SET);
    sr_delay();

    /* QH already holds the first (MSB) bit. Read, then clock to shift. */
    for (int i = 0; i < 8; ++i) {
        rx <<= 1;
        if (HAL_GPIO_ReadPin(SR165_MISO_PORT, SR165_MISO_PIN) == GPIO_PIN_SET)
            rx |= 1u;
        HAL_GPIO_WritePin(SR165_CLK_PORT, SR165_CLK_PIN, GPIO_PIN_RESET);
        sr_delay();
        HAL_GPIO_WritePin(SR165_CLK_PORT, SR165_CLK_PIN, GPIO_PIN_SET);  /* rising edge shifts */
        sr_delay();
    }
    return rx;
}
