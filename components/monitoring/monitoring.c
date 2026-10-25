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