#pragma once
#include <stdbool.h>
#include "fsm.h"

sensor_data_t sensors_read(void);
bool sensors_init(void);

// Getters para datos adicionales del SEN0604
float sensors_get_last_ph(void);
float sensors_get_last_ec(void);
bool sensors_have_sen0604_data(void);