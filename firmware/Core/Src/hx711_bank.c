#include "hx711_bank.h"
#include "shiftreg.h"

/* Short, interrupt-safe busy wait. The HX711 needs SCK high < 60 us or
 * it powers down; one 165 read is ~1-2 us so we're well inside that. */
static inline void sck_high(void){ HAL_GPIO_WritePin(HX711_SCK_PORT, HX711_SCK_PIN, GPIO_PIN_SET); }
static inline void sck_low (void){ HAL_GPIO_WritePin(HX711_SCK_PORT, HX711_SCK_PIN, GPIO_PIN_RESET); }

bool HX711_Bank_Ready(void)
{
    /* DOUT is low when a cell is ready; all-ready => 165 byte == 0. */
    return SR165_Read() == 0x00;
}

int HX711_Bank_Read(int32_t out[HX711_COUNT], uint32_t timeout_ms)
{
    for (int i = 0; i < HX711_COUNT; ++i) out[i] = 0;

    /* 1. Wait for all eight DOUTs to go low (data ready). */
    uint32_t t0 = HAL_GetTick();
    while (!HX711_Bank_Ready()) {
        if (HAL_GetTick() - t0 > timeout_ms) return 0;
    }

    /* 2. Clock 24 bits, MSB first. Each SCK pulse advances all cells;
     *    read the 165 once per pulse to grab all 8 DOUTs simultaneously. */
    uint32_t acc[HX711_COUNT] = {0};
    __disable_irq();                      /* keep the 24-pulse train tight */
    for (int bit = 0; bit < 24; ++bit) {
        sck_high();
        uint8_t s = SR165_Read();         /* bit i = DOUT of cell i */
        sck_low();
        for (int c = 0; c < HX711_COUNT; ++c)
            acc[c] = (acc[c] << 1) | ((s >> c) & 0x1u);
    }
    /* 3. 25th pulse: gain 128, channel A for the next conversion. */
    sck_high(); (void)SR165_Read(); sck_low();
    __enable_irq();

    /* 4. Sign-extend each 24-bit two's-complement value to int32. */
    for (int c = 0; c < HX711_COUNT; ++c) {
        int32_t v = (int32_t)acc[c];
        if (v & 0x00800000) v |= ~0x00FFFFFF;   /* set bits 24..31 */
        out[c] = v;
    }
    return HX711_COUNT;
}
