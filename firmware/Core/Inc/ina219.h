/*
 * ina219.h — INA219 bus-voltage / current / power sensor (I2C1, PB6/PB7).
 *
 * Board: 5 mΩ shunt (SR1). Calibrated for Current_LSB = 1 mA, so the
 * current register reads directly in milliamps (max ~32 A). 32 V bus
 * range, 12-bit ADC, continuous shunt+bus conversion.
 */
#ifndef INA219_H
#define INA219_H

#include <stdint.h>
#include <stdbool.h>

bool    INA219_Init(void);              /* config + calibration */
bool    INA219_Present(void);           /* ACKs on the bus? */
int32_t INA219_BusVoltage_mV(void);     /* rail voltage, mV (-1 on err) */
int32_t INA219_ShuntVoltage_uV(void);   /* across the shunt, µV (signed) */
int32_t INA219_Current_mA(void);        /* signed load current, mA */
int32_t INA219_Power_mW(void);          /* load power, mW */

#endif /* INA219_H */
