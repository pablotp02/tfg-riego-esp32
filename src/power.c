#include "power.h"
#include "config.h"

#include "esp_sleep.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "ina219.h"
#include "device_config.h"

static const char *TAG = "POWER";

// Estado mínimo que queremos conservar entre ciclos de deep sleep
typedef struct
{
    uint32_t cycle_count;
    uint32_t cycles_since_send;
    uint32_t cycles_since_irrigated; // cooldown de riego
    uint32_t sensor_error_count;
    uint32_t sensor_read_error_count;
    bool pending_send; // envío pendiente tras un fallo de conexión/HTTP
    bool boot_initialized;

    float battery_level_pct;
    power_mode_t power_mode;

    // Última configuración remota sincronizada con el backend,
    // usada como fallback cuando no hay WiFi disponible
    device_config_t device_cfg;
    bool device_cfg_valid; // true si device_cfg contiene una sincronización real
} rtc_persisted_state_t;

// Variable almacenada en memoria RTC, persiste tras deep sleep
RTC_DATA_ATTR static rtc_persisted_state_t s_rtc_state =
{
    .cycle_count = 0,
    .cycles_since_send = 0,
    .cycles_since_irrigated = IRRIGATION_COOLDOWN_CYCLES, // arranca listo para regar
    .sensor_error_count = 0,
    .sensor_read_error_count = 0,
    .pending_send = false,
    .boot_initialized = false
};

static void rtc_state_reset_defaults(void); // forward declaration

static void rtc_state_reset_defaults(void)
{
    s_rtc_state.cycle_count = 0;
    s_rtc_state.cycles_since_send = 0;
    s_rtc_state.cycles_since_irrigated = IRRIGATION_COOLDOWN_CYCLES;
    s_rtc_state.sensor_error_count = 0;
    s_rtc_state.sensor_read_error_count = 0;
    s_rtc_state.pending_send = false;

    s_rtc_state.battery_level_pct = 100.0f;
    s_rtc_state.power_mode = POWER_MODE_NORMAL;

    // Sin configuración sincronizada todavía
    s_rtc_state.device_cfg_valid = false;

    s_rtc_state.boot_initialized = false;
}

void power_init_persistent_state(void)
{
    if (!s_rtc_state.boot_initialized)
    {
        rtc_state_reset_defaults();
        s_rtc_state.boot_initialized = true;
        ESP_LOGI(TAG, "Estado RTC inicializado por primera vez");
    }
    else
    {
        ESP_LOGI(TAG, "Estado RTC ya inicializado, se reutiliza");
    }
}

void power_restore_ctx_from_rtc(system_ctx_t *ctx)
{
    if (!ctx)
    {
        ESP_LOGW(TAG, "power_restore_ctx_from_rtc(): ctx es NULL");
        return;
    }

    ctx->cycle_count              = s_rtc_state.cycle_count;
    ctx->cycles_since_send        = s_rtc_state.cycles_since_send;
    ctx->cycles_since_irrigated   = s_rtc_state.cycles_since_irrigated;
    ctx->sensor_error_count       = s_rtc_state.sensor_error_count;
    ctx->sensor_read_error_count  = s_rtc_state.sensor_read_error_count;
    ctx->pending_send             = s_rtc_state.pending_send;
    ctx->battery_level_pct        = s_rtc_state.battery_level_pct;
    ctx->power_mode               = s_rtc_state.power_mode;

    // Restaurar configuración remota si hay una sincronización previa válida
    if (s_rtc_state.device_cfg_valid)
    {
        ctx->device_cfg = s_rtc_state.device_cfg;
        ctx->device_cfg_synced = false; // viene de RTC, no de una sincronización fresca
    }

    ESP_LOGI(TAG,
            "Contexto restaurado desde RTC -> cycle=%lu send_since=%lu irrigated_since=%lu err_val=%lu err_read=%lu battery=%.1f%% mode=%d cfg_valid=%s",
            (unsigned long)ctx->cycle_count,
            (unsigned long)ctx->cycles_since_send,
            (unsigned long)ctx->cycles_since_irrigated,
            (unsigned long)ctx->sensor_error_count,
            (unsigned long)ctx->sensor_read_error_count,
            ctx->battery_level_pct,
            ctx->power_mode,
            s_rtc_state.device_cfg_valid ? "SI" : "NO");
}

void power_store_ctx_to_rtc(const system_ctx_t *ctx)
{
    if (!ctx)
    {
        ESP_LOGW(TAG, "power_store_ctx_to_rtc(): ctx es NULL");
        return;
    }

    s_rtc_state.cycle_count             = ctx->cycle_count;
    s_rtc_state.cycles_since_send       = ctx->cycles_since_send;
    s_rtc_state.cycles_since_irrigated  = ctx->cycles_since_irrigated;
    s_rtc_state.sensor_error_count      = ctx->sensor_error_count;
    s_rtc_state.sensor_read_error_count = ctx->sensor_read_error_count;
    s_rtc_state.pending_send            = ctx->pending_send;
    s_rtc_state.boot_initialized        = true;
    s_rtc_state.battery_level_pct       = ctx->battery_level_pct;
    s_rtc_state.power_mode              = ctx->power_mode;

    // Solo actualizamos la configuración guardada en RTC si este ciclo
    // sincronizó realmente con el backend (evita sobreescribir con
    // valores por defecto o RTC antiguos re-guardados sin cambios)
    if (ctx->device_cfg_synced)
    {
        s_rtc_state.device_cfg = ctx->device_cfg;
        s_rtc_state.device_cfg_valid = true;
    }

    ESP_LOGI(TAG,
            "Contexto guardado en RTC -> cycle=%lu send_since=%lu irrigated_since=%lu err_val=%lu err_read=%lu battery=%.1f%% mode=%d cfg_synced=%s",
            (unsigned long)s_rtc_state.cycle_count,
            (unsigned long)s_rtc_state.cycles_since_send,
            (unsigned long)s_rtc_state.cycles_since_irrigated,
            (unsigned long)s_rtc_state.sensor_error_count,
            (unsigned long)s_rtc_state.sensor_read_error_count,
            s_rtc_state.battery_level_pct,
            s_rtc_state.power_mode,
            ctx->device_cfg_synced ? "SI" : "NO");
}

