#include "unity.h"
#include "monitoring.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void setUp(void) {}
void tearDown(void) {}



static void test_threshold_boundaries(void)
{
    TEST_ASSERT_EQUAL_STRING("DRY", monitoring_classify(29));
    TEST_ASSERT_EQUAL_STRING("NORMAL", monitoring_classify(30));
    TEST_ASSERT_EQUAL_STRING("NORMAL", monitoring_classify(70));
    TEST_ASSERT_EQUAL_STRING("WET", monitoring_classify(71));
}

static void test_confirmation_and_no_repeat(void)
{
    monitoring_t monitor;
    monitoring_init(&monitor);

    TEST_ASSERT_FALSE(monitoring_update(&monitor, "DRY"));
    TEST_ASSERT_FALSE(monitoring_update(&monitor, "DRY"));
    TEST_ASSERT_TRUE(monitoring_update(&monitor, "DRY"));
    TEST_ASSERT_FALSE(monitoring_update(&monitor, "DRY"));
}

static void test_error_reset_breaks_streak(void)
{
    monitoring_t monitor;
    monitoring_init(&monitor);

    monitoring_update(&monitor, "DRY");
    monitoring_update(&monitor, "DRY");

    monitoring_reset_streak(&monitor);

    TEST_ASSERT_FALSE(monitoring_update(&monitor, "DRY"));
    TEST_ASSERT_FALSE(monitoring_update(&monitor, "DRY"));
    TEST_ASSERT_TRUE(monitoring_update(&monitor, "DRY"));
}

static void test_repeated_dry_waits_fifteen_minutes(void)
{
    monitoring_t monitor;
    monitoring_init(&monitor);

    TEST_ASSERT_TRUE(
        monitoring_notification_due(&monitor, "DRY", 0));

    monitoring_record_notification(&monitor, "DRY", 0);

    TEST_ASSERT_FALSE(
        monitoring_notification_due(&monitor, "DRY", 899999999LL));

    TEST_ASSERT_TRUE(
        monitoring_notification_due(&monitor, "DRY", 900000000LL));
}

static void test_dry_and_wet_have_independent_cooldowns(void)
{
    monitoring_t monitor;
    monitoring_init(&monitor);

    monitoring_record_notification(&monitor, "DRY", 0);

    TEST_ASSERT_TRUE(
        monitoring_notification_due(&monitor, "WET", 1));

    monitoring_record_notification(&monitor, "WET", 1);

    TEST_ASSERT_TRUE(
        monitoring_notification_due(&monitor, "DRY", 900000000LL));

    TEST_ASSERT_FALSE(
        monitoring_notification_due(&monitor, "WET", 900000000LL));

    TEST_ASSERT_TRUE(
        monitoring_notification_due(&monitor, "WET", 900000001LL));
}

static void test_cooldown_does_not_block_recovery(void)
{
    monitoring_t monitor;
    monitoring_init(&monitor);

    monitoring_record_notification(&monitor, "DRY", 0);

    TEST_ASSERT_TRUE(
        monitoring_notification_due(&monitor, "NORMAL", 1));

    monitoring_record_notification(&monitor, "WET", 2);

    TEST_ASSERT_TRUE(
        monitoring_notification_due(&monitor, "NORMAL", 3));
}

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(2000));

    UNITY_BEGIN();
    RUN_TEST(test_threshold_boundaries);
    RUN_TEST(test_confirmation_and_no_repeat);
    RUN_TEST(test_error_reset_breaks_streak);
    RUN_TEST(test_repeated_dry_waits_fifteen_minutes);
    RUN_TEST(test_dry_and_wet_have_independent_cooldowns);
RUN_TEST(test_cooldown_does_not_block_recovery);
    UNITY_END();
}