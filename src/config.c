#include "config.h"

// Configuración por defecto (modo demo)
static const system_config_t g_cfg = {
    .use_simulated_sensors   = false, // indica si estamos usando sensores reales o simulados
    .use_rs485_sensor        = false,

    .measure_period_ms       = 5000,  // 5 s demo (luego: horas)
    .irrigate_time_ms        = 2000,  // 2 s demo
    .measure_every_n_cycles  = 1,
    .send_every_n_cycles     = 3,
    .soil_threshold_pct      = 45.0f
};

const system_config_t* config_get(void)
{
    return &g_cfg;
}
