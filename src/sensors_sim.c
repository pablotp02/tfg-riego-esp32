#include "sensors.h"
#include "esp_random.h"

bool sensors_sim_init(void)
{
    // En simulación no hay nada que inicializar
    return true;
}

sensor_data_t sensors_sim_read(void)
{
    uint32_t r = esp_random();

    sensor_data_t d = {0};
    d.soil_moisture_pct = (float)(r % 101);
    return d;
}
