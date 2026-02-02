#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "hal/gpio_types.h"

// PINES
#define DHT22_GPIO      GPIO_NUM_27
#define SOIL_MOIST_GPIO GPIO_NUM_34 // potenciómetro 1 -> humedad suelo
#define SOIL_TEMP_GPIO  GPIO_NUM_35 // potenciómetro 2 -> temperatura suelo

// Umbrales ambientales provisionales (se ajustarán por especie/planta)
#define AMBIENT_MIN_TEMP_C  (5.0f)  // por debajo, no regar
#define AMBIENT_MAX_HUM_PCT (95.0f) // por encima, no regar

typedef struct {
    bool use_simulated_sensors;     // true: simulación | false: ADC real
    
    // Temporización (demo)
    uint32_t measure_period_ms;     // tiempo entre ciclos (demo). Luego: horas
    uint32_t irrigate_time_ms;      // tiempo de riego

    // Planificador por ciclos (demo)
    uint32_t measure_every_n_cycles; // cada cuántos ciclos medir
    uint32_t send_every_n_cycles;    // cada cuántos ciclos enviar

    // Umbrales (provisionales)
    float soil_threshold_pct;       // umbral humedad suelo (%)

} system_config_t;

// Devuelve la configuración actual (por ahora: valores por defecto)
const system_config_t* config_get(void);

// (Opcional futuro) Permitir cambiar config en runtime
// void config_set(const system_config_t* new_cfg);
