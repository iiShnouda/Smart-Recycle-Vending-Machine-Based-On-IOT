#include "shiftreg.h"
#include "pin_map.h"

/* These come from CubeMX (main.c / spi.c). */
extern SPI_HandleTypeDef SR595_SPI;     /* hspi2 */
extern SPI_HandleTypeDef SR165_SPI;     /* hspi3 */

/* ── 74HC595 : motor-select output ──────────────────────────────────── */

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

/* ── 74HC165 : parallel input (8 HX711 DOUTs) ───────────────────────── */

void SR165_Init(void)
{
    /* PL/SH-LD idle high (shift mode); pulse low to capture inputs. */
    HAL_GPIO_WritePin(SR165_LATCH_PORT, SR165_LATCH_PIN, GPIO_PIN_SET);
}

uint8_t SR165_Read(void)
{
    uint8_t rx = 0;
    /* Capture the parallel inputs: PL low briefly, then high. */
    HAL_GPIO_WritePin(SR165_LATCH_PORT, SR165_LATCH_PIN, GPIO_PIN_RESET);
    __NOP(); __NOP();
    HAL_GPIO_WritePin(SR165_LATCH_PORT, SR165_LATCH_PIN, GPIO_PIN_SET);
    /* Clock the 8 captured bits in on SPI3 (MISO only; TX is a dummy). */
    uint8_t tx = 0xFF;
    HAL_SPI_TransmitReceive(&SR165_SPI, &tx, &rx, 1, HAL_MAX_DELAY);
    return rx;
}
