#include "actuators.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "TFG_RIEGO";

void actuators_irrigate(uint32_t duration_ms)
{
    ESP_LOGI(TAG, "[IRRIGATE] Activando riego simulado (%lu ms)...", (unsigned long)duration_ms);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    ESP_LOGI(TAG, "[IRRIGATE] Riego simulado finalizado.");
}
