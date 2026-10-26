/*
 * This file decides what the potentiometer reading means.
 * It turns the number into a percentage, chooses DRY/NORMAL/WET, and counts matches.
 * It also decides whether enough time has passed to allow another DRY/WET alert.
 * app_tasks.c uses these functions; the unit tests check the same rules.
 */
#include "monitoring.h"
#include <string.h>

int monitoring_percent(int raw)
{
    /*
     * The ADC gives 0 to 4095. Convert that to a simulated 0% to 100%.
     * Whole-number division drops the decimal part. This is not measured soil moisture.
     */
    return (raw * 100) / 4095;
}

/* These words stay in memory for the whole program, so the monitor can remember them. */
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

/* Start with no candidate, no confirmed state, and no previously queued alerts. */
void monitoring_init(monitoring_t *monitor)
{
    monitor->candidate_state = "";
    monitor->confirmed_state = "UNKNOWN";
    monitor->consecutive_count = 0;
    /*
     * Use microseconds: 15 minutes x 60 seconds x 1,000,000.
     * A saved time of -1 means we have not queued that kind of alert yet.
     */
    monitor->cooldown_us = 15LL * 60LL * 1000000LL;
    monitor->last_dry_notification_us = -1;
    monitor->last_wet_notification_us = -1;
}

/*
 * A failed or missing reading breaks the run of matching readings.
 * Forget the candidate/count, but keep the last confirmed state and alert times.
 */
void monitoring_reset_streak(monitoring_t *monitor)
{
    monitor->candidate_state = "";
    monitor->consecutive_count = 0;
}

bool monitoring_update(monitoring_t *monitor, const char *state)
{
    /*
     * Count matching state names. Two different raw values can both be DRY.
     * A different state starts a new count at 1; matching states count up to 3.
     */
    if (strcmp(state, monitor->candidate_state) != 0) {
        monitor->candidate_state = state;
        monitor->consecutive_count = 1;
    } else if (monitor->consecutive_count < 3) {
        monitor->consecutive_count++;
    }

    /*
     * Only report a change after three matches, and only if it differs from the confirmed state.
     */
    if (monitor->consecutive_count == 3 &&
        strcmp(monitor->candidate_state, monitor->confirmed_state) != 0) {
        monitor->confirmed_state = monitor->candidate_state;
        return true;
    }

    return false;
}

/* Answer whether this confirmed change is allowed to create a notification now. */
bool monitoring_notification_due(const monitoring_t *monitor, const char *state, int64_t now_us)
{
    if (monitor == NULL || state == NULL || now_us < 0 || monitor->cooldown_us < 0) {
        return false;
    }

    /*
     * Always allow a confirmed return to NORMAL, even while DRY/WET alerts are on cooldown.
     */
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

    /*
     * Allow the first alert, or allow another after its own fifteen-minute wait.
     * DRY and WET each have a separate saved time.
     */
    return last_notification < 0 ||
           (now_us >= last_notification && now_us - last_notification >= monitor->cooldown_us);
}

/*
 * Remember when this DRY/WET event entered the queue. This does not mean it was delivered.
 */
void monitoring_record_notification(monitoring_t *monitor, const char *state, int64_t now_us)
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
