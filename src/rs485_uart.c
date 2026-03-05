#include "rs485_uart.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "RS485_UART";

esp_err_t rs485_uart_init(uart_port_t port, int tx_pin, int rx_pin, int baudrate)
{
    uart_config_t cfg = {
        .baud_rate  = baudrate,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT
    };

    esp_err_t err;

    err = uart_driver_install(port, 1024, 1024, 0, NULL, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "uart_driver_install failed (%s)", esp_err_to_name(err));
        return err;
    }

    err = uart_param_config(port, &cfg);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "uart_param_config failed (%s)", esp_err_to_name(err));
        return err;
    }

    err = uart_set_pin(port, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "uart_set_pin failed (%s)", esp_err_to_name(err));
        return err;
    }

    // Limpiamos buffer RX por si acaso
    uart_flush_input(port);

    ESP_LOGI(TAG, "UART init OK: port=%d tx=%d rx=%d baud=%d", (int)port, tx_pin, rx_pin, baudrate);
    return ESP_OK;
}

esp_err_t rs485_uart_write(uart_port_t port, const uint8_t *data, size_t len)
{
    if (!data || len == 0) return ESP_ERR_INVALID_ARG;

    int written = uart_write_bytes(port, (const char *)data, len);
    if (written < 0 || (size_t)written != len) 
    {
        return ESP_FAIL;
    }
    return uart_wait_tx_done(port, pdMS_TO_TICKS(1000));
}

esp_err_t rs485_uart_read(uart_port_t port, uint8_t *buf, size_t max_len, uint32_t timeout_ms, size_t *out_len)
{
    if (!buf || max_len == 0 || !out_len) return ESP_ERR_INVALID_ARG;

    int n = uart_read_bytes(port, buf, max_len, pdMS_TO_TICKS(timeout_ms));
    if (n < 0) return ESP_FAIL;

    *out_len = (size_t)n;
    return ESP_OK;
}