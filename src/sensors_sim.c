#include "sensors.h"
#include "esp_random.h"

bool sensors_sim_init(void)
{
    // En simulación no hay nada que inicializar
    return true;
}

sensor_data_t sensors_sim_read(void)
{
    uint32_t r1 = esp_random();
    uint32_t r2 = esp_random();
    uint32_t r3 = esp_random();

    sensor_data_t d;
    d.soil_moisture_pct = (float)(r1 % 101);
    d.temperature_c     = 10.0f + (float)(r2 % 251) / 10.0f;  // 10.0..35.0
    d.humidity_pct      = (float)(r3 % 101);
    return d;
}
