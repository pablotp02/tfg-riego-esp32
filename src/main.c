#include "fsm.h"
#include "power.h"
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    system_ctx_t ctx;

    // 0) Inicializar NVS (necesario para el WiFi)
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_err);

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