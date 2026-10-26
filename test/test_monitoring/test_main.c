/*
 * These are small automatic checks for the rules in monitoring.c, using Unity.
 * They supply pretend readings/times, so the cooldown tests do not wait fifteen minutes.
 * pio test uploads this test program. Upload the normal application again afterward.
 */
#include "unity.h"
/* Use our percentage, state-confirmation, and notification-cooldown rules. */
#include "monitoring.h"
/* Use FreeRTOS types and constants for tasks, queues, and waiting. */
#include "freertos/FreeRTOS.h"
/* Create background jobs, pause them, or wake them with task notifications. */
#include "freertos/task.h"

/* Unity calls this before each monitoring test; these tests need no extra setup here. */
void setUp(void) {}
/* Unity calls this after each monitoring test; these tests leave nothing to clean up here. */
void tearDown(void) {}

/* Check both sides of each boundary: 30% and 70% themselves must be NORMAL. */
static void test_threshold_boundaries(void)
{
    /* Require the exact expected classification at this threshold boundary. */
    TEST_ASSERT_EQUAL_STRING("DRY", monitoring_classify(29));
    /* Require the exact expected classification at this threshold boundary. */
    TEST_ASSERT_EQUAL_STRING("NORMAL", monitoring_classify(30));
    /* Require the exact expected classification at this threshold boundary. */
    TEST_ASSERT_EQUAL_STRING("NORMAL", monitoring_classify(70));
    /* Require the exact expected classification at this threshold boundary. */
    TEST_ASSERT_EQUAL_STRING("WET", monitoring_classify(71));
}

/*
 * Two DRY readings are not enough. The third confirms DRY; the fourth must not repeat it.
 */
static void test_confirmation_and_no_repeat(void)
{
    /*
     * Keep this task/test's candidate, confirmed state, match count, and cooldown history
     * together.
     */
    monitoring_t monitor;
    /* Initialize the monitor before counting readings or consulting its cooldown history. */
    monitoring_init(&monitor);

    /* Require false here: this reading/change must not confirm or bypass the cooldown yet. */
    TEST_ASSERT_FALSE(monitoring_update(&monitor, "DRY"));
    /* Require false here: this reading/change must not confirm or bypass the cooldown yet. */
    TEST_ASSERT_FALSE(monitoring_update(&monitor, "DRY"));
    /* Require true here: this reading/change has met the confirmation or cooldown rule. */
    TEST_ASSERT_TRUE(monitoring_update(&monitor, "DRY"));
    /* Require false here: this reading/change must not confirm or bypass the cooldown yet. */
    TEST_ASSERT_FALSE(monitoring_update(&monitor, "DRY"));
}

/*
 * Call the reset function to act like a broken reading streak.
 * This checks the rule; it does not physically disconnect the potentiometer.
 */
static void test_error_reset_breaks_streak(void)
{
    /*
     * Keep this task/test's candidate, confirmed state, match count, and cooldown history
     * together.
     */
    monitoring_t monitor;
    /* Initialize the monitor before counting readings or consulting its cooldown history. */
    monitoring_init(&monitor);

    /* Feed one classified reading into the three-in-a-row confirmation rule. */
    monitoring_update(&monitor, "DRY");
    /* Feed one classified reading into the three-in-a-row confirmation rule. */
    monitoring_update(&monitor, "DRY");

    /*
     * Discard the unfinished candidate streak because its readings are no longer
     * consecutive/valid.
     */
    monitoring_reset_streak(&monitor);

    /* Require false here: this reading/change must not confirm or bypass the cooldown yet. */
    TEST_ASSERT_FALSE(monitoring_update(&monitor, "DRY"));
    /* Require false here: this reading/change must not confirm or bypass the cooldown yet. */
    TEST_ASSERT_FALSE(monitoring_update(&monitor, "DRY"));
    /* Require true here: this reading/change has met the confirmation or cooldown rule. */
    TEST_ASSERT_TRUE(monitoring_update(&monitor, "DRY"));
}

/*
 * Pretend an alert happened at time zero. It must wait all fifteen minutes.
 * 900,000,000 microseconds = fifteen minutes; one microsecond earlier is too soon.
 */
