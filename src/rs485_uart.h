#pragma once 
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "driver/uart.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t rs485_uart_init(uart_port_t port, int tx_pin, int rx_pin, int baudrate);

// Envía bytes por UART
esp_err_t rs485_uart_write(uart_port_t port, const uint8_t *data, size_t len);

// Lee hasta max_len bytes con timeout (ms). Devuelve el número de bytes leídos en out_len
esp_err_t rs485_uart_read(uart_port_t port, uint8_t *buf, size_t max_len, uint32_t timeout_ms, size_t *out_len);

#ifdef __cplusplus
}
#endif