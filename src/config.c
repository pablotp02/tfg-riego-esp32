#include "config.h"

// Configuración por defecto (modo demo)
static const system_config_t g_cfg = {
    .use_simulated_sensors      = false, // indica si estamos usando sensores reales o simulados
    .use_rs485_sensor           = true,

    .measure_period_ms          = 5000,  // 5 s demo (luego: horas)
    .irrigate_time_ms           = 2000,  // 2 s demo
    .soil_start_irrigation_pct  = 40.0f,
    .soil_stop_irrigation_pct   = 50.0f
};

const system_config_t* config_get(void)
{
    return &g_cfg;
}
