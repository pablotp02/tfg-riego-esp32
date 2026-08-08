#include "device_config.h"
#include "credentials.h"
#include "config.h"

#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"

#include <string.h>
#include <stdlib.h>

static const char *TAG = "DEVICE_CONFIG";

#define URL_BUFFER_SIZE   (128)
#define RESPONSE_BUFFER_SIZE (512)

// Buffer donde acumulamos la respuesta HTTP
static char s_response_buffer[RESPONSE_BUFFER_SIZE];

// ─── Parseo del JSON recibido ──────────────────────────────────
static bool parse_config_json(const char *json_str, device_config_t *out_config)
{
    cJSON *root = cJSON_Parse(json_str);
    if (!root)
    {
        ESP_LOGE(TAG, "Error parseando JSON de configuración");
        return false;
    }

    cJSON *soil_start = cJSON_GetObjectItem(root, "soil_start_irrigation_pct");
    cJSON *soil_stop  = cJSON_GetObjectItem(root, "soil_stop_irrigation_pct");
    cJSON *min_temp   = cJSON_GetObjectItem(root, "soil_min_temp_c");
    cJSON *cooldown   = cJSON_GetObjectItem(root, "irrigation_cooldown_cycles");
    cJSON *period     = cJSON_GetObjectItem(root, "measure_period_ms");
    cJSON *irrig_time = cJSON_GetObjectItem(root, "irrigate_time_ms");

    if (!cJSON_IsNumber(soil_start) || !cJSON_IsNumber(soil_stop) ||
        !cJSON_IsNumber(min_temp)   || !cJSON_IsNumber(cooldown) ||
        !cJSON_IsNumber(period)     || !cJSON_IsNumber(irrig_time))
    {
        ESP_LOGE(TAG, "JSON de configuración incompleto o con tipos incorrectos");
        cJSON_Delete(root);
        return false;
    }

    out_config->soil_start_irrigation_pct = (float)soil_start->valuedouble;
    out_config->soil_stop_irrigation_pct  = (float)soil_stop->valuedouble;
    out_config->soil_min_temp_c           = (float)min_temp->valuedouble;
    out_config->irrigation_cooldown_cycles = (uint32_t)cooldown->valueint;
    out_config->measure_period_ms         = (uint32_t)period->valueint;
    out_config->irrigate_time_ms          = (uint32_t)irrig_time->valueint;

    cJSON_Delete(root);
    return true;
}

// ─── Petición HTTP GET al backend ──────────────────────────────
esp_err_t device_config_fetch(device_config_t *out_config)
{
    char url[URL_BUFFER_SIZE];
    snprintf(url, sizeof(url), "http://%s:%d/api/config/latest", BACKEND_IP, BACKEND_PORT);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client)
    {
        ESP_LOGE(TAG, "Error inicializando cliente HTTP");
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error abriendo conexión HTTP: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    int content_length = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);

    if (status_code != 200 || content_length <= 0 || content_length >= RESPONSE_BUFFER_SIZE)
    {
        ESP_LOGW(TAG, "Respuesta no válida: status=%d, content_length=%d", status_code, content_length);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    memset(s_response_buffer, 0, sizeof(s_response_buffer));
    int read_len = esp_http_client_read(client, s_response_buffer, content_length);

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (read_len <= 0)
    {
        ESP_LOGE(TAG, "Error leyendo cuerpo de la respuesta");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Configuración recibida: %s", s_response_buffer);

    if (!parse_config_json(s_response_buffer, out_config))
    {
        return ESP_FAIL;
    }

    return ESP_OK;
}

// ─── Valores por defecto (último recurso) ──────────────────────
void device_config_get_defaults(device_config_t *out_config)
{
    out_config->soil_start_irrigation_pct = 40.0f;
    out_config->soil_stop_irrigation_pct  = 50.0f;
    out_config->soil_min_temp_c           = SOIL_MIN_TEMP_C;
    out_config->irrigation_cooldown_cycles = IRRIGATION_COOLDOWN_CYCLES;
    out_config->measure_period_ms         = 15000;
    out_config->irrigate_time_ms          = 2000;

    ESP_LOGW(TAG, "Usando configuración por defecto (sin sincronización previa)");
}