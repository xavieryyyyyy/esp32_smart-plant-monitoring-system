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

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(2000));

    UNITY_BEGIN();
    RUN_TEST(test_threshold_boundaries);
    RUN_TEST(test_confirmation_and_no_repeat);
    RUN_TEST(test_error_reset_breaks_streak);
    UNITY_END();
}