#pragma once

#include <stdint.h>
#include "esp_err.h"

// Dirección I2C por defecto del INA219 (A0=GND, A1=GND)
#define INA219_I2C_ADDR     (0x40)

// Pines I2C
#define INA219_SDA_GPIO     (21)
#define INA219_SCL_GPIO     (22)

// Puerto I2C
#define INA219_I2C_PORT     (0)

// Voltajes de la batería LiPo 2S
#define LIPO_2S_VOLTAGE_MAX (8.4f)   // voltaje máximo (cargada al 100%)
#define LIPO_2S_VOLTAGE_MIN (6.0f)   // voltaje mínimo (descargada al 0%)

// Inicializa el bus I2C y configura el INA219
esp_err_t ina219_init(void);

// Lee el voltaje del bus (voltaje de la batería) en voltios
esp_err_t ina219_read_bus_voltage(float *voltage_out);

// Devuelve el porcentaje de batería (0-100%) a partir del voltaje
float ina219_voltage_to_pct(float voltage);