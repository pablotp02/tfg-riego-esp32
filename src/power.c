#include "power.h"

#include "esp_sleep.h"
#include "esp_log.h"
#include "esp_attr.h"

static const char *TAG = "POWER";

// Estado mínimo que queremos conservar entre ciclos de deep sleep
typedef struct 
{
    uint32_t cycle_count;
    uint32_t cycles_since_send;
    uint32_t sensor_error_count;
    uint32_t sensor_read_error_count;
    bool boot_initialized;
} rtc_persisted_state_t;

// Variable almacenada en memoria RTC, persiste tras deep sleep
RTC_DATA_ATTR static rtc_persisted_state_t s_rtc_state = 
{
    .cycle_count = 0,
    .cycles_since_send = 0,
    .sensor_error_count = 0,
    .sensor_read_error_count = 0,
    .boot_initialized = false
};

// No queremos que la función sea pública, por lo que la declaramos aquí y no en power.h
static void rtc_state_reset_defaults(void);

// Reinicia el estado RTC a valores por defecto
static void rtc_state_reset_defaults(void)
{
    s_rtc_state.cycle_count = 0;
    s_rtc_state.cycles_since_send = 0;
    s_rtc_state.sensor_error_count = 0;
    s_rtc_state.sensor_read_error_count = 0;
    s_rtc_state.boot_initialized = false;
}

// Reinicia el estado RTC a valores por defecto
void power_init_persistent_state(void)
{
    // Si todavía no se ha inicializado el estado RTc, lo dejamos con valores por defecto
    if (!s_rtc_state.boot_initialized)
    {
        rtc_state_reset_defaults();
        s_rtc_state.boot_initialized = true;
        ESP_LOGI(TAG, "Estado RTC incializado por primera vez");
    }
    else
    {
        ESP_LOGI(TAG, "Estado RTC ya inicializado, se reutiliza");
    }
}

// Copia el estado persistente RTC al system_ctx_t
void power_restore_ctx_from_rtc(system_ctx_t *ctx)
{
    if (!ctx) 
    {
        ESP_LOGW(TAG, "power_restore_ctx_from_rtc(): ctx es NULL");
        return;
    }

    ctx->cycle_count = s_rtc_state.cycle_count;
    ctx->cycles_since_send = s_rtc_state.cycles_since_send;
    ctx->sensor_error_count = s_rtc_state.sensor_error_count;
    ctx->sensor_read_error_count = s_rtc_state.sensor_read_error_count;

    ESP_LOGI(TAG,
                "Contexto restaurado desde RTC -> cycle=%lu send_since=%lu err_val=%lu err_read=%lu",
            (unsigned long)ctx->cycle_count,
            (unsigned long)ctx->cycles_since_send,
            (unsigned long)ctx->sensor_error_count,
            (unsigned long)ctx->sensor_read_error_count);
}

// Copia desde el contexto normal de ejecución al estado persistente RTC
void power_store_ctx_to_rtc(const system_ctx_t *ctx)
{
    if (!ctx)
    {
        ESP_LOGW(TAG, "power_store_ctx_to_rtc(): ctx es NULL");
        return;
    }

    s_rtc_state.cycle_count = ctx->cycle_count;
    s_rtc_state.cycles_since_send = ctx->cycles_since_send;
    s_rtc_state.sensor_error_count = ctx->sensor_error_count;
    s_rtc_state.sensor_read_error_count = ctx->sensor_read_error_count;
    s_rtc_state.boot_initialized = true;

    ESP_LOGI(TAG,
            "Contexto guardado en RTC -> cycle=%lu send_since=%lu err_val=%lu err_read=%lu",
            (unsigned long)s_rtc_state.cycle_count,
            (unsigned long)s_rtc_state.cycles_since_send,
            (unsigned long)s_rtc_state.sensor_error_count,
            (unsigned long)s_rtc_state.sensor_read_error_count);
}

// Devuelve un texto legible para poder hacer logs personalizados
const char *power_get_wakeup_cause_str(void)
{
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

    switch(cause)
    {
        case ESP_SLEEP_WAKEUP_TIMER:
            return "TIMER";
        case ESP_SLEEP_WAKEUP_EXT0:
            return "EXT0";
        case ESP_SLEEP_WAKEUP_EXT1: 
            return "EXT1";
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            return "TOUCHPAD";
        case ESP_SLEEP_WAKEUP_ULP:
            return "ULP";
        case ESP_SLEEP_WAKEUP_UNDEFINED: 
            return "POWER_ON_OR_RESET";
        default:
            return "UNKNOWN";
    }
}

bool power_is_wakeup_from_timer(void)
{
    return (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER);
}

void power_enter_deep_sleep(uint32_t sleep_ms)
{
    // ESP-IDF pide el número de microsegundos
    uint64_t sleep_us = (uint64_t)sleep_ms * 1000ULL;

    ESP_LOGI(TAG, "Entrando en deep sleep durante %lu ms", (unsigned long)sleep_ms);

    esp_err_t err = esp_sleep_enable_timer_wakeup(sleep_us);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "No se pudo configurar timer wakeup: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "Wakeup source configurada: TIMER");
    ESP_LOGI(TAG, "Deep sleep start...");
    esp_deep_sleep_start();
}