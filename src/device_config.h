#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// Configuración remota del sistema, sincronizada desde el backend
typedef struct {
    float soil_start_irrigation_pct;
    float soil_stop_irrigation_pct;
    float soil_min_temp_c;
    uint32_t irrigation_cooldown_cycles;
    uint32_t measure_period_ms;
    uint32_t irrigate_time_ms;
} device_config_t;

// Pide la configuración activa al backend (GET /api/config/latest)
// y la parsea en out_config. Requiere WiFi ya conectado.
// Devuelve ESP_OK si se obtuvo y parseó correctamente.
esp_err_t device_config_fetch(device_config_t *out_config);

// Devuelve valores por defecto (los que ya estaban en config.c) para
// usar como fallback si nunca se ha sincronizado con el backend.
void device_config_get_defaults(device_config_t *out_config);