#include "sensors_dht22.h"

#include "esp_rom_sys.h" //esp_rom_delay_us
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "DHT22";

// Ajustes típicos del protocolo
#define DHT_START_LOW_US    1000    // host baja línea aprox. 1ms (DHT22)
#define DHT_START_HIGH_US   30      // host libera línea arpox. 20-40us
#define DHT_TIMEOUT_US      120     // timeout para esperar flancos (seguridad)
#define DHT_BITS            40

// Espera a que el pin esté en un nivel específico hasta timeout_us
// Devuelve true si lo consigue, false si timeout
static bool wait_for_level(gpio_num_t pin, int level, uint32_t timeout_us)
{
    uint32_t t = 0;
    while (gpio_get_level(pin) != level)
    {
        esp_rom_delay_us(1);
        if (t++ >= timeout_us) return false; // si el tiempo que llevamos es mayor que el de timeout, salimos
    }
    return true;
}

// Mide cuánto dura el pin en un nivel concreto (en ms), con timeout
// Devuelve duración medida, o -1 si timeout
static int measure_pulse_us(gpio_num_t pin, int level, uint32_t timeout_us)
{
    uint32_t t = 0;
    while (gpio_get_level(pin) == level)
    {
        esp_rom_delay_us(1);
        if (t++ >= timeout_us) return -1;
    }
    return (int)t;
}

esp_err_t dht22_read(gpio_num_t pin, float *temp_c, float * hum_pct)
{
    if (!temp_c || !hum_pct) return ESP_ERR_INVALID_ARG;

    // 1) Preparar GPIO como salida open-drain "manual"
    // Usamos OUTPUT para forzar LOW, y luego INPUT para "soltar" la línea.
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_set_level(pin, 0);

    // NOTA: La resistencia pull-up externa es obligatoria para que suba a 1 cuando soltamos
    esp_rom_delay_us(DHT_START_LOW_US);

    // Soltamos línea: pasamos a input (alta impedancia)
    gpio_set_direction(pin, GPIO_MODE_INPUT);
    esp_rom_delay_us(DHT_START_HIGH_US);

    // 2) Respuesta del sensor: 'low' aprox. 80us, 'high' aprox. 80us
    if (!wait_for_level(pin, 0, DHT_TIMEOUT_US))
    {
        ESP_LOGW(TAG, "Timeout esperando ACK LOW");
        return ESP_ERR_TIMEOUT;
    }

    if (!wait_for_level(pin, 1, DHT_TIMEOUT_US))
    {
        ESP_LOGW(TAG, "Timeout esperando ACK HIGH");
        return ESP_ERR_TIMEOUT;
    }

    // Esperamos a que termine el HIGH del ACK para empezar a leer bits
    if (measure_pulse_us(pin, 1, DHT_TIMEOUT_US) < 0)
    {
        ESP_LOGW(TAG, "Timeout final ACK HIGH");
        return ESP_ERR_TIMEOUT;
    }

    // 3) Leer 40 bits: cada bit: 
    // 50 us LOW
    // HIGH: aprox 26-28us => 0, aprox 70us => 1
    uint8_t data[5] = {0};

    for (int i = 0; i < DHT_BITS; i++)
    {
        // Esperar inicio del bit: LOW 50us (a veces ya estamos en LOW)
        if (!wait_for_level(pin, 0, DHT_TIMEOUT_US))
        {
            ESP_LOGW(TAG, "Timeout esperando bit LOW (i=%d)", i);
            return ESP_ERR_TIMEOUT;
        }

        // Medir duración LOW (no nos hace falta exacta, pero avanzamos estado)
        if (measure_pulse_us(pin, 0, DHT_TIMEOUT_US) < 0)
        {
            ESP_LOGW(TAG, "Timeout midiendo LOW bit (i=%d)", i);
            return ESP_ERR_TIMEOUT;
        }

        // A continuación viene HIGH, su duración la decide el bit
        if (!wait_for_level(pin, 1, DHT_TIMEOUT_US))
        {
            ESP_LOGW(TAG, "Timeout esperando bit HIGH (i=%d)", i);
            return ESP_ERR_TIMEOUT;
        }

        int high_us = measure_pulse_us(pin, 1, DHT_TIMEOUT_US);
        if (high_us < 0)
        {
            ESP_LOGW(TAG, "Timeout midiendo HIGH bit (i=%d)",i);
            return ESP_ERR_TIMEOUT;
        }

        // Umbral típico: si > aprox. 40us lo tratamos como 1, sino 0
        int bit = (high_us > 40) ? 1 : 0;

        data[i / 8] <<= 1;
        data[i / 8] |= (uint8_t)bit;
    }

    // 4) Checksum: suma de los 4 primeros bytes (8-bit)
    uint8_t sum = (uint8_t)(data[0] + data[1] + data[2] + data[3]);
    if (sum != data[4])
    {
        ESP_LOGW(TAG, "CHECKSUM FAIL: calc=0x%02X recv=0x%02X", sum, data[4]);
        return ESP_ERR_INVALID_CRC;
    }

    // 5) Decodificar DHT22: 
    // Humedad: 16 bits unsigned, decimales *0.1
    // Temp: 16 bits, bit15 signo
    uint16_t rh = ((uint16_t)data[0] << 8) | data[1];
    uint16_t rt = ((uint16_t)data[2] << 8) | data[3];

    *hum_pct = (float)rh / 10.0f;

    bool neg = (rt & 0x8000) != 0;
    rt &= 0x7FFF;
    float t = (float)rt / 10.0f;
    *temp_c = neg ? -t : t;

    return ESP_OK;
}