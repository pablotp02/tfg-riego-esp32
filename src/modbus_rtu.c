#include "modbus_rtu.h"

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

bool modbus_validate_read_holding_resp(const uint8_t *resp, size_t len, uint8_t expected_slave, uint16_t expected_reg_count)
{
    // Respuesta mínima: addr(1)+func(1)+bytecount(1)+data(2*regs)+crc(2)
    size_t min_len = 1 + 1 + 1 + (2 * expected_reg_count) + 2;
    if (!resp || len < min_len) return false;

    if (resp[0] != expected_slave) return false;
    if (resp[1] != 0x03) return false;

    uint8_t byte_count = resp[2];
    if (byte_count != (uint8_t)(2 * expected_reg_count)) return false;

    // CRC: últimos 2 bytes
    uint16_t crc_recv = (uint16_t)resp[len-2] | ((uint16_t)resp[len-1] << 8);
    uint16_t crc_calc = modbus_crc16(resp, len - 2);
    return (crc_recv == crc_calc);
}