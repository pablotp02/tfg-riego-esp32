#include "fsm.h"
#include "config.h"
#include "device_config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "sensors.h"
#include "actuators.h"
#include "logger.h"
#include "power.h"
#include "ina219.h"

#include "wifi.h"
#include "http_client.h"

// Rangos válidos (provisionales) para validación
#define SOIL_MIN_PCT   (0.0f)
#define SOIL_MAX_PCT   (100.0f)
#define TEMP_MIN_C     (-10.0f)
#define TEMP_MAX_C     (60.0f)
#define HUM_MIN_PCT    (0.0f)
#define HUM_MAX_PCT    (100.0f)
#define PH_MIN         (0.0f)
#define PH_MAX         (14.0f)
#define EC_MIN_US_CM   (0.0f)
#define EC_MAX_US_CM   (20000.0f)

static const char *TAG = "TFG_RIEGO";

// La validación se centra en las variables del suelo obtenidas por el SEN0604
// Se comprueba disponibilidad de lectura, humedad de suelo y pH y EC
static bool validate_sensor_data(const sensor_data_t *d, const char **reason_out)
{
    const system_config_t *cfg = config_get();

    // Si estamos usando RS485 pero aún no hay ninguna lectura válida del SEN0604,
    // no debemos interpretar los valores de suelo como datos reales
    if (cfg->use_rs485_sensor && !sensors_have_sen0604_data())
    {
        if (reason_out) *reason_out = "Lectura de suelo no disponible";
        return false;
    }

    if (d->soil_moisture_pct < SOIL_MIN_PCT || d->soil_moisture_pct > SOIL_MAX_PCT)
    {
        if (reason_out) *reason_out = "Humedad de suelo fuera de rango";
        return false;
    }

    // Validación adicional solo si ya hay datos válidos del SEN0604
    if (sensors_have_sen0604_data())
    {
        float ph = sensors_get_last_ph();
        float ec = sensors_get_last_ec();

        if (ph < PH_MIN || ph > PH_MAX)
        {
            if (reason_out) *reason_out = "pH fuera de rango (dato inválido)";
            return false;
        }

        if (ec < EC_MIN_US_CM || ec > EC_MAX_US_CM)
        {
            if (reason_out) *reason_out = "EC fuera de rango (dato inválido)";
            return false;
        }
    }

    if (reason_out) *reason_out = NULL;
    return true;
}

// Ahora recibe soil_min_temp_c como parámetro (viene de ctx->device_cfg)
// en lugar de usar la constante fija SOIL_MIN_TEMP_C de config.h
static bool decide_irrigation(const sensor_data_t *d,
                                 float soil_start_pct,
                                 float soil_stop_pct,
                                 float soil_min_temp_c,
                                 bool was_irrigating,
                                 const char **reason_out)
{
    // Regla 0: temperatura de suelo mínima
    // Si la temperatura del suelo es demasiado baja, no regar
    // independientemente de la humedad (riesgo daño a las raíces)
    if (d->soil_temp_c < soil_min_temp_c)
    {
        if (reason_out) *reason_out = "temperatura del suelo demasiado baja";
        return false;
    }

    // Regla 1: histéresis por humedad de suelo
    if (d->soil_moisture_pct < soil_start_pct)
    {
        if (reason_out) *reason_out = "suelo por debajo del umbral de arranque";
        return true;
    }

    if (d->soil_moisture_pct > soil_stop_pct)
    {
        if (reason_out) *reason_out = "suelo por encima del umbral de parada";
        return false;
    }

    // Zona intermedia: mantener estado anterior
    if (reason_out) *reason_out = "zona de histéresis: mantener estado previo";
    return was_irrigating;
}

// Comprueba si el cooldown de riego está activo.
// Devuelve true si hay que bloquear el riego, false si se puede regar.
// Usa ctx->device_cfg.irrigation_cooldown_cycles (configurable remotamente)
// en lugar de la constante fija IRRIGATION_COOLDOWN_CYCLES.
static bool irrigation_cooldown_active(const system_ctx_t *ctx)
{
    return (ctx->cycles_since_irrigated < ctx->device_cfg.irrigation_cooldown_cycles);
}

