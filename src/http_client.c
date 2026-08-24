#include "http_client.h"
#include "credentials.h"
#include "sensors.h"
#include "power.h"
#include "config.h"

#include "esp_http_client.h"
#include "esp_log.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "HTTP_CLIENT";

// Buffer para la URL y el payload JSON
#define URL_BUFFER_SIZE     (128)
#define JSON_BUFFER_SIZE    (768)

// ─── Construcción del JSON del payload ────────────────────────
static int build_json_payload(const system_ctx_t *ctx, char *buf, size_t buf_size)
{
    const system_config_t *cfg = config_get();

    bool have_soil_extra = sensors_have_sen0604_data();
    float ph = have_soil_extra ? sensors_get_last_ph() : 0.0f;
    float ec = have_soil_extra ? sensors_get_last_ec() : 0.0f;

    // Motivo detallado de la decisión de riego, calculado en
    // decide_irrigation() y conservado en el contexto
    const char *irrigation_reason = ctx->irrigate_reason ?
            ctx->irrigate_reason : "sin motivo registrado";

    int written = snprintf(buf, buf_size,
        "{"
        "\"cycle_number\":%lu,"
        "\"soil_moisture\":%.1f,"
        "\"soil_temp\":%.1f,"
        "\"ph\":%.1f,"
        "\"ec\":%.1f,"
        "\"read_ok\":%s,"
        "\"validated\":%s,"
        "\"irrigated\":%s,"
        "\"duration_ms\":%lu,"
        "\"irrigation_reason\":\"%s\","
        "\"battery_pct\":%.1f,"
        "\"power_mode\":%d,"
        "\"sleep_ms\":%lu,"
        "\"wakeup_cause\":\"%s\","
        "\"cooldown_current\":%lu,"
        "\"cooldown_required\":%u"
        "}",
        (unsigned long)ctx->cycle_count,
        ctx->last.soil_moisture_pct,
        ctx->last.soil_temp_c,
        ph,
        ec,
        sensors_last_rs485_read_ok() ? "true" : "false",
        ctx->sensor_valid ? "true" : "false",
        ctx->irrigate_request ? "true" : "false",
        ctx->irrigate_request ? (unsigned long)cfg->irrigate_time_ms : 0UL,
        irrigation_reason,
        ctx->battery_level_pct,
        (int)ctx->power_mode,
        (unsigned long)cfg->measure_period_ms,
        power_get_wakeup_cause_str(),
        (unsigned long)ctx->cycles_since_irrigated,
        IRRIGATION_COOLDOWN_CYCLES
    );

    if (written < 0 || (size_t)written >= buf_size)
    {
        ESP_LOGE(TAG, "Payload JSON truncado o error de formato");
        return -1;
    }

    return written;
}

// ─── Construcción del JSON del payload de un ciclo pendiente ──
static int build_json_payload_pending(const pending_cycle_data_t *p, char *buf, size_t buf_size)
{
    const char *irrigation_reason = p->irrigate_reason ?
            p->irrigate_reason : "sin motivo registrado";

    int written = snprintf(buf, buf_size,
        "{"
        "\"cycle_number\":%lu,"
        "\"soil_moisture\":%.1f,"
        "\"soil_temp\":%.1f,"
        "\"ph\":%.1f,"
        "\"ec\":%.1f,"
        "\"read_ok\":%s,"
        "\"validated\":%s,"
        "\"irrigated\":%s,"
        "\"duration_ms\":%lu,"
        "\"irrigation_reason\":\"%s\","
        "\"battery_pct\":%.1f,"
        "\"power_mode\":%d,"
        "\"sleep_ms\":%lu,"
        "\"wakeup_cause\":\"%s\","
        "\"cooldown_current\":%lu,"
        "\"cooldown_required\":%lu"
        "}",
        (unsigned long)p->cycle_number,
        p->last.soil_moisture_pct,
        p->last.soil_temp_c,
        p->ph,
        p->ec,
        "true", // el propio hecho de haber llegado a intentar el envío implica que la lectura fue correcta
        "true", // idem, ya pasó por STATE_VALIDATE en su momento
        p->irrigate_request ? "true" : "false",
        (unsigned long)p->duration_ms,
        irrigation_reason,
        p->battery_level_pct,
        (int)p->power_mode,
        (unsigned long)p->sleep_ms,
        "PENDING_SEND_RETRY", // no se conserva la causa de despertar original
        (unsigned long)p->cycles_since_irrigated,
        (unsigned long)p->irrigation_cooldown_cycles
    );

    if (written < 0 || (size_t)written >= buf_size)
    {
        ESP_LOGE(TAG, "Payload JSON (pendiente) truncado o error de formato");
        return -1;
    }

    return written;
}

// ─── Envío HTTP POST ───────────────────────────────────────────
esp_err_t http_send_cycle(const system_ctx_t *ctx)
{
    char url[URL_BUFFER_SIZE];
    char json_payload[JSON_BUFFER_SIZE];

    snprintf(url, sizeof(url), "http://%s:%d/api/cycles/", BACKEND_IP, BACKEND_PORT);

    int json_len = build_json_payload(ctx, json_payload, sizeof(json_payload));
    if (json_len < 0)
    {
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Enviando a %s", url);
    ESP_LOGI(TAG, "Payload: %s", json_payload);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
    {
        ESP_LOGE(TAG, "Error inicializando cliente HTTP");
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_payload, json_len);

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK)
    {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "POST completado, código HTTP: %d", status_code);

        if (status_code != 200)
        {
            ESP_LOGW(TAG, "Backend respondió con código no esperado: %d", status_code);
            err = ESP_FAIL;
        }
    }
    else
    {
        ESP_LOGE(TAG, "Error en petición HTTP: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}


// ─── Envío HTTP POST de un ciclo pendiente ─────────────────────
esp_err_t http_send_pending_cycle(const pending_cycle_data_t *pending)
{
    char url[URL_BUFFER_SIZE];
    char json_payload[JSON_BUFFER_SIZE];

    snprintf(url, sizeof(url), "http://%s:%d/api/cycles/", BACKEND_IP, BACKEND_PORT);

    int json_len = build_json_payload_pending(pending, json_payload, sizeof(json_payload));
    if (json_len < 0)
    {
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Enviando ciclo pendiente a %s", url);
    ESP_LOGI(TAG, "Payload: %s", json_payload);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
    {
        ESP_LOGE(TAG, "Error inicializando cliente HTTP");
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_payload, json_len);

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK)
    {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "POST completado, código HTTP: %d", status_code);

        if (status_code != 200)
        {
            ESP_LOGW(TAG, "Backend respondió con código no esperado: %d", status_code);
            err = ESP_FAIL;
        }
    }
    else
    {
        ESP_LOGE(TAG, "Error en petición HTTP: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}