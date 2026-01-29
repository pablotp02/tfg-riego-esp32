#pragma once 
#include "driver/gpio.h"
#include "esp_err.h"

// Lee DHT22 (1 wire-like, timings)
// temp_c y hum_pct devueltos en unidades reales
// Devuelve ESP_OK si lectura válida (incluye checksum OK)
// función tipo esp_err_t para unificar errores con el resto del proyecto
esp_err_t dht22_read(gpio_num_t pin, float *temp_c, float *hum_pct);