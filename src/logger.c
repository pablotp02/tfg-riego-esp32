#include "logger.h"
#include "esp_log.h"

static const char *TAG = "TFG_RIEGO";

void logger_log_cycle(const system_ctx_t *ctx)
{
    ESP_LOGI(TAG,
             "[LOG] ciclo=%lu | suelo=%.1f%% | temp=%.1fC | hum=%.1f%% | valid=%s | riego=%s%s%s",
             (unsigned long)ctx->cycle_count,
             ctx->last.soil_moisture_pct,
             ctx->last.temperature_c,
             ctx->last.humidity_pct,
             ctx->sensor_valid ? "SI" : "NO",
             ctx->irrigate_request ? "SI" : "NO",
             ctx->sensor_valid ? "" : " | motivo=",
             ctx->sensor_valid ? "" : (ctx->error_reason ? ctx->error_reason : "desconocido"));
}
