#pragma once 
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// CRC16 Modbus (polinomio 0xA001, init 0xFFFF)
uint16_t modbus_crc16(const uint8_t *data, size_t len);

// Construye una petición Modbus RTU "Read Holding Registers" (0x03)
// out debe tener al menos 8 bytes
// Devuelve el número de bytes escritos (8) o 0 si es error
size_t modbus_build_read_holding(uint8_t slave_addr, uint16_t start_reg, uint16_t reg_count, uint8_t *out, size_t out_max);

// Valida una respuesta a 0x03 (CRC + formato básico)
// Devuelve true si es OK
bool modbus_validate_read_holding_res(const uint8_t *resp, size_t len, uint8_t expected_slave, uint16_t expected_reg_count);

#ifdef __cplusplus
}
#endif