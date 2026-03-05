#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float soil_moisture_pct; // %
    float soil_temp_c;       // ºC
    float ph;                // pH
    float ec_us_cm;          // uS/cm
} sen0604_data_t;

esp_err_t sen0604_init(void);

// Lee humedad, temperatura, pH y EC en una única lectura Modbus (4 registros)
esp_err_t sen0604_read_all(uint8_t slave_addr, sen0604_data_t *out);

#ifdef __cplusplus
}
#endif