#include "fsm.h"
#include "config.h"
#include "esp_log.h"
#include "sensors.h"

#include "driver/adc.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static const char *TAG = "TFG_RIEGO";

// Vamos a usar ADC1 en GPIO34 (ADC_CHANNEL_6 en ADC_UNIT_1)
static adc_oneshot_unit_handle_t adc1_handle;
static adc_cali_handle_t cali_handle;
static bool cali_enabled = false;

static bool adc_calibration_init(void)
{
#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED

    adc_cali_line_fitting_config_t line_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN_DB_11,
        .bitwidth = ADC_BITWIDTH_DEFAULT
    };

    if (adc_cali_create_scheme_line_fitting(&line_config, &cali_handle) == ESP_OK) {
        ESP_LOGI(TAG, "[ADC] Calibración Line Fitting inicializada");
        return true;
    } else {
        ESP_LOGW(TAG, "[ADC] No se pudo inicializar calibración Line Fitting");
        return false;
    }

#else
    ESP_LOGW(TAG, "[ADC] Line Fitting no soportado en este ESP-IDF");
    return false;
#endif
}


static int read_adc_raw_gpio34(void)
{
    int raw = 0;
    adc_oneshot_read(adc1_handle, ADC_CHANNEL_6, &raw);
    return raw;
}

bool sensors_adc_init(void)
{
    // Configurar unidad ADC1
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    if (adc_oneshot_new_unit(&unit_cfg, &adc1_handle) != ESP_OK) {
        ESP_LOGE(TAG, "[ADC] No se pudo inicializar ADC1");
        return false;
    }

    // Configurar canal (GPIO34 -> ADC_CHANNEL_6) y atenuación
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_11,          // rango aprox hasta 3.3V
        .bitwidth = ADC_BITWIDTH_DEFAULT
    };
    if (adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_6, &chan_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "[ADC] No se pudo configurar canal ADC1_CH6 (GPIO34)");
        return false;
    }

    cali_enabled = adc_calibration_init();
    ESP_LOGI(TAG, "[ADC] init OK (GPIO34). Calibración: %s", cali_enabled ? "SI" : "NO");

    return true;
}

sensor_data_t sensors_adc_read(void)
{
    int raw = read_adc_raw_gpio34();

    int mv = -1;
    if (cali_enabled) {
        adc_cali_raw_to_voltage(cali_handle, raw, &mv);
    }

    ESP_LOGI(TAG, "[ADC] raw=%d %s", raw, (mv >= 0) ? "" : "(sin mV)");

    // Provisional: devolvemos “suelo” como una escala simple solo para demostrar lectura.
    // (No es calibración real de humedad. Se implementará con el sensor definitivo.)
    sensor_data_t d = {
        .soil_moisture_pct = (raw * 100.0f) / 4095.0f,
        .temperature_c = 25.0f,
        .humidity_pct = 50.0f
    };

    if (mv >= 0) {
        ESP_LOGI(TAG, "[ADC] approx=%d mV | soil_pct(prov)=%.1f%%", mv, d.soil_moisture_pct);
    }

    return d;
}
