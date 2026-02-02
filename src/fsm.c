#include "fsm.h"
#include "config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "sensors.h"
#include "actuators.h"
#include "logger.h"

// Rangos válidos (provisionales) para validación
#define SOIL_MIN_PCT   (0.0f)
#define SOIL_MAX_PCT   (100.0f)
#define TEMP_MIN_C     (-10.0f)
#define TEMP_MAX_C     (60.0f)
#define HUM_MIN_PCT    (0.0f)
#define HUM_MAX_PCT    (100.0f)

static const char *TAG = "TFG_RIEGO";

static bool validate_sensor_data(const sensor_data_t *d, const char **reason_out)
{
    if (d->soil_moisture_pct < SOIL_MIN_PCT || d->soil_moisture_pct > SOIL_MAX_PCT) {
        if (reason_out) *reason_out = "Humedad de suelo fuera de rango";
        return false;
    }
    if (d->temperature_c < TEMP_MIN_C || d->temperature_c > TEMP_MAX_C) {
        if (reason_out) *reason_out = "Temperatura fuera de rango";
        return false;
    }
    if (d->humidity_pct < HUM_MIN_PCT || d->humidity_pct > HUM_MAX_PCT) {
        if (reason_out) *reason_out = "Humedad ambiente fuera de rango";
        return false;
    }
    if (reason_out) *reason_out = NULL;
    return true;
}

static bool decide_irrigation(const sensor_data_t *d, float soil_threshold_pct, const char **reason_out)
{
    // Regla 1: suelo seco
    if (d->soil_moisture_pct >= soil_threshold_pct)
    {
        if (reason_out) *reason_out = "suelo por encima del umbral";
        return false;
    }

    // Regla 2 (provisional): bloqueo por temperatura baja
    // Evita regar con ambiente frío (criterio prudente hasta que se defina la planta con la que trabajar)
    if (d->temperature_c < 5.0f)
    {
        if (reason_out) *reason_out = "bloqueo: temperatura baja";
        return false;
    }
    
    // Regla 3 (provisional): bloqueo por humedad ambiente muy alta
    // Evita regar  en condiciones cercanas a saturación
    if (d->humidity_pct > 95.0f)
    {
        if (reason_out) *reason_out = "bloqueo: humedad ambiente alta";
        return false;
    }

    if (reason_out) *reason_out = "suelo seco y condiciones OK";
    return true;
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
        .error_reason = NULL
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
        
        ESP_LOGI(TAG, "[CFG] period=%lums irrig=%lums measure_n=%lu send_n=%lu soil_thr=%.1f%%",
            (unsigned long)cfg->measure_period_ms,
            (unsigned long)cfg->irrigate_time_ms,
            (unsigned long)cfg->measure_every_n_cycles,
            (unsigned long)cfg->send_every_n_cycles,
            cfg->soil_threshold_pct);

        if (!sensors_init())
        {
            ctx->sensor_valid = false;
            ctx->error_reason = "Fallo en sensores_init()";
            ctx->state = STATE_ERROR;
            break;
        }

        // Aquí iría: NVS, config, GPIO, etc.
        ctx->state = STATE_SCHEDULE;
        break;

    case STATE_SCHEDULE:
        ctx->cycles_since_measure++;
        ctx->cycles_since_send++;

        ESP_LOGI(TAG, "[SCHEDULE] ciclos: desde_measure=%lu, desde_send=%lu, pending_send=%s",
                 (unsigned long)ctx->cycles_since_measure,
                 (unsigned long)ctx->cycles_since_send,
                 ctx->pending_send ? "SI" : "NO");

        if (ctx->cycles_since_send >= cfg->send_every_n_cycles) {
            ctx->cycles_since_send = 0;
            ctx->pending_send = true;
        }

        if (ctx->cycles_since_measure >= cfg->measure_every_n_cycles) {
            ctx->cycles_since_measure = 0;
            ctx->state = STATE_MEASURE;
        } else if (ctx->pending_send) {
            ctx->state = STATE_SEND;
        } else {
            ctx->state = STATE_SLEEP;
        }
        break;

    case STATE_MEASURE:
        ctx->cycle_count++;
        ESP_LOGI(TAG, "[MEASURE] Tomando medidas...");
        ctx->last = sensors_read();
        ESP_LOGI(TAG, "[MEASURE] suelo=%.1f%% | temp=%.1fC | hum=%.1f%%",
                 ctx->last.soil_moisture_pct,
                 ctx->last.temperature_c,
                 ctx->last.humidity_pct);
        ctx->state = STATE_VALIDATE;
        break;

    case STATE_VALIDATE:
        ctx->sensor_valid = validate_sensor_data(&ctx->last, &ctx->error_reason);
        if (ctx->sensor_valid) {
            ESP_LOGI(TAG, "[VALIDATE] Datos OK.");
            ctx->state = STATE_DECIDE;
        } else {
            ESP_LOGW(TAG, "[VALIDATE] Datos INVALIDOS: %s", ctx->error_reason);
            ctx->state = STATE_ERROR;
        }
        break;

    case STATE_DECIDE:
    {
        const char *reason = NULL;

        ctx->irrigate_request = decide_irrigation(&ctx->last, cfg->soil_threshold_pct, &reason);

        ESP_LOGI(TAG, "[DECIDE] suelo_thr=%.1f%% | T=%.1fC | H=%.1f%% -> riego=%s (%s)",
                    cfg->soil_threshold_pct,
                    ctx->last.temperature_c,
                    ctx->last.humidity_pct,
                    ctx->irrigate_request ? "SI" : "NO",
                    reason ? reason : "sin motivo"); 

        ctx->state = ctx->irrigate_request ? STATE_IRRIGATE : STATE_LOG;
        break;
    } // ponemos corchetes en el case porque dentro hemos declarado una variable
        

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
        ESP_LOGI(TAG, "[SEND] Envío de datos (placeholder). En el futuro: WiFi + API/BD.");
        vTaskDelay(pdMS_TO_TICKS(300));
        ctx->pending_send = false;
        ctx->state = STATE_SLEEP;
        break;

    case STATE_SLEEP:
        ESP_LOGI(TAG, "[SLEEP] Esperando siguiente ciclo (%lu ms)...",
                (unsigned long)cfg->measure_period_ms);
        vTaskDelay(pdMS_TO_TICKS(cfg->measure_period_ms));
        ctx->state = STATE_SCHEDULE;
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
