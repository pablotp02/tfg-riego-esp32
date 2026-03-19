#pragma once
#include <stdbool.h>
#include <stdint.h>

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

typedef struct {
    float soil_moisture_pct;  // 0..100
    float temperature_c;      // ºC
    float humidity_pct;       // 0..100
} sensor_data_t;

typedef struct {
    system_state_t state;
    sensor_data_t last;
    bool irrigate_request;
    uint32_t cycle_count;

    bool sensor_valid;
    const char *error_reason;
    uint32_t sensor_error_count;

    uint32_t cycles_since_measure;
    uint32_t cycles_since_send;
    bool pending_send;
} system_ctx_t;

// Inicializa el contexto
void fsm_init(system_ctx_t *ctx);

// Ejecuta UN paso de la FSM (una transición/acción)
void fsm_step(system_ctx_t *ctx);