static void test_repeated_dry_waits_fifteen_minutes(void)
{
    /*
     * Keep this task/test's candidate, confirmed state, match count, and cooldown history
     * together.
     */
    monitoring_t monitor;
    /* Initialize the monitor before counting readings or consulting its cooldown history. */
    monitoring_init(&monitor);

    /* Require true here: this reading/change has met the confirmation or cooldown rule. */
    TEST_ASSERT_TRUE(monitoring_notification_due(&monitor, "DRY", 0));

    /*
     * Record when this event was queued; this starts its state's cooldown before network
     * delivery.
     */
    monitoring_record_notification(&monitor, "DRY", 0);

    /* Require false here: this reading/change must not confirm or bypass the cooldown yet. */
    TEST_ASSERT_FALSE(monitoring_notification_due(&monitor, "DRY", 899999999LL));

    /* Require true here: this reading/change has met the confirmation or cooldown rule. */
    TEST_ASSERT_TRUE(monitoring_notification_due(&monitor, "DRY", 900000000LL));
}

/*
 * A DRY alert must not use up WET's turn. Check that each keeps its own fifteen-minute clock.
 */
static void test_dry_and_wet_have_independent_cooldowns(void)
{
    /*
     * Keep this task/test's candidate, confirmed state, match count, and cooldown history
     * together.
     */
    monitoring_t monitor;
    /* Initialize the monitor before counting readings or consulting its cooldown history. */
    monitoring_init(&monitor);

    /*
     * Record when this event was queued; this starts its state's cooldown before network
     * delivery.
     */
    monitoring_record_notification(&monitor, "DRY", 0);

    /* Require true here: this reading/change has met the confirmation or cooldown rule. */
    TEST_ASSERT_TRUE(monitoring_notification_due(&monitor, "WET", 1));

    /*
     * Record when this event was queued; this starts its state's cooldown before network
     * delivery.
     */
    monitoring_record_notification(&monitor, "WET", 1);

    /* Require true here: this reading/change has met the confirmation or cooldown rule. */
    TEST_ASSERT_TRUE(monitoring_notification_due(&monitor, "DRY", 900000000LL));

    /* Require false here: this reading/change must not confirm or bypass the cooldown yet. */
    TEST_ASSERT_FALSE(monitoring_notification_due(&monitor, "WET", 900000000LL));

    /* Require true here: this reading/change has met the confirmation or cooldown rule. */
    TEST_ASSERT_TRUE(monitoring_notification_due(&monitor, "WET", 900000001LL));
}

/* A return to NORMAL should be allowed even just after a DRY or WET alert. */
static void test_cooldown_does_not_block_recovery(void)
{
    /*
     * Keep this task/test's candidate, confirmed state, match count, and cooldown history
     * together.
     */
    monitoring_t monitor;
    /* Initialize the monitor before counting readings or consulting its cooldown history. */
    monitoring_init(&monitor);

    /*
     * Record when this event was queued; this starts its state's cooldown before network
     * delivery.
     */
    monitoring_record_notification(&monitor, "DRY", 0);

    /* Require true here: this reading/change has met the confirmation or cooldown rule. */
    TEST_ASSERT_TRUE(monitoring_notification_due(&monitor, "NORMAL", 1));

    /*
     * Record when this event was queued; this starts its state's cooldown before network
     * delivery.
     */
    monitoring_record_notification(&monitor, "WET", 2);

    /* Require true here: this reading/change has met the confirmation or cooldown rule. */
    TEST_ASSERT_TRUE(monitoring_notification_due(&monitor, "NORMAL", 3));
}

/*
 * Start the test program here. PlatformIO excludes production src files for this test build.
 */
void app_main(void)
{
    /*
     * Pause this task and let other tasks run; pdMS_TO_TICKS converts milliseconds to scheduler
     * ticks.
     */
    vTaskDelay(pdMS_TO_TICKS(2000));

    /* Start Unity's count of tests and failures for this ESP32 test run. */
    UNITY_BEGIN();
    /* Run this named Unity case, including its setup/teardown hooks. */
    RUN_TEST(test_threshold_boundaries);
    /* Run this named Unity case, including its setup/teardown hooks. */
    RUN_TEST(test_confirmation_and_no_repeat);
    /* Run this named Unity case, including its setup/teardown hooks. */
    RUN_TEST(test_error_reset_breaks_streak);
    /* Run this named Unity case, including its setup/teardown hooks. */
    RUN_TEST(test_repeated_dry_waits_fifteen_minutes);
    /* Run this named Unity case, including its setup/teardown hooks. */
    RUN_TEST(test_dry_and_wet_have_independent_cooldowns);
    /* Run this named Unity case, including its setup/teardown hooks. */
    RUN_TEST(test_cooldown_does_not_block_recovery);
    /* Finish Unity and print the test summary for PlatformIO to collect. */
    UNITY_END();
}