const char *power_get_wakeup_cause_str(void)
{
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

    switch(cause)
    {
        case ESP_SLEEP_WAKEUP_TIMER:    return "TIMER";
        case ESP_SLEEP_WAKEUP_EXT0:     return "EXT0";
        case ESP_SLEEP_WAKEUP_EXT1:     return "EXT1";
        case ESP_SLEEP_WAKEUP_TOUCHPAD: return "TOUCHPAD";
        case ESP_SLEEP_WAKEUP_ULP:      return "ULP";
        case ESP_SLEEP_WAKEUP_UNDEFINED:return "POWER_ON_OR_RESET";
        default:                        return "UNKNOWN";
    }
}

bool power_is_wakeup_from_timer(void)
{
    return (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER);
}

void power_enter_deep_sleep(uint32_t sleep_ms)
{
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

void power_update_battery_and_mode(system_ctx_t *ctx)
{
    if (!ctx)
    {
        ESP_LOGW(TAG, "power_update_battery_and_mode(): ctx es NULL");
        return;
    }

    #if ENABLE_BATTERY_SIMULATION
        // simulación activa: consumo por ciclo según modo energético actual
        float consumption = 0.0f;

        switch (ctx->power_mode)
        {
            case POWER_MODE_NORMAL:   consumption = 10.0f; break;
            case POWER_MODE_LOW:      consumption = 5.0f;  break;
            case POWER_MODE_CRITICAL: consumption = 2.0f;  break;
            default:                  consumption = 1.0f;  break;
        }

        ctx->battery_level_pct -= consumption;
        if (ctx->battery_level_pct < 0.0f) ctx->battery_level_pct = 0.0f;

        power_mode_t prev_mode = ctx->power_mode;

        if      (ctx->battery_level_pct >= 50.0f) ctx->power_mode = POWER_MODE_NORMAL;
        else if (ctx->battery_level_pct >= 20.0f) ctx->power_mode = POWER_MODE_LOW;
        else                                       ctx->power_mode = POWER_MODE_CRITICAL;

        if (ctx->power_mode != prev_mode)
        {
            ESP_LOGW(TAG,
                    "Cambio de modo energético: %s -> %s (batería=%.1f%%)",
                    power_mode_to_str(prev_mode),
                    power_mode_to_str(ctx->power_mode),
                    ctx->battery_level_pct);
        }
        else
        {
            ESP_LOGI(TAG, "Batería=%.1f%% | modo=%s", 
                ctx->battery_level_pct, 
                power_mode_to_str(ctx->power_mode));
        }

    #else 
        // Medición real de batería mediante INA219
        float voltage = 0.0f;
        esp_err_t err = ina219_read_bus_voltage(&voltage);

        if (err == ESP_OK)
        {
            ctx->battery_level_pct = ina219_voltage_to_pct(voltage);
            ESP_LOGI(TAG, "Batería real: %.3fV -> %.1f%%", voltage, ctx->battery_level_pct);
        }
        else
        {
            // Si falla la lectura, mantenemos el último valor conocido
            ESP_LOGW(TAG, "Fallo en la lectura INA219, usando último valor: %.1f%%", ctx->battery_level_pct);
        }

        // Actualizar modo energético según nivel de batería real
        power_mode_t prev_mode = ctx->power_mode;

        if      (ctx->battery_level_pct >= 50.0f) ctx->power_mode = POWER_MODE_NORMAL;
        else if (ctx->battery_level_pct >= 20.0f) ctx->power_mode = POWER_MODE_LOW;
        else                                      ctx->power_mode = POWER_MODE_CRITICAL;

        if (ctx->power_mode != prev_mode)
        {
            ESP_LOGW(TAG, "Cambio de modo energético: %s -> %s (batería=%.1f%%)", 
                    power_mode_to_str(prev_mode),
                    power_mode_to_str(ctx->power_mode),
                    ctx->battery_level_pct);
        }
    #endif
}

const char *power_mode_to_str(power_mode_t mode)
{
    switch(mode)
    {
        case POWER_MODE_NORMAL:   return "NORMAL";
        case POWER_MODE_LOW:      return "LOW";
        case POWER_MODE_CRITICAL: return "CRITICAL";
        default:                  return "UNKNOWN";
    }
}

uint32_t power_get_sleep_interval_ms(const system_ctx_t *ctx, uint32_t default_ms)
{
    if (!ctx)
    {
        ESP_LOGW(TAG, "power_get_sleep_interval_ms(): ctx es NULL, usando valor por defecto");
        return default_ms;
    }

    switch (ctx->power_mode)
    {
        case POWER_MODE_NORMAL:   return default_ms;
        case POWER_MODE_LOW:      return default_ms * 2U;
        case POWER_MODE_CRITICAL: return default_ms * 4U;
        default:                  return default_ms;
    }
}