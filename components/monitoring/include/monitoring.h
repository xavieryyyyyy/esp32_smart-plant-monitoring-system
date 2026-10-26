#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    const char *candidate_state;
    const char *confirmed_state;
    int consecutive_count;
    int64_t cooldown_us;
int64_t last_dry_notification_us;
int64_t last_wet_notification_us;
} monitoring_t;

int monitoring_percent(int raw);
const char *monitoring_classify(int percent);

void monitoring_init(monitoring_t *monitor);
bool monitoring_update(monitoring_t *monitor, const char *state);
void monitoring_reset_streak(monitoring_t *monitor);

bool monitoring_notification_due(
    const monitoring_t *monitor, const char *state, int64_t now_us);

void monitoring_record_notification(
    monitoring_t *monitor, const char *state, int64_t now_us);