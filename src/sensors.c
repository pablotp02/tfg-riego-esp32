#include "sensors.h"
#include "config.h"

// Backends
bool sensors_adc_init(void);
sensor_data_t sensors_adc_read(void);

bool sensors_sim_init(void);
sensor_data_t sensors_sim_read(void);

bool sensors_init(void)
{
    const system_config_t *cfg = config_get();
    return cfg->use_simulated_sensors ? sensors_sim_init() : sensors_adc_init();
}

sensor_data_t sensors_read(void)
{
    const system_config_t *cfg = config_get();
    return cfg->use_simulated_sensors ? sensors_sim_read() : sensors_adc_read();
}
