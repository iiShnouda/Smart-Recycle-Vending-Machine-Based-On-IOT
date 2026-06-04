#include "load_cell.h"
#include "hx711_bank.h"
#include <string.h>

static int32_t s_offset[HX711_COUNT];
static float   s_scale [HX711_COUNT];     /* counts per gram */
static int32_t s_raw   [HX711_COUNT];     /* most recent raw  */

void LoadCell_Init(void)
{
    for (int i = 0; i < HX711_COUNT; ++i) {
        s_offset[i] = 0;
        s_scale[i]  = 1.0f;               /* until calibrated */
        s_raw[i]    = 0;
    }
}

/* One bank read, stored into s_raw[]. */
static void read_bank(void)
{
    int32_t v[HX711_COUNT];
    if (HX711_Bank_Read(v, 250) == HX711_COUNT)
        memcpy(s_raw, v, sizeof s_raw);
}

void LoadCell_Tare(uint8_t idx0, uint8_t samples)
{
    if (idx0 >= HX711_COUNT) return;
    if (samples == 0) samples = 1;
    int64_t acc = 0;
    for (uint8_t s = 0; s < samples; ++s) { read_bank(); acc += s_raw[idx0]; }
    s_offset[idx0] = (int32_t)(acc / samples);
}

void LoadCell_TareAll(uint8_t samples)
{
    if (samples == 0) samples = 1;
    int64_t acc[HX711_COUNT] = {0};
    for (uint8_t s = 0; s < samples; ++s) {
        read_bank();
        for (int i = 0; i < HX711_COUNT; ++i) acc[i] += s_raw[i];
    }
    for (int i = 0; i < HX711_COUNT; ++i) s_offset[i] = (int32_t)(acc[i] / samples);
}

void LoadCell_SetScale(uint8_t idx0, float counts_per_gram)
{
    if (idx0 < HX711_COUNT && counts_per_gram != 0.0f)
        s_scale[idx0] = counts_per_gram;
}

bool LoadCell_Calibrate(uint8_t idx0, float known_grams, uint8_t samples)
{
    if (idx0 >= HX711_COUNT || known_grams == 0.0f) return false;
    if (samples == 0) samples = 1;
    int64_t acc = 0;
    for (uint8_t s = 0; s < samples; ++s) { read_bank(); acc += s_raw[idx0]; }
    int32_t avg = (int32_t)(acc / samples);
    s_scale[idx0] = (float)(avg - s_offset[idx0]) / known_grams;
    return s_scale[idx0] != 0.0f;
}

float LoadCell_Grams(uint8_t idx0)
{
    if (idx0 >= HX711_COUNT) return 0.0f;
    read_bank();
    return (float)(s_raw[idx0] - s_offset[idx0]) / s_scale[idx0];
}

int LoadCell_ReadAllGrams(float grams[HX711_COUNT])
{
    read_bank();
    for (int i = 0; i < HX711_COUNT; ++i)
        grams[i] = (float)(s_raw[i] - s_offset[i]) / s_scale[i];
    return HX711_COUNT;
}

int32_t LoadCell_Raw(uint8_t idx0)
{
    return (idx0 < HX711_COUNT) ? s_raw[idx0] : 0;
}
