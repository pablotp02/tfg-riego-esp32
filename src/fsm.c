#include "fsm.h"
#include "config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "sensors.h"
#include "actuators.h"
#include "logger.h"
#include "power.h"

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

    // temperature_c representa temperatura del suelo
    if (d->temperature_c < TEMP_MIN_C || d->temperature_c > TEMP_MAX_C) 
    {
        if (reason_out) *reason_out = "Temperatura de suelo fuera de rango";
        return false;
    }

    // humidity_pct representa humedad ambiente
    if (d->humidity_pct < HUM_MIN_PCT || d->humidity_pct > HUM_MAX_PCT) 
    {
        if (reason_out) *reason_out = "Humedad ambiente fuera de rango";
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

static bool decide_irrigation(const sensor_data_t *d,
                                 float soil_start_pct,
                                 float soil_stop_pct,
                                 bool was_irrigating,
                                 const char **reason_out)
{
    // Regla 1: bloqueo por temperatura de suelo baja
    if (d->temperature_c < SOIL_MIN_TEMP_C)
    {
        if (reason_out) *reason_out = "bloqueo: temperatura de suelo baja";
        return false;
    }

    // Regla 2: bloqueo por humedad ambiente muy alta
    if (d->humidity_pct > AIR_MAX_HUM_PCT)
    {
        if (reason_out) *reason_out = "bloqueo: humedad ambiente alta";
        return false;
    }
    
    // Regla 3: histéresis por humedad de suelo
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
        .power_mode = POWER_MODE_NORMAL
    };

    ESP_LOGI(TAG, "TFG Riego - arranque OK (ESP-IDF).");
}

