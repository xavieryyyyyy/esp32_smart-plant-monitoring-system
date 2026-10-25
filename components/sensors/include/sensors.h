#pragma once

#include <stdint.h>
#include "esp_err.h"

typedef struct {
    uint32_t sequence;
    int raw;
    float humidity;
    float temperature;
    esp_err_t pot_result;
    esp_err_t dht_result;
} sensor_reading_t;

void sensors_init(void);
esp_err_t sensors_read_potentiometer(int *raw);
esp_err_t sensors_read_dht(float *humidity, float *temperature);