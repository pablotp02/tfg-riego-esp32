#pragma once 

#include <stdbool.h>
#include <stdint.h>
#include "fsm.h"

// Inicializa el estado persistente si es el primer arranque
void power_init_persistent_state(void);

// Restaura en el contexto de la FSM los campos persistentes almacenados en RTC
void power_restore_ctx_from_rtc(system_ctx_t *ctx);

// Guarda en RTC los campos persistentes del contexto actual
void power_store_ctx_to_rtc(const system_ctx_t *ctx);

// Configura el temporizador de wakeup y entra en deep sleep
void power_enter_deep_sleep(uint32_t sleep_ms);

// Devuelve una cadena legible con la causa del último wakeup
const char *power_get_wakeup_cause_str(void);

// Indica si el último arranque proviene de un wakeup por temporizador
bool power_is_wakeup_from_timer(void);