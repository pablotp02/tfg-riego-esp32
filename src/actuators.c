#include "actuators.h"
#include "config.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "ACTUATORS";

static bool initialized = false;

static void relay_init(void)
{
    if (initialized) return;

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << RELAY_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE, 
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error configurando relé: %s", esp_err_to_name(err));
        return;
    }

    // APAGAR relé
    gpio_set_level(RELAY_GPIO, 0);

    initialized = true; 

    ESP_LOGI(TAG, "Relé inicializado en GPIO %d", RELAY_GPIO);
}

void actuators_irrigate(uint32_t duration_ms)
{
    relay_init();

    if (!initialized)
    {
        ESP_LOGE(TAG, "[IRRIGATE] No se pudo inicializar el relé");
        return;
    }

    // Compilación condicional para habilitar/deshabilitar
    // el control físico dle relé durante las pruebas
    // Si ENABLE_REAL_RELAY = 0, el sistema simula riego sin activar hardware real
    #if ENABLE_REAL_RELAY
        ESP_LOGI(TAG, "[IRRIGATE] Activando relé (%lu ms)...", (unsigned long)duration_ms);

        gpio_set_level(RELAY_GPIO, 1); // ENCENDER relé

        vTaskDelay(pdMS_TO_TICKS(duration_ms));

        gpio_set_level(RELAY_GPIO, 0); // APAGAR relé

        ESP_LOGI(TAG, "[IRRIGATE] Relé desactivado"); 
    #else
        ESP_LOGW(TAG, 
                "[IRRIGATE] Riego real deshabilitado por configuración (%lu ms)",
                (unsigned long)duration_ms);
        
        vTaskDelay(pdMS_TO_TICKS(duration_ms));

        ESP_LOGI(TAG, "[IRRIGATE] Simulación de riego finalizada");
    #endif
}