void fsm_step(system_ctx_t *ctx)
{
    const system_config_t *cfg = config_get();

    switch (ctx->state)
    {
    case STATE_INIT:
        ESP_LOGI(TAG, "[INIT] Inicializando sistema...");
        
        ESP_LOGI(TAG, "[CFG] period=%lums irrig=%lums measure_n=%lu send_n=%lu soil_start=%.1f%% soil_stop=%.1f%%",
            (unsigned long)cfg->measure_period_ms,
            (unsigned long)cfg->irrigate_time_ms,
            (unsigned long)cfg->measure_every_n_cycles,
            (unsigned long)cfg->send_every_n_cycles,
            cfg->soil_start_irrigation_pct,
            cfg->soil_stop_irrigation_pct);

        if (!sensors_init())
        {
            ctx->sensor_valid = false;
            ctx->error_reason = "Fallo en sensores_init()";
            ctx->state = STATE_ERROR;
            break;
        }

        // Actualizamos batería simulada y modo energético al inicio de cada ciclo
        power_update_battery_and_mode(ctx);

        ESP_LOGI(TAG, "[POWER] battery=%.1f%% | mode=%s",
                    ctx->battery_level_pct,
                    power_mode_to_str(ctx->power_mode));

        ctx->state = STATE_SCHEDULE;
        break;

    case STATE_SCHEDULE:
        ctx->cycles_since_measure++;
        ctx->cycles_since_send++;

        // En modo crítico no se realizan envíos para ahorrar energía
        if (ctx->power_mode == POWER_MODE_CRITICAL)
        {
            ctx->pending_send = false;
        }
        else if (ctx->cycles_since_send >= cfg->send_every_n_cycles)
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

        ESP_LOGI(TAG, "[SCHEDULE] ciclos: desde_measure=%lu, desde_send=%lu, pending_send=%s | mode=%s",
                (unsigned long)ctx->cycles_since_measure,
                (unsigned long)ctx->cycles_since_send,
                ctx->pending_send ? "SI" : "NO",
                power_mode_to_str(ctx->power_mode));
        
        break;

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
        else // si cambiamos de modo reseteamos contador
        {
            ctx->sensor_read_error_count = 0;
        }

        ESP_LOGI(TAG, "[MEASURE] suelo=%.1f%% | temp=%.1fC | hum=%.1f%%",
                    ctx->last.soil_moisture_pct,
                    ctx->last.temperature_c,
                    ctx->last.humidity_pct);
        
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
                                                cfg->soil_start_irrigation_pct,
                                                cfg->soil_stop_irrigation_pct,
                                                ctx->irrigate_request,
                                                &reason);

        // En modo energético crítico, el riego queda bloqueado para priorizar la autonomía
        if (ctx->power_mode == POWER_MODE_CRITICAL && ctx->irrigate_request)
        {
            ctx->irrigate_request = false;
            reason = "bloqueo: modo energético crítico";
        }

        ESP_LOGI(TAG,
                 "[DECIDE] suelo_start=%.1f%% | suelo_stop=%.1f%% | T=%.1fC | H=%.1f%% | mode=%s -> riego=%s (%s)",
                 cfg->soil_start_irrigation_pct,
                 cfg->soil_stop_irrigation_pct,
                 ctx->last.temperature_c,
                 ctx->last.humidity_pct,
                 power_mode_to_str(ctx->power_mode),
                 ctx->irrigate_request ? "SI" : "NO",
                 reason ? reason : "sin motivo");
        
        ctx->state = ctx->irrigate_request ? STATE_IRRIGATE : STATE_LOG;
        break;
    }

    case STATE_IRRIGATE:
        actuators_irrigate(cfg->irrigate_time_ms);
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
    {// las llaves dentro del case son porque hemos declarado variables locales (evita problemas de alcance)
        bool have_soil_extra = sensors_have_sen0604_data();
        float ph = sensors_get_last_ph();
        float ec = sensors_get_last_ec();

        if (have_soil_extra) 
        {
            ESP_LOGI(TAG, 
                    "[SEND] payload -> suelo=%.1f%% | temp=%.1fC | hum=%.1f%% | pH=%.1f | EC=%.0f | riego=%s | valid=%s | err_val=%lu | err_read=%lu",
                    ctx->last.soil_moisture_pct,
                    ctx->last.temperature_c,
                    ctx->last.humidity_pct,
                    ph,
                    ec,
                    ctx->irrigate_request ? "SI" : "NO",
                    ctx->sensor_valid ? "SI" : "NO", 
                    (unsigned long)ctx->sensor_error_count,
                    (unsigned long)ctx->sensor_read_error_count);
        }
        else
        {
            ESP_LOGI(TAG,
                    "[SEND] payload -> suelo=%.1f%% | temp=%.1fC | hum=%.1f%% | pH=N/A | EC=N/A | riego=%s | valid=%s | err_val=%lu | err_read=%lu",
                    ctx->last.soil_moisture_pct,
                    ctx->last.temperature_c,
                    ctx->last.humidity_pct,
                    ctx->irrigate_request ? "SI" : "NO",
                    ctx->sensor_valid ? "SI" : "NO",
                    (unsigned long)ctx->sensor_error_count,
                    (unsigned long)ctx->sensor_read_error_count);
        }

        vTaskDelay(pdMS_TO_TICKS(300));
        ctx->pending_send = false;
        ctx->state = STATE_SLEEP;
        break;
    }

    case STATE_SLEEP:
        uint32_t sleep_ms = power_get_sleep_interval_ms(ctx, cfg->measure_period_ms);

        ESP_LOGI(TAG, "[SLEEP] Entrando en deep sleep (%lu ms) | battery=%.1f%% | mode=%s",
                (unsigned long)sleep_ms,
                ctx->battery_level_pct,
                power_mode_to_str(ctx->power_mode));

        //Guardamos en RTC los campos persistentes antes de dormir
        power_store_ctx_to_rtc(ctx);

        // Entramos en deep sleep con wakeup por timer
        power_enter_deep_sleep(sleep_ms);

        // Si todo va bien, no debería volver nunca de esta función
        ESP_LOGW(TAG, "[SLEEP] Retorno inesperado desde power_enter_deep_sleep()");
        break;

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
