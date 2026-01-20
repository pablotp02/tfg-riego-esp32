#pragma once
#include <stdint.h>

typedef struct {
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
