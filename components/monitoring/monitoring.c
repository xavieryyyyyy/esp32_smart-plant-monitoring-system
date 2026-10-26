#include "monitoring.h"
#include <string.h>

int monitoring_percent(int raw)
{
    return (raw * 100) / 4095;
}

const char *monitoring_classify(int percent)
{
    if (percent < 30) {
        return "DRY";
    }

    if (percent > 70) {
        return "WET";
    }

    return "NORMAL";
}

void monitoring_init(monitoring_t *monitor)
{
    monitor->candidate_state = "";
    monitor->confirmed_state = "UNKNOWN";
    monitor->consecutive_count = 0;
    monitor->cooldown_us = 15LL * 60LL * 1000000LL;
monitor->last_dry_notification_us = -1;
monitor->last_wet_notification_us = -1;
}

void monitoring_reset_streak(monitoring_t *monitor)
{
    monitor->candidate_state = "";
    monitor->consecutive_count = 0;
}

bool monitoring_update(monitoring_t *monitor, const char *state)
{
    if (strcmp(state, monitor->candidate_state) != 0) {
        monitor->candidate_state = state;
        monitor->consecutive_count = 1;
    } else if (monitor->consecutive_count < 3) {
        monitor->consecutive_count++;
    }

    if (monitor->consecutive_count == 3 &&
        strcmp(monitor->candidate_state,
               monitor->confirmed_state) != 0) {
        monitor->confirmed_state = monitor->candidate_state;
        return true;
    }

    return false;
}

bool monitoring_notification_due(
    const monitoring_t *monitor, const char *state, int64_t now_us)
{
    if (monitor == NULL || state == NULL ||
        now_us < 0 || monitor->cooldown_us < 0) {
        return false;
    }

    if (strcmp(state, "NORMAL") == 0) {
        return true;
    }

    int64_t last_notification;

    if (strcmp(state, "DRY") == 0) {
        last_notification = monitor->last_dry_notification_us;
    } else if (strcmp(state, "WET") == 0) {
        last_notification = monitor->last_wet_notification_us;
    } else {
        return false;
    }

    return last_notification < 0 ||
           (now_us >= last_notification &&
            now_us - last_notification >= monitor->cooldown_us);
}

void monitoring_record_notification(
    monitoring_t *monitor, const char *state, int64_t now_us)
{
    if (monitor == NULL || state == NULL || now_us < 0) {
        return;
    }

    if (strcmp(state, "DRY") == 0) {
        monitor->last_dry_notification_us = now_us;
    } else if (strcmp(state, "WET") == 0) {
        monitor->last_wet_notification_us = now_us;
    }
}