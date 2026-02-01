#include "sensors.h"
#include "config.h"
#include "sensors_sim.h"
#include "sensors_adc.h"
#include "sensors_dht22.h"

#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "SENSORS";

// Caché de la última lectura válida del DHT22 (estados internos del módulo)
static bool s_have_dht22 = false;   // la 's_' indica que es estático a nivel de fichero 
static float s_last_temp_c = 25.0f; // (static file-scope)
static float s_last_hum_pct = 50.0f;

sensor_data_t sensors_read(void)
{
    const system_config_t *cfg = config_get();

    // 1) Modo simulado 
    if (cfg->use_simulated_sensors)
    {
        return sensors_sim_read();
    }

    // 2) Modo real: suelo por ADC
    sensor_data_t d = sensors_adc_read();
    // Ahora mismo rellena soil_moisture_pct y deja dummy temp y hum, pero lo vamos a sobreescribir

    // 3) Modo real: temp/hum por DHT22
    float t = 0.0f, h = 0.0f;
    esp_err_t err = dht22_read(DHT22_GPIO, &t, &h);

    if (err == ESP_OK)
    {
        s_last_temp_c = t;
        s_last_hum_pct = h;
        s_have_dht22 = true;
        ESP_LOGI(TAG, "[DHT22] OK -> T=%.1fºC H=%.1f%%", t, h);
    }
    else
    {
        ESP_LOGW(TAG, "[DHT22] Fallo lectura (%s). Usando último valor %s", esp_err_to_name(err), 
                                                            s_have_dht22 ? "válido" : "por defecto");
    }

    // 4) Sobreescribimos en el struct final
    d.temperature_c = s_last_temp_c;
    d.humidity_pct = s_last_hum_pct;

    return d;
}

bool sensors_init(void)
{
    const system_config_t *cfg = config_get();

    ESP_LOGI(TAG, "Inicializando sensores...");

    if (cfg->use_simulated_sensors)
    {
        ESP_LOGI(TAG, "Modo simulación: no se incializan sensores reales");
        return true;
    }

    // Inicialización ADC (humedad de suelo)
    if (!sensors_adc_init())
    {
        ESP_LOGE(TAG, "Fallo en sensors_adc_init()");
        return false;
    }

    // Inicialización DHT22 (temperatura y humedad ambiente)
    sensors_dht22_init();

    ESP_LOGI(TAG, "Inicialización de sensores completada");
    return true;
}
