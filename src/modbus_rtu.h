#pragma once 
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MODBUS_OK = 0,
    MODBUS_ERR_NULL,
    MODBUS_ERR_LENGTH,
    MODBUS_ERR_SLAVE,
    MODBUS_ERR_FUNCTION,
    MODBUS_ERR_BYTECOUNT,
    MODBUS_ERR_CRC,
    MODBUS_ERR_EXCEPTION
} modbus_status_t;

// Función para convertir el enum a texto
const char *modbus_status_to_str(modbus_status_t st);

// Función para convertir modbus_status_t a esp_err_t
esp_err_t modbus_status_to_esp_err(modbus_status_t st);

// CRC16 Modbus (polinomio 0xA001, init 0xFFFF)
uint16_t modbus_crc16(const uint8_t *data, size_t len);

// Construye una petición Modbus RTU "Read Holding Registers" (0x03)
// out debe tener al menos 8 bytes
// Devuelve el número de bytes escritos (8) o 0 si es error
size_t modbus_build_read_holding(uint8_t slave_addr, uint16_t start_reg, uint16_t reg_count, uint8_t *out, size_t out_max);

// Valida una respuesta a 0x03 (CRC + formato básico)
// Devuelve true si es OK
modbus_status_t modbus_validate_read_holding_resp(const uint8_t *resp, size_t len, uint8_t expected_slave, uint16_t expected_reg_count);

#ifdef __cplusplus
}
#endif