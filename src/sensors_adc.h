#pragma once 
#include <stdbool.h>
#include "sensors.h" 

bool sensors_adc_init(void);
sensor_data_t sensors_adc_read(void);