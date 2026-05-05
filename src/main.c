#include "fsm.h"
#include "power.h"
#include "esp_log.h"

static const char *TAG = "MAIN";

void app_main(void)
{
 /* dht22_test_start();
    return; // solo para la prueba del dht22 y no arrancar la fsm */

    system_ctx_t ctx;

    // 1) Inicializar estado persistente de energía / RTC
    power_init_persistent_state();

    // 2) Inicializar contexto normal de la FSM
    fsm_init(&ctx);

    // 3) Restaurar en el contexto los campos persistentes
    power_restore_ctx_from_rtc(&ctx);

    // 4) Log informativo de la causa del arranque / wakeup
    ESP_LOGI(TAG, "Wakeup cause: %s", power_get_wakeup_cause_str());

    while (1) {
        fsm_step(&ctx);
    }
}
