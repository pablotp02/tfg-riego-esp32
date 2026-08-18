#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "power_types.h"
#include "device_config.h"

typedef enum {
    STATE_INIT = 0,
    STATE_SCHEDULE,
    STATE_MEASURE,
    STATE_VALIDATE,
    STATE_DECIDE,
    STATE_IRRIGATE,
    STATE_LOG,
    STATE_SEND,
    STATE_SLEEP,
    STATE_ERROR
} system_state_t;

// Estructura para representar la humedad y temperatura del suelo
typedef struct {
    float soil_moisture_pct;  // 0..100
    float soil_temp_c;        // ºC
} sensor_data_t;

typedef struct {
    system_state_t state;
    sensor_data_t last;
    bool irrigate_request;
    uint32_t cycle_count;

    bool sensor_valid;
    const char *error_reason;
    uint32_t sensor_error_count;
    uint32_t sensor_read_error_count;

    uint32_t cycles_since_measure;
    uint32_t cycles_since_send;
    bool pending_send;

    // Cooldown riego: 
    // Cuenta los ciclos transcurridos desde el último riego
    // Se inicializa a IRRIGATION_COOLDOWN_CYCLES para que el primer ciclo
    // pueda regar si es necesario
    uint32_t cycles_since_irrigated;

    float battery_level_pct;
    power_mode_t power_mode;

    // Configuración remota del sistema (umbrales, cooldown, tiempos)
    device_config_t device_cfg;

    // true si device_cfg viene de una sincronización con el backend en
    // este ciclo; false si se está usando el valor guardado en RTC o
    // los valores por defecto (sin sincronización disponible)
    bool device_cfg_synced;

    // Motivo detallado de la última decisión de riego (histéresis,
    // cooldown, modo crítico, etc.), calculado en decide_irrigation()
    // y conservado aquí para poder incluirlo en el payload enviado
    // al backend
    const char *irrigate_reason;
} system_ctx_t;

// Inicializa el contexto
void fsm_init(system_ctx_t *ctx);

// Ejecuta UN paso de la FSM (una transición/acción)
void fsm_step(system_ctx_t *ctx);