void fsm_init(system_ctx_t *ctx)
{
    *ctx = (system_ctx_t){
        .state = STATE_INIT,
        .irrigate_request = false,
        .cycle_count = 0,
        .cycles_since_measure = 0,
        .cycles_since_send = 0,
        .pending_send = false,
        .sensor_valid = true,
        .error_reason = NULL,
        .sensor_error_count = 0,
        .sensor_read_error_count = 0,

        .battery_level_pct = 100.0f,
        .power_mode = POWER_MODE_NORMAL,

        // Sin sincronización todavía en este arranque
        .device_cfg_synced = false
    };

    // Configuración por defecto como último recurso. Si hay algo válido
    // en RTC, power_restore_ctx_from_rtc() lo sobreescribirá justo después.
    device_config_get_defaults(&ctx->device_cfg);

    // Arranca con el cooldown cumplido para que el primer ciclo pueda
    // regar si las condiciones lo requieren
    ctx->cycles_since_irrigated = ctx->device_cfg.irrigation_cooldown_cycles;

    ESP_LOGI(TAG, "TFG Riego - arranque OK (ESP-IDF).");
}

void fsm_step(system_ctx_t *ctx)
{
    const system_config_t *cfg = config_get();

    switch (ctx->state)
    {
    case STATE_INIT:
        ESP_LOGI(TAG, "[INIT] Inicializando sistema...");

        if (!sensors_init())
        {
            ctx->sensor_valid = false;
            ctx->error_reason = "Fallo en sensores_init()";
            ctx->state = STATE_ERROR;
            break;
        }

        #if !ENABLE_BATTERY_SIMULATION
            esp_err_t ina_err = ina219_init();
            if (ina_err != ESP_OK)
            {
                ESP_LOGW(TAG, "[INIT]: INA219A no disponible: %s - se usará último valor conocido",
                        esp_err_to_name(ina_err));
            }
        #endif

        // Sincronización de configuración remota:
        // Intentamos conectar WiFi y pedir la configuración activa al backend.
        // Si no hay conexión o falla la petición, se mantiene la configuración
        // restaurada desde RTC (o los valores por defecto si nunca hubo
        // sincronización previa, ya establecidos en fsm_init).
        ctx->device_cfg_synced = false;

        esp_err_t wifi_cfg_err = wifi_connect();
        if (wifi_cfg_err == ESP_OK)
        {
            vTaskDelay(pdMS_TO_TICKS(2000)); // pequeño margen tras conectar antes de la petición HTTP

            device_config_t fetched_cfg;
            esp_err_t cfg_err = device_config_fetch(&fetched_cfg);
            if (cfg_err == ESP_OK)
            {
                ctx->device_cfg = fetched_cfg;
                ctx->device_cfg_synced = true;
                ESP_LOGI(TAG, "[INIT] Configuración sincronizada con el backend");
            }
            else
            {
                ESP_LOGW(TAG, "[INIT] Fallo al pedir configuración, se usa la última conocida");
            }
            wifi_disconnect();
        }
        else
        {
            ESP_LOGW(TAG, "[INIT] Sin WiFi, se usa configuración local (RTC o por defecto)");
        }

        ESP_LOGI(TAG,
                "[CFG] period=%lums irrig=%lums measure_n=%lu send_n=%lu soil_start=%.1f%% soil_stop=%.1f%% min_temp=%.1fC cooldown=%lu (synced=%s)",
                (unsigned long)ctx->device_cfg.measure_period_ms,
                (unsigned long)ctx->device_cfg.irrigate_time_ms,
                (unsigned long)cfg->measure_every_n_cycles,
                (unsigned long)cfg->send_every_n_cycles,
                ctx->device_cfg.soil_start_irrigation_pct,
                ctx->device_cfg.soil_stop_irrigation_pct,
                ctx->device_cfg.soil_min_temp_c,
                (unsigned long)ctx->device_cfg.irrigation_cooldown_cycles,
                ctx->device_cfg_synced ? "SI" : "NO");

        // Actualizamos batería simulada y modo energético al inicio de cada ciclo
        power_update_battery_and_mode(ctx);

        ESP_LOGI(TAG, "[POWER] battery=%.1f%% | mode=%s",
                    ctx->battery_level_pct,
                    power_mode_to_str(ctx->power_mode));

        ctx->state = STATE_SCHEDULE;
        break;

    case STATE_SCHEDULE:
    {
        ctx->cycles_since_measure++;
        ctx->cycles_since_send++;

        // Frecuencia de envío adaptada al modo energético
        uint32_t send_every_n = cfg->send_every_n_cycles;

        // En modo LOW se reduce la frecuencia de envío
        if (ctx->power_mode == POWER_MODE_LOW)
        {
            send_every_n = 5;
        }

        // En modo crítico no se realizan envíos para ahorrar energía
        if (ctx->power_mode == POWER_MODE_CRITICAL)
        {
            ctx->pending_send = false;
        }
        else if (ctx->cycles_since_send >= send_every_n)
        {
            ctx->cycles_since_send = 0;
            ctx->pending_send = true;
        }

        if (ctx->cycles_since_measure >= cfg->measure_every_n_cycles)
        {
            ctx->cycles_since_measure = 0;
            ctx->state = STATE_MEASURE;
        }
        else if (ctx->pending_send)
        {
            ctx->state = STATE_SEND;
        }
        else
        {
            ctx->state = STATE_SLEEP;
        }

        ESP_LOGI(TAG, "[SCHEDULE] ciclos: desde_measure=%lu, desde_send=%lu, pending_send=%s | mode=%s | send_every=%lu",
                (unsigned long)ctx->cycles_since_measure,
                (unsigned long)ctx->cycles_since_send,
                ctx->pending_send ? "SI" : "NO",
                power_mode_to_str(ctx->power_mode),
                (unsigned long)send_every_n);

        break;
    }

    case STATE_MEASURE:
        ctx->cycle_count++;
        ESP_LOGI(TAG, "[MEASURE] Tomando medidas...");
        ctx->last = sensors_read();

        if (cfg->use_rs485_sensor)
        {
            if (sensors_last_rs485_read_ok())
            {
                ctx->sensor_read_error_count = 0;
            }
            else
            {
                ctx->sensor_read_error_count++;
            }
        }
        else
        {
            ctx->sensor_read_error_count = 0;
        }

        ESP_LOGI(TAG, "[MEASURE] suelo=%.1f%% | temp=%.1fC",
                 ctx->last.soil_moisture_pct,
                 ctx->last.soil_temp_c);

        ctx->state = STATE_VALIDATE;
        break;

    case STATE_VALIDATE:
        ctx->sensor_valid = validate_sensor_data(&ctx->last, &ctx->error_reason);
        if (ctx->sensor_valid)
        {
            ctx->sensor_error_count = 0;
            ESP_LOGI(TAG, "[VALIDATE] Datos OK.");
            ctx->state = STATE_DECIDE;
        }
        else
        {
            ctx->sensor_error_count++;
            ESP_LOGW(TAG, "[VALIDATE] Datos INVALIDOS: %s (errores seguidos: %lu)",
                 ctx->error_reason, (unsigned long)ctx->sensor_error_count);
            ctx->state = STATE_ERROR;
        }
        break;

    case STATE_DECIDE:
    {
        const char *reason = NULL;

        ctx->irrigate_request = decide_irrigation(&ctx->last,
                                                ctx->device_cfg.soil_start_irrigation_pct,
                                                ctx->device_cfg.soil_stop_irrigation_pct,
                                                ctx->device_cfg.soil_min_temp_c,
                                                ctx->irrigate_request,
                                                &reason);

        // Bloqueo por cooldown: no regar si aún no han pasado suficientes ciclos
        // desde el último riego.
        if (ctx->irrigate_request && irrigation_cooldown_active(ctx))
        {
            ctx->irrigate_request = false;
            reason = "bloqueo: cooldown activo";
            ESP_LOGW(TAG, "[DECIDE] Riego bloqueado por cooldown (%lu/%lu ciclos)",
                    (unsigned long)ctx->cycles_since_irrigated,
                    (unsigned long)ctx->device_cfg.irrigation_cooldown_cycles);
        }

        // En modo energético crítico, el riego queda bloqueado para priorizar la autonomía
        if (ctx->power_mode == POWER_MODE_CRITICAL && ctx->irrigate_request)
        {
            ctx->irrigate_request = false;
            reason = "bloqueo: modo energético crítico";
        }

        ESP_LOGI(TAG,
                    "[DECIDE]: suelo_start=%.1f%% | suelo_stop=%.1f%% | suelo=%.1f%% | temp=%.1fC | mode=%s | cooldown=%lu/%lu -> riego=%s (%s)",
                    ctx->device_cfg.soil_start_irrigation_pct,
                    ctx->device_cfg.soil_stop_irrigation_pct,
                    ctx->last.soil_moisture_pct,
                    ctx->last.soil_temp_c,
                    power_mode_to_str(ctx->power_mode),
                    (unsigned long)ctx->cycles_since_irrigated,
                    (unsigned long)ctx->device_cfg.irrigation_cooldown_cycles,
                    ctx->irrigate_request ? "SI" : "NO",
                    reason ? reason : "sin motivo");

        ctx->state = ctx->irrigate_request ? STATE_IRRIGATE : STATE_LOG;
        break;
    }

    case STATE_IRRIGATE:
        actuators_irrigate(ctx->device_cfg.irrigate_time_ms);
        // Resetear cooldown: el contador vuelve a 0 tras cada riego
        ctx->cycles_since_irrigated = 0;
        ESP_LOGI(TAG, "[IRRIGATE] Cooldown reseteado. Próximo riego en %lu ciclos mínimo.",
                (unsigned long)ctx->device_cfg.irrigation_cooldown_cycles);
        ctx->state = STATE_LOG;
        break;

    case STATE_LOG:
        logger_log_cycle(ctx);
        if (ctx->pending_send) {
            ctx->state = STATE_SEND;
        } else {
            ctx->state = STATE_SLEEP;
        }
        break;

    case STATE_SEND:
    {
        bool have_soil_extra = sensors_have_sen0604_data();
        float ph = sensors_get_last_ph();
        float ec = sensors_get_last_ec();

        if (have_soil_extra)
        {
            ESP_LOGI(TAG,
                    "[SEND] payload -> suelo=%.1f%% | pH=%.1f | EC=%.0f | riego=%s | valid=%s | err_val=%lu | err_read=%lu | cooldown=%lu/%lu",
                    ctx->last.soil_moisture_pct,
                    ph,
                    ec,
                    ctx->irrigate_request ? "SI" : "NO",
                    ctx->sensor_valid ? "SI" : "NO",
                    (unsigned long)ctx->sensor_error_count,
                    (unsigned long)ctx->sensor_read_error_count,
                    (unsigned long)ctx->cycles_since_irrigated,
                    (unsigned long)ctx->device_cfg.irrigation_cooldown_cycles);
        }
        else
        {
            ESP_LOGI(TAG,
                    "[SEND] payload -> suelo=%.1f%% | pH=N/A | EC=N/A | riego=%s | valid=%s | err_val=%lu | err_read=%lu | cooldown=%lu/%lu",
                    ctx->last.soil_moisture_pct,
                    ctx->irrigate_request ? "SI" : "NO",
                    ctx->sensor_valid ? "SI" : "NO",
                    (unsigned long)ctx->sensor_error_count,
                    (unsigned long)ctx->sensor_read_error_count,
                    (unsigned long)ctx->cycles_since_irrigated,
                    (unsigned long)ctx->device_cfg.irrigation_cooldown_cycles);
        }

        // Conectar WiFi y enviar el payload al backend.
        // Si falla la conexión o el envío, el sistema continúa sin
        // bloquearse - se reintentará en el siguiente ciclo con pending_send.
        esp_err_t wifi_err = wifi_connect();
        if (wifi_err == ESP_OK)
        {
            esp_err_t send_err = http_send_cycle(ctx);
            if (send_err == ESP_OK)
            {
                ESP_LOGI(TAG, "[SEND] Datos enviados correctamente al backend");
            }
            else
            {
                ESP_LOGW(TAG, "[SEND] Fallo al enviar datos al backend, se reintentará en el próximo ciclo con envío pendiente");
            }
            wifi_disconnect();
        }
        else
        {
            ESP_LOGW(TAG, "[SEND] Sin conexión WiFi, se reintentará en el próximo ciclo con envío pendiente");
        }

        vTaskDelay(pdMS_TO_TICKS(300));
        ctx->pending_send = false;
        ctx->state = STATE_SLEEP;
        break;
    }

    case STATE_SLEEP:
    {
        uint32_t sleep_ms = power_get_sleep_interval_ms(ctx, ctx->device_cfg.measure_period_ms);

        // Incrementar cooldown antes de dormir, para que cada deep sleep
        // cuente como un ciclo transcurrido desde el último riego
        if (ctx->cycles_since_irrigated < ctx->device_cfg.irrigation_cooldown_cycles)
        {
            ctx->cycles_since_irrigated++;
            ESP_LOGI(TAG, "[SLEEP] Cooldown: %lu/%lu ciclos",
                    (unsigned long)ctx->cycles_since_irrigated,
                    (unsigned long)ctx->device_cfg.irrigation_cooldown_cycles);
        }

        ESP_LOGI(TAG, "[SLEEP] Entrando en deep sleep (%lu ms) | battery=%.1f%% | mode=%s",
                (unsigned long)sleep_ms,
                ctx->battery_level_pct,
                power_mode_to_str(ctx->power_mode));

        // Guardamos en RTC los campos persistentes antes de dormir
        power_store_ctx_to_rtc(ctx);

        // Entramos en deep sleep con wakeup por timer
        power_enter_deep_sleep(sleep_ms);

        // Si todo va bien, no debería volver nunca de esta función
        ESP_LOGW(TAG, "[SLEEP] Retorno inesperado desde power_enter_deep_sleep()");
        break;
    }

    case STATE_ERROR:
        ESP_LOGE(TAG, "[ERROR] %s", ctx->error_reason ? ctx->error_reason : "Error desconocido");
        ctx->irrigate_request = false;
        ctx->state = STATE_LOG;
        break;

    default:
        ESP_LOGW(TAG, "[WARN] Estado desconocido. Reiniciando FSM.");
        ctx->state = STATE_INIT;
        break;
    }
}