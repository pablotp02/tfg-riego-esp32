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

static bool gpio_to_adc1_channel(gpio_num_t gpio, adc_channel_t *out_ch)
{
    switch(gpio)
    {
        case GPIO_NUM_34: *out_ch = ADC_CHANNEL_6; return true;
        case GPIO_NUM_35: *out_ch = ADC_CHANNEL_7; return true;
        default: return false; // no soportado
    }
}

static int read_adc_raw(gpio_num_t gpio)
{
    adc_channel_t ch;
    if (!gpio_to_adc1_channel(gpio, &ch))
    {
        ESP_LOGE(TAG, "[ADC] GPIO %d no soportado (solo 34/35)", (int)gpio);
        return -1;
    }

    int raw = 0;
    adc_oneshot_read(adc1_handle, ch, &raw);
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

    // Configurar canales y atenuación (GPIO34/35)
    adc_oneshot_chan_cfg_t chan_cfg = 
    {
        .atten = ADC_ATTEN_DB_11, // rango aprox. hasta 3.3V
        .bitwidth = ADC_BITWIDTH_DEFAULT
    };

    if (adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_6, &chan_cfg) != ESP_OK)
    {
        ESP_LOGE(TAG, "[ADC] No se pudo configurar ADC1_CH6 (GPIO34)");
        return false;
    }
    if (adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_7, &chan_cfg) != ESP_OK)
    {
        ESP_LOGE(TAG, "[ADC] No se pudo configurar ADC1_CH7 (GPIO35)");
        return false;
    }

    cali_enabled = adc_calibration_init();
    ESP_LOGI(TAG, "[ADC] init OK (GPIO34/GPIO35). Calibración: %s", cali_enabled ? "SI" : "NO");

    return true;
}

sensor_data_t sensors_adc_read(void)
{
    int raw_moist = read_adc_raw(SOIL_MOIST_GPIO);
    int raw_temp  = read_adc_raw(SOIL_TEMP_GPIO);

    // mV solo para el de humedad, para no liar logs
    int mv_moist = -1;
    if (cali_enabled)
    {
        adc_cali_raw_to_voltage(cali_handle, raw_moist, &mv_moist);
    }

    // Humedad suelo (provisional, escala lineal)
    float soil_pct = (raw_moist * 100.0f) / 4095.0f;

    // Temperatura suelo (provisoinal) 0 a 50ºC
    float soil_temp_c = (raw_temp * 50.0f) / 4095.0f;

    ESP_LOGI(TAG, "[ADC] moist_raw=%d %s | temp_raw=%d", raw_moist, (mv_moist >= 0) ? "" : "(sin mV)", raw_temp);

    if (mv_moist >= 0)
    {
        ESP_LOGI(TAG, "[ADC] moist=%d mV | soil_pct(prov)=%.1f%% | soil_temp(prov)=%.1fºC", mv_moist, soil_pct, soil_temp_c);
    }
    else 
    {
        ESP_LOGI(TAG, "[ADC] soil_pct(prov)=%.1f%% | soil_temp(prov)=%.1fºC", soil_pct, soil_temp_c);
    }

    sensor_data_t d = 
    {
        .soil_moisture_pct = soil_pct,
        .temperature_c     = soil_temp_c, // temperatura de suelo
        .humidity_pct      = 50.0f        // lo pisa DHT22 en sensors.c
    };

    return d;

}
