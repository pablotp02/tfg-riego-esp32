#pragma once

#include "esp_err.h"
#include "fsm.h"

// Construye el payload JSON a partir del contexto de la FSM y lo envía
// al backend mediante HTTP POST a /api/cycles/
// Devuelve ESP_OK si el envío fue exitoso (código HTTP 200)
esp_err_t http_send_cycle(const system_ctx_t *ctx);

// Envía al backend los datos de un ciclo cuyo envío falló
// anteriormente, reconstruidos a partir de la estructura persistida
// en RTC, en lugar de a partir del contexto actual del sistema
esp_err_t http_send_pending_cycle(const pending_cycle_data_t *pending);