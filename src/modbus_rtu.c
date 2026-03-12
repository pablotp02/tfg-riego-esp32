#include "modbus_rtu.h"

const char *modbus_status_to_str(modbus_status_t st)
{
    switch(st)
    {
        case MODBUS_OK:             return "MODBUS_OK";
        case MODBUS_ERR_NULL:       return "MODBUS_ERR_NULL";
        case MODBUS_ERR_LENGTH:     return "MODBUS_ERR_LENGTH";
        case MODBUS_ERR_SLAVE:      return "MODBUS_ERR_SLAVE";
        case MODBUS_ERR_FUNCTION:   return "MODBUS_ERR_FUNCTION";
        case MODBUS_ERR_BYTECOUNT:  return "MODBUS_ERR_BYTECOUNT";
        case MODBUS_ERR_CRC:        return "MODBUS_ERR_CRC";
        case MODBUS_ERR_EXCEPTION:  return "MODBUS_ERR_EXCEPTION";
        default:                    return "MODBUS_ERR_UNKNOWN";
    }
}

esp_err_t modbus_status_to_esp_err(modbus_status_t st)
{
    switch (st)
    {
        case MODBUS_OK:             return ESP_OK;
        case MODBUS_ERR_NULL:       return ESP_ERR_INVALID_ARG;
        case MODBUS_ERR_LENGTH:
        case MODBUS_ERR_BYTECOUNT:  return ESP_ERR_INVALID_SIZE;
        case MODBUS_ERR_SLAVE:
        case MODBUS_ERR_FUNCTION:
        case MODBUS_ERR_EXCEPTION:  return ESP_ERR_INVALID_RESPONSE;
        case MODBUS_ERR_CRC:        return ESP_ERR_INVALID_CRC;
        default:                    return ESP_FAIL;
    }
}

uint16_t modbus_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (int b = 0; b < 8; b++)
        {
            if (crc & 0x0001) crc = (crc >> 1) ^ 0xA001;
            else              crc = (crc >> 1);
        }
    }
    return crc;
}

size_t modbus_build_read_holding(uint8_t slave_addr, uint16_t start_reg, uint16_t reg_count, uint8_t *out, size_t out_max)
{
    if (!out || out_max < 8 || reg_count == 0) return 0;

    out[0] = slave_addr;
    out[1] = 0x03; // Read Holding Registers
    out[2] = (uint8_t)(start_reg >> 8);
    out[3] = (uint8_t)(start_reg & 0xFF);
    out[4] = (uint8_t)(reg_count >> 8);
    out[5] = (uint8_t)(reg_count & 0xFF);

    uint16_t crc = modbus_crc16(out, 6);
    out[6] = (uint8_t)(crc & 0xFF);       // CRC Lo
    out[7] = (uint8_t)((crc >>8) & 0xFF); // CRC Hi

    return 8;
}

modbus_status_t modbus_validate_read_holding_resp(const uint8_t *resp, size_t len, uint8_t expected_slave, uint16_t expected_reg_count)
{
    // Primero comprobamos que el puntero a la trama recibida existe
    if (!resp) return MODBUS_ERR_NULL; 

    // Caso 1: respuesta de excepción Modbus
    // Formato: addr(1) + func|0x80(1) + exception_code(1) + cod(2) = 5 bytes
    if (len == 5 && resp[0] == expected_slave && (resp[1] & 0x80))
    {
        // Calculamos CRC 
        uint16_t crc_recv = (uint16_t)resp[len - 2] | ((uint16_t)resp[len - 1] << 8);
        uint16_t crc_calc = modbus_crc16(resp, len - 2);

        // Si CRC incorrecto: trama corrupta
        if (crc_recv != crc_calc) return MODBUS_ERR_CRC;

        // CRC correcto: trama bien formada, pero el esclavo ha respondido con error
        return MODBUS_ERR_EXCEPTION;
    }

    // Caso 2: respuesta normal
    // addr(1) + func(1) + bytecount(1) + data(2*regs) + crc(2)
    size_t expected_len = 1 + 1 + 1 + (2 * expected_reg_count) + 2; // calculamos longitud de una respuesta normal
    if (len != expected_len) return MODBUS_ERR_LENGTH;

    // Validación de esclavo que responde
    if (resp[0] != expected_slave) return MODBUS_ERR_SLAVE; 

    // Validación de función, la respuesta debe ser de Read Holding Registers y no de otra
    if (resp[1] != 0x03) return MODBUS_ERR_FUNCTION; 

    // Validación byte_count
    uint8_t byte_count = resp[2];
    if (byte_count != (uint8_t)(2 * expected_reg_count)) return MODBUS_ERR_BYTECOUNT;

    // Validación CRC de la repsuesta normal
    uint16_t crc_recv = (uint16_t)resp[len-2] | ((uint16_t)resp[len-1] << 8); // reconstruimos CRC recibido
    uint16_t crc_calc = modbus_crc16(resp, len - 2); // recalculamos el CRC sin incluir los 2 bytes finales (parte útil de la trama)

    // Comprobación de integridad de trama
    if (crc_recv != crc_calc) return MODBUS_ERR_CRC;

    return MODBUS_OK; // la trama es válida
}