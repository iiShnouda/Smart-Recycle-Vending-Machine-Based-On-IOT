#include "ina219.h"
#include "pin_map.h"

extern I2C_HandleTypeDef INA219_I2C;     /* hi2c1 */

/* Register map */
#define REG_CONFIG   0x00
#define REG_SHUNT    0x01
#define REG_BUS      0x02
#define REG_POWER    0x03
#define REG_CURRENT  0x04
#define REG_CALIB    0x05

/* Calibration for a 5 mΩ shunt with Current_LSB = 1 mA:
 *   Cal = trunc(0.04096 / (Current_LSB[A] * R_shunt[Ω]))
 *       = 0.04096 / (0.001 * 0.005) = 8192
 *   Power_LSB = 20 * Current_LSB = 20 mW                              */
#define INA219_CAL       8192
#define CURRENT_LSB_mA   1
#define POWER_LSB_mW     20
/* Config: BRNG=32V, PGA=/8 (±320mV), 12-bit bus+shunt, continuous.    */
#define INA219_CONFIG    0x399F

static bool wr(uint8_t reg, uint16_t val)
{
    uint8_t b[2] = { (uint8_t)(val >> 8), (uint8_t)(val & 0xFF) };
    return HAL_I2C_Mem_Write(&INA219_I2C, INA219_ADDR, reg,
                             I2C_MEMADD_SIZE_8BIT, b, 2, 100) == HAL_OK;
}
static bool rd(uint8_t reg, uint16_t *val)
{
    uint8_t b[2];
    if (HAL_I2C_Mem_Read(&INA219_I2C, INA219_ADDR, reg,
                         I2C_MEMADD_SIZE_8BIT, b, 2, 100) != HAL_OK)
        return false;
    *val = ((uint16_t)b[0] << 8) | b[1];
    return true;
}

bool INA219_Present(void)
{
    return HAL_I2C_IsDeviceReady(&INA219_I2C, INA219_ADDR, 2, 100) == HAL_OK;
}

bool INA219_Init(void)
{
    if (!wr(REG_CONFIG, INA219_CONFIG)) return false;
    return wr(REG_CALIB, INA219_CAL);
}

int32_t INA219_BusVoltage_mV(void)
{
    uint16_t v;
    if (!rd(REG_BUS, &v)) return -1;
    /* bits 15..3 hold the voltage; LSB = 4 mV. */
    return (int32_t)((v >> 3) * 4);
}

int32_t INA219_ShuntVoltage_uV(void)
{
    uint16_t v;
    if (!rd(REG_SHUNT, &v)) return 0;
    return (int32_t)((int16_t)v) * 10;        /* LSB = 10 µV, signed */
}

int32_t INA219_Current_mA(void)
{
    uint16_t v;
    /* Re-assert calibration (survives an INA219 power glitch), then read. */
    wr(REG_CALIB, INA219_CAL);
    if (!rd(REG_CURRENT, &v)) return 0;
    return (int32_t)((int16_t)v) * CURRENT_LSB_mA;
}

int32_t INA219_Power_mW(void)
{
    uint16_t v;
    if (!rd(REG_POWER, &v)) return 0;
    return (int32_t)v * POWER_LSB_mW;
}
