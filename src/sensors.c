#include "sensors.h"
#include "config.h"
#include "sensors_sim.h"
#include "sensors_adc.h"
#include "sensors_dht22.h"
#include "sen0604.h"

#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "SENSORS";

// Caché de la última lectura válida del DHT22 (estados internos del módulo)
static bool s_have_dht22 = false;   // la 's_' indica que es estático a nivel de fichero 
static float s_last_air_temp_c = 25.0f; // (static file-scope)
static float s_last_air_hum_pct = 50.0f;

// Caché para SEN0604
static bool s_have_sen0604 = false;
static float s_last_soil_moist_pct = 0.0f;
static float s_last_soil_temp_c = 25.0f;
static float s_last_ph = 0.0f;
static float s_last_ec = 0.0f;

sensor_data_t sensors_read(void)
{
    const system_config_t *cfg = config_get();
    sensor_data_t d = {0};

    // 1) Modo simulado
    if (cfg->use_simulated_sensors) 
    {
        return sensors_sim_read();
    }

    // 2) Fuente de datos del suelo
    if (cfg->use_rs485_sensor) 
    {
        sen0604_data_t sen = {0};
        esp_err_t rs_err = sen0604_read_all(RS485_SLAVE_ADDR, &sen);

        if (rs_err == ESP_OK) 
        {
            d.soil_moisture_pct = sen.soil_moisture_pct;
            d.temperature_c = sen.soil_temp_c;

            s_last_soil_moist_pct = sen.soil_moisture_pct;
            s_last_soil_temp_c = sen.soil_temp_c;
            s_last_ph = sen.ph;
            s_last_ec = sen.ec_us_cm;
            s_have_sen0604 = true;

            ESP_LOGI(TAG, "[SEN0604] suelo=%.1f%% | temp_suelo=%.1fC | pH=%.1f | EC=%.0f",
                     sen.soil_moisture_pct, sen.soil_temp_c, sen.ph, sen.ec_us_cm);
        } 
        else 
        {
            ESP_LOGW(TAG, "[SEN0604] Fallo lectura (%s). Usando último valor %s",
                     esp_err_to_name(rs_err), s_have_sen0604 ? "válido" : "por defecto");

            // fallback si falla RS485
            d.soil_moisture_pct = s_last_soil_moist_pct;
            d.temperature_c = s_last_soil_temp_c;
        }
    } 
    else 
    {
        d = sensors_adc_read();
    }

    // 3) DHT22 -> humedad ambiente
    float t = 0.0f, h = 0.0f;
    esp_err_t err = dht22_read(DHT22_GPIO, &t, &h);

    if (err == ESP_OK) 
    {
        s_last_air_temp_c = t;
        s_last_air_hum_pct = h;
        s_have_dht22 = true;
        ESP_LOGI(TAG, "[DHT22] OK -> T=%.1fC H=%.1f%%", t, h);
    }
    else
    {
        ESP_LOGW(TAG, "[DHT22] Fallo lectura (%s). Usando último valor %s",
                 esp_err_to_name(err),
                 s_have_dht22 ? "válido" : "por defecto");
    }

    // 4) Mantener humidity_pct como humedad ambiente
    d.humidity_pct = s_last_air_hum_pct;

    return d;
}

bool sensors_init(void)
{
    const system_config_t *cfg = config_get();

    ESP_LOGI(TAG, "Inicializando sensores...");

    if (cfg->use_simulated_sensors)
    {
        ESP_LOGI(TAG, "Modo simulación: no se inicializan sensores reales");
        return true;
    }

    if (cfg->use_rs485_sensor) // Inicialización RS485
    {
        if (sen0604_init() != ESP_OK)
        {
            ESP_LOGE(TAG, "Fallo en sen0604_init()");
            return false;
        }
    }
    else // Inicialización ADC
    {
        if (!sensors_adc_init())
        {
            ESP_LOGE(TAG, "Fallo en sensors_adc_init()");
            return false;
        }
    }

    // Inicialización DHT22 (temperatura y humedad ambiente)
    sensors_dht22_init();

    ESP_LOGI(TAG, "Inicialización de sensores completada");
    return true;
}
