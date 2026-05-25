#include "sensors.h"
#include "config.h"
#include "sensors_sim.h"
#include "sensors_adc.h"
#include "sen0604.h"

#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "SENSORS";

// Caché para SEN0604
static bool s_have_sen0604 = false;
static float s_last_soil_moist_pct = 0.0f;
static float s_last_soil_temp_c = 0.0f;
static float s_last_ph = 0.0f;
static float s_last_ec = 0.0f;

static bool s_last_rs485_read_ok = true;

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
            s_last_rs485_read_ok = true;

            d.soil_moisture_pct = sen.soil_moisture_pct;
            d.soil_temp_c = sen.soil_temp_c;

            s_last_soil_moist_pct = sen.soil_moisture_pct;
            s_last_soil_temp_c = sen.soil_temp_c;
            s_last_ph = sen.ph;
            s_last_ec = sen.ec_us_cm;
            s_have_sen0604 = true;

            ESP_LOGI(TAG, "[SEN0604] suelo=%.1f%% | pH=%.1f | EC=%.0f",
                     sen.soil_moisture_pct, sen.ph, sen.ec_us_cm);
        } 
        else 
        {
            s_last_rs485_read_ok = false;

            ESP_LOGW(TAG, "[SEN0604] Fallo lectura (%s). Usando último valor %s",
                     esp_err_to_name(rs_err), s_have_sen0604 ? "válido" : "inválido");


            if (s_have_sen0604)
            {
                // Si ya hubo una lectura válida antes, reutilizamos el último valor bueno
                d.soil_moisture_pct = s_last_soil_moist_pct;
                d.soil_temp_c = s_last_soil_temp_c;
            }
            else 
            {
                // Si nunca hubo una lectura válida del sensor, devolvemos datos inválidos
                // para que la FSM no tome decisiones de riego a ciegas
                d.soil_moisture_pct = -1.0f;
            }
        }
    } 
    else 
    {
        d = sensors_adc_read();
    }

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

    ESP_LOGI(TAG, "Inicialización de sensores completada");
    return true;
}

// Devuelve el último pH almacenado
float sensors_get_last_ph(void)
{
    return s_last_ph;
}

// Devuelve la última EC almacenada
float sensors_get_last_ec(void)
{
    return s_last_ec;
}

// Permite distinguir si la caché es válida o no
bool sensors_have_sen0604_data(void)
{
    return s_have_sen0604;
}

// Indica si la última lectura del sensor RS485 se realizó correctamente
bool sensors_last_rs485_read_ok(void)
{
    return s_last_rs485_read_ok;
}