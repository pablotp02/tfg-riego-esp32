#include "dht22_test.h"
#include "config.h" // Para DHT22_GPIO
#include "sensors_dht22.h" /// El driver

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "DHT22_TEST";

static void dht22_task(void *arg)
{
    (void)arg;

    ESP_LOGI(TAG, "Iniciando test DHT22 en GPIO %d", (int)DHT22_GPIO);

    while(1)
    {
        float t = 0.0f, h = 0.0f;
        esp_err_t err = dht22_read(DHT22_GPIO, &t, &h);

        if (err == ESP_OK)
        {
            ESP_LOGI(TAG, "OK -> Temp=%.1f ºC | Hum=%.1f %%", t, h);
        }
        else
        {
            ESP_LOGW(TAG, "Fallo lectura DHT22: %s", esp_err_to_name(err));
        }

        vTaskDelay(pdMS_TO_TICKS(2500)); // 2.5s (DHT22 no conviene leerlo muy seguido)
    }
}

void dht22_test_start(void)
{
    // Stack: DHT22 usa delays y lectura GPIO, no es enorme, pero mejor no ir justos
    xTaskCreate(dht22_task, "dht22_task", 4096, NULL, 5, NULL);
}