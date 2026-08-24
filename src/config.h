#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "hal/gpio_types.h"
#include "driver/uart.h"

// PINES
#define SOIL_MOIST_GPIO GPIO_NUM_34 // potenciómetro 1 -> humedad suelo
#define RELAY_GPIO GPIO_NUM_25 // se puede cambiar el numero más adelante
#define ENABLE_REAL_RELAY 1

// RS485 / MODBUS (SEN0604 + DFR0845)
#define RS485_UART_PORT  (UART_NUM_2)
#define RS485_UART_TX    (GPIO_NUM_17)
#define RS485_UART_RX    (GPIO_NUM_16)
#define RS485_BAUDRATE   (9600)
#define RS485_SLAVE_ADDR (0x01)

// Umbrales ambientales provisionales (se ajustarán por especie/planta)
#define SOIL_MIN_TEMP_C  (5.0f)  // por debajo, no regar
#define AIR_MAX_HUM_PCT  (95.0f) // por encima, no regar

// Cooldown de riego:
// Número mínimo de ciclos que deben pasar entre dos riegos consecutivos.
// Evita saturar el suelo si la humedad vuelve a bajar rápido tras un riego.
// TODO (producción): migrar a cooldown por tiempo real (horas) usando timestamp NTP en RTC.
#define IRRIGATION_COOLDOWN_CYCLES (3U)

/* Simulación de batería:
    0 -> batería fija al 100%, modo energético siempre NORMAL
        Usar durante desarrollo y pruebas para evitar bloqueos artificiales
    1 -> simulación activa: la batería se consume por ciclo y el modo energético cambia según el nivel.
        Útil para probar la lógica de gestión energética
    TODO: reemplazar por medición real de batería vía ADC (divisor de tensión o módulo INA219)
*/
#define ENABLE_BATTERY_SIMULATION 0

typedef struct {
    bool use_simulated_sensors;     // true: simulación | false: ADC real
    bool use_rs485_sensor;          // true: leer suelo por SEN0604 (RS485)

    // Temporización (demo)
    uint32_t measure_period_ms;     // tiempo entre ciclos (demo). Luego: horas
    uint32_t irrigate_time_ms;      // tiempo de riego

    // Planificador por ciclos (demo)
    uint32_t measure_every_n_cycles; // cada cuántos ciclos medir

    // Umbrales
    float soil_start_irrigation_pct; // por debajo de este valor, pedir riego
    float soil_stop_irrigation_pct;  // por encima de este valor, dejar de regar

} system_config_t;

// Devuelve la configuración actual (por ahora: valores por defecto)
const system_config_t* config_get(void);

// (Opcional futuro) Permitir cambiar config en runtime
// void config_set(const system_config_t* new_cfg);