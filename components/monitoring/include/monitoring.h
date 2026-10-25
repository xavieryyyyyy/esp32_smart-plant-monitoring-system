#pragma once

#include <stdbool.h>

typedef struct {
    const char *candidate_state;
    const char *confirmed_state;
    int consecutive_count;
} monitoring_t;

int monitoring_percent(int raw);
const char *monitoring_classify(int percent);

void monitoring_init(monitoring_t *monitor);
bool monitoring_update(monitoring_t *monitor, const char *state);
void monitoring_reset_streak(monitoring_t *monitor);