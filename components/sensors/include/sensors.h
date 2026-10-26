/*
 * app_tasks.c includes this header to use the sensor functions in sensors.c.
 * sensor_reading_t keeps one reading's values, times, and success/error results together.
 * The queues carry copies of this structure between tasks.
 * The pointer arguments let a function store results in variables provided by its caller.
 * Only use those results when the function returns ESP_OK (success).
 */
#pragma once

/* Use bool, true, and false for yes/no values. */
#include <stdbool.h>
/* Use explicit-width integers such as uint32_t and int64_t for counters and timers. */
#include <stdint.h>
/* Store Unix timestamps and convert them into calendar date/time fields. */
#include <time.h>
/* Use ESP-IDF result codes: ESP_OK means success; other codes describe failures. */
#include "esp_err.h"

/* Group related fields into one value so callers/queues keep the correct pieces together. */
typedef struct {
    /*
     * Reading number: 0, 1, 2, ... A missing number tells monitoring that data was skipped.
     */
    uint32_t sequence;
    /* Time since ESP32 startup when this reading began; used to check its age. */
    int64_t sampled_at_us;
    /* Date/time saved when reading began. Use it only when time_valid is true. */
    time_t measured_at;
    /* Remember whether the clock was already synchronized when this measurement began. */
    bool time_valid;
    /* Potentiometer number before percentage conversion: 0..4095. */
    int raw;
    /* Air humidity in percent, and temperature in degrees Celsius below. */
    float humidity;
    /*
     * Store air temperature in degrees Celsius; its read-validity flag determines whether it is
     * usable.
     */
    float temperature;
    /*
     * Separate results for the two sensors: ESP_OK means that particular read succeeded.
     */
    esp_err_t pot_result;
    /* Keep this operation's success/error code so later steps can stop safely on failure. */
    esp_err_t dht_result;
} sensor_reading_t;

/* Prepare the GPIO1 potentiometer and GPIO2 DHT11 once, in the sensor task. */
void sensors_init(void);
/* Read one knob value, validate it, then write the result through the caller's pointer. */
esp_err_t sensors_read_potentiometer(int *raw);
/*
 * Store humidity first and temperature second. Return an error if either read/check fails.
 */
esp_err_t sensors_read_dht(float *humidity, float *temperature);