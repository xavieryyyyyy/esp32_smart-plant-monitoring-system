#pragma once

#include "esp_err.h"

void sensors_init(void);
esp_err_t sensors_read_potentiometer(int *raw);
esp_err_t sensors_read_dht(float *humidity, float *temperature);