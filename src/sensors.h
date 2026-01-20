#pragma once
#include <stdbool.h>
#include "fsm.h"

sensor_data_t sensors_read(void);
bool sensors_init(void);