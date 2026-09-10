#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "hal/gpio_types.h"
#include "driver/uart.h"

// PINES
#define RELAY_GPIO GPIO_NUM_25 

// RS485 / MODBUS (SEN0604 + DFR0845)
#define RS485_UART_PORT  (UART_NUM_2)
#define RS485_UART_TX    (GPIO_NUM_17)
#define RS485_UART_RX    (GPIO_NUM_16)
#define RS485_BAUDRATE   (9600)
#define RS485_SLAVE_ADDR (0x01)

// Umbral de temperatura mínima de suelo, usado como valor por
// defecto cuando no hay ninguna configuración sincronizada con
// el backend (véase device_config_get_defaults())
#define SOIL_MIN_TEMP_C  (5.0f)

// Cooldown de riego:
// Número mínimo de ciclos que deben pasar entre dos riegos consecutivos.
#define IRRIGATION_COOLDOWN_CYCLES (3U)

/* Simulación de batería:
    0 -> medición real mediante el módulo INA219
    1 -> simulación activa: la batería se consume por ciclo y el modo energético cambia según el nivel.
        Útil para probar la lógica de cambio de modo energético sin depender de la descarga real de la 
        batería
*/
#define ENABLE_BATTERY_SIMULATION 0