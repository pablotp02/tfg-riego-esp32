#include "sen0604.h"
#include "config.h"
#include "rs485_uart.h"
#include "modbus_rtu.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "SEN0604";

esp_err_t sen0604_init(void)
{
    // Inicializa UART2 
    return rs485_uart_init(RS485_UART_PORT, RS485_UART_TX, RS485_UART_RX, RS485_BAUDRATE);
}

static int16_t u16_to_i16(uint16_t v)
{
    return (int16_t)v; // reinterpretación signed para temperaturas negativas
}

esp_err_t sen0604_read_all(uint8_t slave_addr, sen0604_data_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;

    // Request: leer 4 registros desde 0x0000 (moist, temp, ec, ph)
    uint8_t req[8];
    size_t req_len = modbus_build_read_holding(slave_addr, 0x0000, 4, req, sizeof(req));
    if (req_len == 0) return ESP_FAIL;

    // Limpiamos la salida
    memset(out, 0, sizeof(*out));

    // Enviar 
    esp_err_t err = rs485_uart_write(RS485_UART_PORT, req, req_len);
    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "UART write failed: %s", esp_err_to_name(err));
        return err;
    }

    // Recibir: respuesta esperada = 1+1+1+8+2 = 13 bytes
    uint8_t resp[32];
    size_t resp_len = 0;
    err = rs485_uart_read(RS485_UART_PORT, resp, sizeof(resp), 200, &resp_len);
    if (err != ESP_OK) return err;

    if (resp_len == 0)
    {
        // normal si no hay sensor conectado
        ESP_LOGW(TAG, "No response (sensor not connected yet?)");
        return ESP_ERR_TIMEOUT;
    }

    if (!modbus_validate_read_holding_resp(resp, resp_len, slave_addr, 4))
    {
        ESP_LOGW(TAG, "Invalid Modbus response (len=%u)", (unsigned)resp_len);
        return ESP_ERR_INVALID_CRC;
    }

    // Data empieza en resp[3], big endian por registro
    uint16_t r0 = ((uint16_t)resp[3] << 8) | resp[4]; // moist x10
    uint16_t r1 = ((uint16_t)resp[5] << 8) | resp[6]; // temp x10 (signed)
    uint16_t r2 = ((uint16_t)resp[7] << 8) | resp[8]; // EC us/cm
    uint16_t r3 = ((uint16_t)resp[9] << 8) | resp[10]; // pH x10

    out->soil_moisture_pct = r0 / 10.0f;
    out->soil_temp_c       = u16_to_i16(r1) / 10.0f;
    out->ec_us_cm          = (float)r2;
    out->ph                = r3 / 10.0f;

    ESP_LOGI(TAG, "SEN0604 OK: moist=%.1f%% temp=%.1fC EC=%.0f uS/cm pH=%.1f",
             out->soil_moisture_pct, out->soil_temp_c, out->ec_us_cm, out->ph);
    
    return ESP_OK;
}