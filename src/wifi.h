#pragma once

#include <stdbool.h>
#include "esp_err.h"

// Inicializa el WiFi en modo estación y conecta a la red configurada
// en credentials.h. Bloquea hasta conectar o hasta agotar el timeout.
esp_err_t wifi_connect(void);

// Indica si actualmente hay conexión WiFi activa
bool wifi_is_connected(void);

// Desconecta y apaga el WiFi (para ahorrar energía antes de deep sleep)
void wifi_disconnect(void);