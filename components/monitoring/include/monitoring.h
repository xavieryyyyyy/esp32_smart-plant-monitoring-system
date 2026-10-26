/*
 * This header lists the checking rules that app_tasks.c can use.
 * monitoring.c contains the actual rules. The monitor below remembers each check's progress.
 * These functions do not read sensors or send Internet messages.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

/*
 * A state pointer remembers where a word is stored; it does not copy the word.
 * Use words that stay available, such as the strings returned by monitoring_classify().
 */
typedef struct {
    /* State currently being counted, for example DRY. */
    const char *candidate_state;
    /* Most recent state that reached three matching readings. */
    const char *confirmed_state;
    /* Number of matching readings in a row; stops at three. */
    int consecutive_count;
    /* Minimum wait before another alert of the same DRY/WET type; in microseconds. */
    int64_t cooldown_us;
    /* Time a DRY event last entered the queue; -1 means none yet. */
    int64_t last_dry_notification_us;
    /* Time a WET event last entered the queue; -1 means none yet. */
    int64_t last_wet_notification_us;
} monitoring_t;

/* Convert a valid ADC number from 0..4095 into a simulated percentage. */
int monitoring_percent(int raw);
const char *monitoring_classify(int percent);

void monitoring_init(monitoring_t *monitor);
/* Return true when three matching readings confirm a different state. */
bool monitoring_update(monitoring_t *monitor, const char *state);
/* Start the matching count again. Keep the last confirmed state and alert times. */
void monitoring_reset_streak(monitoring_t *monitor);

/* Ask whether a changed state may be queued now. This function does not send reminders. */
bool monitoring_notification_due(const monitoring_t *monitor, const char *state, int64_t now_us);

/* Call this after the notification queue accepts the event, to start its cooldown. */
void monitoring_record_notification(monitoring_t *monitor, const char *state, int64_t now_us);
