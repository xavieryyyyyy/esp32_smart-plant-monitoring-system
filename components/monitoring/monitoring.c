/*
 * This file decides what the potentiometer reading means.
 * It turns the number into a percentage, chooses DRY/NORMAL/WET, and counts matches.
 * It also decides whether enough time has passed to allow another DRY/WET alert.
 * app_tasks.c uses these functions; the unit tests check the same rules.
 */
/*
 * Rule-reading key:
 * monitor->field reads/writes a field through the monitor pointer supplied by the caller.
 * strcmp(a, b) returns 0 for matching text; comparing the pointer addresses is not enough.
 * A candidate is the state being counted; confirmed_state is the last accepted state.
 * Fifteen-minute waits apply to returning DRY/WET changes, not periodic reminders.
 * Recording a queued event does not prove any service received or displayed it.
 */
#include "monitoring.h"
/* Measure, compare, copy, or search text and byte arrays. */
#include <string.h>

/* Convert a validated 12-bit ADC value into a simulated whole-number percentage. */
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
    /* Below 30% is the demonstration DRY range; exactly 30% belongs to NORMAL. */
    if (percent < 30) {
        /* Return the fixed classification word; callers may remember this persistent string. */
        return "DRY";
    }

    /* Above 70% is the demonstration WET range; exactly 70% belongs to NORMAL. */
    if (percent > 70) {
        /* Return the fixed classification word; callers may remember this persistent string. */
        return "WET";
    }

    /* Return the fixed classification word; callers may remember this persistent string. */
    return "NORMAL";
}

/* Start with no candidate, no confirmed state, and no previously queued alerts. */
void monitoring_init(monitoring_t *monitor)
{
    /* Remember the new candidate word (or clear it during initialization/reset). */
    monitor->candidate_state = "";
    /* Keep the last three-reading-confirmed word; UNKNOWN is the startup value. */
    monitor->confirmed_state = "UNKNOWN";
    /* Set/increase the number of matching readings; the count saturates at three. */
    monitor->consecutive_count = 0;
    /*
     * Use microseconds: 15 minutes x 60 seconds x 1,000,000.
     * A saved time of -1 means we have not queued that kind of alert yet.
     */
    monitor->cooldown_us = 15LL * 60LL * 1000000LL;
    /* Save DRY's last queued uptime; -1 during initialization means no earlier event. */
    monitor->last_dry_notification_us = -1;
    /* Save WET's last queued uptime; -1 during initialization means no earlier event. */
    monitor->last_wet_notification_us = -1;
}

/*
 * A failed or missing reading breaks the run of matching readings.
 * Forget the candidate/count, but keep the last confirmed state and alert times.
 */
void monitoring_reset_streak(monitoring_t *monitor)
{
    /* Remember the new candidate word (or clear it during initialization/reset). */
    monitor->candidate_state = "";
    /* Set/increase the number of matching readings; the count saturates at three. */
    monitor->consecutive_count = 0;
}

/*
 * Count matching classifications and return true only when a different state reaches three
 * readings.
 */
bool monitoring_update(monitoring_t *monitor, const char *state)
{
    /*
     * Count matching state names. Two different raw values can both be DRY.
     * A different state starts a new count at 1; matching states count up to 3.
     */
    if (strcmp(state, monitor->candidate_state) != 0) {
        /* Remember the new candidate word (or clear it during initialization/reset). */
        monitor->candidate_state = state;
        /* Set/increase the number of matching readings; the count saturates at three. */
        monitor->consecutive_count = 1;
        /* Check the run length before increasing it or confirming a different state. */
    } else if (monitor->consecutive_count < 3) {
        /* Set/increase the number of matching readings; the count saturates at three. */
        monitor->consecutive_count++;
    }

    /*
     * Only report a change after three matches, and only if it differs from the confirmed state.
     */
    if (monitor->consecutive_count == 3 &&
        strcmp(monitor->candidate_state, monitor->confirmed_state) != 0) {
        /* Keep the last three-reading-confirmed word; UNKNOWN is the startup value. */
        monitor->confirmed_state = monitor->candidate_state;
        /* Return yes/true: the condition this helper is checking has been satisfied. */
        return true;
    }

    /* Return no/false: this check did not authorize a change, output, or action. */
    return false;
}

/* Answer whether this confirmed change is allowed to create a notification now. */
bool monitoring_notification_due(const monitoring_t *monitor, const char *state, int64_t now_us)
{
    /* Check for an absent pointer or failed allocation before dereferencing/using it. */
    if (monitor == NULL || state == NULL || now_us < 0 || monitor->cooldown_us < 0) {
        /* Return no/false: this check did not authorize a change, output, or action. */
        return false;
    }

    /*
     * Always allow a confirmed return to NORMAL, even while DRY/WET alerts are on cooldown.
     */
    if (strcmp(state, "NORMAL") == 0) {
        /* Return yes/true: the condition this helper is checking has been satisfied. */
        return true;
    }

    /* Select the previous queue time for this state, keeping DRY and WET independent. */
    int64_t last_notification;

    /* Compare the actual characters; strcmp returns zero when both state words match. */
    if (strcmp(state, "DRY") == 0) {
        /* Select DRY's previous queued-alert time without consulting WET's independent history. */
        last_notification = monitor->last_dry_notification_us;
        /* Compare the actual characters; strcmp returns zero when both state words match. */
    } else if (strcmp(state, "WET") == 0) {
        /* Select WET's previous queued-alert time without consulting DRY's independent history. */
        last_notification = monitor->last_wet_notification_us;
    } else {
        /* Return no/false: this check did not authorize a change, output, or action. */
        return false;
    }

    /*
     * Allow the first alert, or allow another after its own fifteen-minute wait.
     * DRY and WET each have a separate saved time.
     */
    return last_notification < 0 ||
           /*
            * Require a non-backward clock and at least the full cooldown since this state's last
            * queued alert.
            */
           (now_us >= last_notification && now_us - last_notification >= monitor->cooldown_us);
}

/*
 * Remember when this DRY/WET event entered the queue. This does not mean it was delivered.
 */
void monitoring_record_notification(monitoring_t *monitor, const char *state, int64_t now_us)
{
    /* Check for an absent pointer or failed allocation before dereferencing/using it. */
    if (monitor == NULL || state == NULL || now_us < 0) {
        /* Leave this function now; no valid result/action is available on this path. */
        return;
    }

    /* Compare the actual characters; strcmp returns zero when both state words match. */
    if (strcmp(state, "DRY") == 0) {
        /* Save DRY's last queued uptime; -1 during initialization means no earlier event. */
        monitor->last_dry_notification_us = now_us;
        /* Compare the actual characters; strcmp returns zero when both state words match. */
    } else if (strcmp(state, "WET") == 0) {
        /* Save WET's last queued uptime; -1 during initialization means no earlier event. */
        monitor->last_wet_notification_us = now_us;
    }
}
