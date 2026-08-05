#include "ina219.h"
#include "driver/i2c.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "INA219";

// ─── Registros del INA219 ─────────────────────────────────────
#define REG_CONFIG      (0x00)  // registro de configuración
#define REG_BUS_VOLTAGE (0x02)  // registro de voltaje de bus

// Configuración:
// - Rango de voltaje de bus: 32V
// - Ganancia: /8 (320mV)
// - Resolución ADC bus: 12 bits
// - Resolución ADC shunt: 12 bits
// - Modo: continuo (shunt + bus)
#define INA219_CONFIG_VALUE (0x399F)

// ─── Inicialización I2C ───────────────────────────────────────
esp_err_t ina219_init(void)
{
    // Configurar el bus I2C en modo master
    i2c_config_t conf = {
        .mode             = I2C_MODE_MASTER,
        .sda_io_num       = INA219_SDA_GPIO,
        .scl_io_num       = INA219_SCL_GPIO,
        .sda_pullup_en    = GPIO_PULLUP_ENABLE,
        .scl_pullup_en    = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000  // 100 kHz
    };

    esp_err_t err = i2c_param_config(INA219_I2C_PORT, &conf);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error configurando I2C: %s", esp_err_to_name(err));
        return err;
    }

    err = i2c_driver_install(INA219_I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error instalando driver I2C: %s", esp_err_to_name(err));
        return err;
    }

    // Escribir registro de configuración al INA219
    uint8_t config_data[3] = {
        REG_CONFIG,
        (INA219_CONFIG_VALUE >> 8) & 0xFF,
        INA219_CONFIG_VALUE & 0xFF
    };

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (INA219_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, config_data, 3, true);
    i2c_master_stop(cmd);
    err = i2c_master_cmd_begin(INA219_I2C_PORT, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error escribiendo configuración INA219: %s", esp_err_to_name(err));
        return err;
    }

    // Esperar a que el INA219 complete la primera conversión
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "INA219 inicializado correctamente");
    return ESP_OK;
}

// ─── Lectura de voltaje de bus ────────────────────────────────
esp_err_t ina219_read_bus_voltage(float *voltage_out)
{
    if (!voltage_out) return ESP_ERR_INVALID_ARG;

    // Apuntar al registro de voltaje de bus
    uint8_t reg = REG_BUS_VOLTAGE;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (INA219_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_stop(cmd);
    esp_err_t err = i2c_master_cmd_begin(INA219_I2C_PORT, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error apuntando al registro de voltaje: %s", esp_err_to_name(err));
        return err;
    }

    // Leer 2 bytes del registro
    uint8_t raw[2] = {0};
    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (INA219_I2C_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, &raw[0], I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &raw[1], I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    err = i2c_master_cmd_begin(INA219_I2C_PORT, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);

    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error leyendo voltaje de bus: %s", esp_err_to_name(err));
        return err;
    }

    uint16_t raw_val = ((uint16_t)raw[0] << 8) | raw[1];

    // Verificar que la conversión está lista (bit 1 = CNVR)
    if (!(raw_val & 0x0002))
    {
        ESP_LOGW(TAG, "INA219: conversión no lista todavía");
    }

    // El voltaje está en bits 15:3, cada LSB = 4mV
    raw_val >>= 3;
    *voltage_out = raw_val * 0.004f;

    ESP_LOGI(TAG, "Raw register: 0x%04X | Voltaje: %.3fV", raw_val, *voltage_out);

    return ESP_OK;
}

// ─── Conversión voltaje → porcentaje ─────────────────────────
float ina219_voltage_to_pct(float voltage)
{
    if (voltage >= LIPO_2S_VOLTAGE_MAX) return 100.0f;
    if (voltage <= LIPO_2S_VOLTAGE_MIN) return 0.0f;

    float pct = (voltage - LIPO_2S_VOLTAGE_MIN) /
                (LIPO_2S_VOLTAGE_MAX - LIPO_2S_VOLTAGE_MIN) * 100.0f;

    return pct;
}