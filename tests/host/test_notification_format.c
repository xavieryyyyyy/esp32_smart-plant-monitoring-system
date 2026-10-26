/* Use the shared text, PhST, and JSON builders instead of duplicating layouts. */
/*
 * These tests run on the computer through scripts/test_notification_format.py.
 * They call the REAL formatter with dummy data; they do not read sensors or send messages.
 * CHECK records a failure and leaves the current test. RUN calls each named test,
 * counts cases, and prints PASS/FAIL. Their backslashes continue each macro definition.
 * The allocation-budget cases intentionally make cJSON run out of memory to check
 * cleanup. A passing host run does not prove delivery or phone rendering on hardware.
 */
#include "notification_format.h"
/* Use integer limits to reject arithmetic that would overflow. */
#include <limits.h>
/* Check for invalid floating-point values, including NaN and infinity. */
#include <math.h>
/* Format bounded text with snprintf(), or use file/console I/O in host tests. */
#include <stdio.h>
/* Allocate/free temporary memory, and provide standard utility functions. */
#include <stdlib.h>
/* Measure, compare, copy, or search text and byte arrays. */
#include <string.h>

/* Count failed offline checks so the executable can return a failing exit code. */
static int failures;
/* Count named test cases; this is separate from the number of individual assertions. */
static int cases;
/* Remember where the test runner wants generated example files saved. */
static const char *artifacts;
/* Test this condition; print its location, count a failure, and leave the current test if false. */
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "FAIL %s:%d: %s\n", __func__, __LINE__, #condition);                   \
            failures++;                                                                            \
            return;                                                                                \
        }                                                                                          \
    } while (0)

/*
 * Create repeatable dummy readings/time so tests compare against known values rather than live
 * sensors.
 */
static notification_message_t sample(void)
{
    /*
     * Return a fresh known DRY sample; the same captured values make each formatting test
     * repeatable.
     */
    return (notification_message_t){.state = "DRY",
                                    /*
                                     * Copy this captured event's simulated percentage, not a
                                     * newer live reading.
                                     */
                                    .moisture_percent = 26,
                                    /*
                                     * Use temperature captured with this event;
                                     * environment_valid determines whether it is displayed.
                                     */
                                    .temperature = 28.0f,
                                    /*
                                     * Use humidity from the same captured reading, keeping the
                                     * values paired.
                                     */
                                    .humidity = 77.0f,
                                    /*
                                     * Mark both environment values usable only if this event's
                                     * DHT read succeeded.
                                     */
                                    .environment_valid = true,
                                    /*
                                     * Keep the original captured Unix time; the formatter
                                     * converts only its display to PhST.
                                     */
                                    .measured_at = (time_t)1792973754,
                                    /*
                                     * Carry the acquisition-time validity flag instead of
                                     * substituting a later synchronized time.
                                     */
                                    .time_valid = true};
}

/* Save a generated example in the requested temporary output folder for inspection. */
static void save_text(const char *name, const char *text)
{
    /* Reserve a bounded path buffer for a generated test artifact. */
    char filename[1024];
    /* Format text into a fixed-size array; check the returned required length before using it. */
    int count = snprintf(filename, sizeof(filename), "%s/%s", artifacts, name);
    /* Reject a generated artifact path that failed formatting or would be truncated. */
    if (count < 0 || count >= (int)sizeof(filename)) {
        /* Count this failed host check so the final executable exits with failure. */
        failures++;
        /* Leave this function now; no valid result/action is available on this path. */
        return;
    }
    /* Open the generated example file in binary mode to preserve its exact UTF-8/text bytes. */
    FILE *file = fopen(filename, "wb");
    /* Stop if the local artifact file could not be opened. */
    if (!file) {
        /* Count this failed host check so the final executable exits with failure. */
        failures++;
        /* Leave this function now; no valid result/action is available on this path. */
        return;
    }
    /* Write these captured/generated bytes to the opened local test artifact file. */
    fwrite(text, 1, strlen(text), file);
    /* Close the local file so its saved output is complete and the handle is released. */
    fclose(file);
}

/* Look up a named JSON field while preserving the exact spelling of its key. */
static const cJSON *get(const cJSON *object, const char *key)
{
    /* Return the named JSON child without changing key capitalization. */
    return cJSON_GetObjectItemCaseSensitive(object, key);
}
/* Check that a JSON value really is text and matches the expected text exactly. */
static bool string_is(const cJSON *value, const char *expected)
{
    /* Require a real JSON string, then compare its characters with the expected value. */
    return cJSON_IsString(value) && strcmp(value->valuestring, expected) == 0;
}

/* Check that phst uses captured time; use controlled inputs rather than live sensors. */
static void phst_uses_captured_time(void)
{
    /* Keep this function's prepared display values or its repeatable test event. */
    notification_message_t data = sample();
    /* Reserve a bounded character array for generated text, including its final zero byte. */
    char text[96];
    /* Keep the captured timestamp so the test can detect accidental modification. */
    time_t original = data.measured_at;
    /* Require successful formatting/building before examining the resulting text or JSON. */
    CHECK(notification_format_time(&data, text, sizeof(text)) == ESP_OK);
    /* Require an exact match with the expected display string or JSON field value. */
    CHECK(strcmp(text, "26 October 2026, 8:15:54 AM PhST") == 0);
    /* Assert this expected property; a failed CHECK records the failure and leaves this case. */
    CHECK(data.measured_at == original);
}
/* Check that phst crosses year; use controlled inputs rather than live sensors. */
static void phst_crosses_year(void)
{
    /* Keep this function's prepared display values or its repeatable test event. */
    notification_message_t data = sample();
    /* Reserve a bounded character array for generated text, including its final zero byte. */
    char text[96];
    /*
     * Set this controlled test input/flag for the scenario named by the surrounding test
     * function.
     */
    data.measured_at = (time_t)1767225599;
    /* Require successful formatting/building before examining the resulting text or JSON. */
    CHECK(notification_format_time(&data, text, sizeof(text)) == ESP_OK);
    /* Require an exact match with the expected display string or JSON field value. */
    CHECK(strcmp(text, "1 January 2026, 7:59:59 AM PhST") == 0);
}
/* Check that phst midnight and noon; use controlled inputs rather than live sensors. */
static void phst_midnight_and_noon(void)
{
    /* Keep this function's prepared display values or its repeatable test event. */
    notification_message_t data = sample();
    /* Reserve a bounded character array for generated text, including its final zero byte. */
    char text[96];
    /*
     * Set this controlled test input/flag for the scenario named by the surrounding test
     * function.
     */
    data.measured_at = (time_t)1792944000;
    /* Require successful formatting/building before examining the resulting text or JSON. */
    CHECK(notification_format_time(&data, text, sizeof(text)) == ESP_OK);
    /* Require an exact match with the expected display string or JSON field value. */
    CHECK(strcmp(text, "26 October 2026, 12:00:00 AM PhST") == 0);
    /*
     * Set this controlled test input/flag for the scenario named by the surrounding test
     * function.
     */
    data.measured_at += (time_t)(12 * 60 * 60);
    /* Require successful formatting/building before examining the resulting text or JSON. */
    CHECK(notification_format_time(&data, text, sizeof(text)) == ESP_OK);
    /* Require an exact match with the expected display string or JSON field value. */
    CHECK(strcmp(text, "26 October 2026, 12:00:00 PM PhST") == 0);
}
/* Check that unknown clock is explicit; use controlled inputs rather than live sensors. */
static void unknown_clock_is_explicit(void)
{
    /* Keep this function's prepared display values or its repeatable test event. */
    notification_message_t data = sample();
    /* Reserve a bounded character array for generated text, including its final zero byte. */
    char text[96];
    /*
     * Set this controlled test input/flag for the scenario named by the surrounding test
     * function.
     */
    data.time_valid = false;
    /* Require successful formatting/building before examining the resulting text or JSON. */
    CHECK(notification_format_time(&data, text, sizeof(text)) == ESP_OK);
    /* Require an exact match with the expected display string or JSON field value. */
    CHECK(strcmp(text, "Unavailable (clock not synchronized)") == 0);
}
/* Check that short time buffer is rejected; use controlled inputs rather than live sensors. */
static void short_time_buffer_is_rejected(void)
{
    /* Keep this function's prepared display values or its repeatable test event. */
    notification_message_t data = sample();
    /* Reserve a bounded character array for generated text, including its final zero byte. */
    char text[8];
    /* Require the intended error code for this deliberately invalid input or undersized buffer. */
    CHECK(notification_format_time(&data, text, sizeof(text)) == ESP_ERR_INVALID_SIZE);
    /* Assert this expected property; a failed CHECK records the failure and leaves this case. */
    CHECK(text[0] == '\0');
}
/* Check that plain text has complete values; use controlled inputs rather than live sensors. */
static void plain_text_has_complete_values(void)
{
    /* Keep this function's prepared display values or its repeatable test event. */
    notification_message_t data = sample();
    /* Reserve a bounded character array for generated text, including its final zero byte. */
    char text[NOTIFICATION_TEXT_CAPACITY];
    /* Require successful formatting/building before examining the resulting text or JSON. */
    CHECK(notification_format_text(&data, false, text, sizeof(text)) == ESP_OK);
    /* Require this expected content (or its absence with !) in the generated message. */
    CHECK(strstr(text, "State: DRY"));
    /* Require this expected content (or its absence with !) in the generated message. */
    CHECK(strstr(text, "Simulated moisture: 26%"));
    /* Require this expected content (or its absence with !) in the generated message. */
    CHECK(strstr(text, "Temperature: 28.0 \xC2\xB0"
                       "C"));
    /* Require this expected content (or its absence with !) in the generated message. */
    CHECK(strstr(text, "Humidity: 77.0%"));
    /* Require this expected content (or its absence with !) in the generated message. */
    CHECK(strstr(text, "26 October 2026, 8:15:54 AM PhST"));
    /* Require this expected content (or its absence with !) in the generated message. */
    CHECK(strstr(text, "UTC+08:00"));
    /* Require this expected content (or its absence with !) in the generated message. */
    CHECK(strstr(text, "Confirmed moisture state"));
    /* Require this expected content (or its absence with !) in the generated message. */
    CHECK(strstr(text, "potentiometer"));
    /* Save this generated example so it can be inspected without sending a live message. */
    save_text("notification.txt", text);
}
/* Check that failed dht keeps measured time; use controlled inputs rather than live sensors. */
static void failed_dht_keeps_measured_time(void)
{
    /* Keep this function's prepared display values or its repeatable test event. */
    notification_message_t data = sample();
    /* Reserve a bounded character array for generated text, including its final zero byte. */
    char text[NOTIFICATION_TEXT_CAPACITY];
    /*
     * Set this controlled test input/flag for the scenario named by the surrounding test
     * function.
     */
    data.environment_valid = false;
    /*
     * Supply an intentionally nonfinite test value to verify that invalid measurements are
     * handled safely.
     */
    data.temperature = NAN;
    /*
     * Supply an intentionally nonfinite test value to verify that invalid measurements are
     * handled safely.
     */
    data.humidity = NAN;
    /* Require successful formatting/building before examining the resulting text or JSON. */
    CHECK(notification_format_text(&data, false, text, sizeof(text)) == ESP_OK);
    /* Require this expected content (or its absence with !) in the generated message. */
    CHECK(strstr(text, "Temperature: Unavailable"));
    /* Require this expected content (or its absence with !) in the generated message. */
    CHECK(strstr(text, "Humidity: Unavailable"));
    /* Require this expected content (or its absence with !) in the generated message. */
    CHECK(strstr(text, "8:15:54 AM PhST"));
    /* Require this expected content (or its absence with !) in the generated message. */
    CHECK(!strstr(text, "nan"));
}
/* Check that invalid values are rejected; use controlled inputs rather than live sensors. */
static void invalid_values_are_rejected(void)
{
    /* Keep this function's prepared display values or its repeatable test event. */
    notification_message_t data = sample();
    /* Reserve a bounded character array for generated text, including its final zero byte. */
    char text[NOTIFICATION_TEXT_CAPACITY];
    /*
     * Set this controlled test input/flag for the scenario named by the surrounding test
     * function.
     */
    data.state = "<b>WET</b>";
    /* Require the intended error code for this deliberately invalid input or undersized buffer. */
    CHECK(notification_format_text(&data, true, text, sizeof(text)) == ESP_ERR_INVALID_ARG);
    /* Reset all dummy readings/flags before testing a different invalid input. */
    data = sample();
    /*
     * Set this controlled test input/flag for the scenario named by the surrounding test
     * function.
     */
    data.moisture_percent = 101;
    CHECK(notification_format_text(&data, false, text, sizeof(text)) == ESP_ERR_INVALID_ARG);
    data = sample();
    data.temperature = INFINITY;
    CHECK(notification_format_text(&data, false, text, sizeof(text)) == ESP_ERR_INVALID_ARG);
    data = sample();
    data.humidity = -1.0f;
    CHECK(notification_format_text(&data, false, text, sizeof(text)) == ESP_ERR_INVALID_ARG);
}
static void formatting_does_not_publish_truncated_text(void)
{
    notification_message_t data = sample();
    char text[12];
    CHECK(notification_format_text(&data, true, text, sizeof(text)) == ESP_ERR_INVALID_SIZE);
    CHECK(text[0] == '\0');
    CHECK(notification_format_gmail(&data, text, sizeof(text)) == ESP_ERR_INVALID_SIZE);
    CHECK(text[0] == '\0');
}
static void telegram_has_bold_labels_and_html_mode(void)
{
    notification_message_t data = sample();
    cJSON *json = NULL;
    CHECK(notification_build_telegram(&data, "demo\"chat", &json) == ESP_OK);
    CHECK(string_is(get(json, "parse_mode"), "HTML"));
    CHECK(string_is(get(json, "chat_id"), "demo\"chat"));
    const cJSON *text = get(json, "text");
    CHECK(cJSON_IsString(text));
    CHECK(strstr(text->valuestring, "<b>State:</b> DRY"));
    CHECK(strstr(text->valuestring, "<b>Measured</b>"));
    CHECK(strstr(text->valuestring, "8:15:54 AM PhST"));
    char *body = cJSON_PrintUnformatted(json);
    CHECK(body);
    cJSON *parsed = cJSON_Parse(body);
    CHECK(parsed);
    cJSON_Delete(parsed);
    save_text("telegram.json", body);
    cJSON_free(body);
    cJSON_Delete(json);
}
static void slack_has_fields_and_accessible_fallback(void)
{
    notification_message_t data = sample();
    cJSON *json = NULL;
    CHECK(notification_build_slack(&data, &json) == ESP_OK);
    CHECK(cJSON_IsArray(get(json, "blocks")));
    CHECK(cJSON_IsString(get(json, "text")));
    const cJSON *blocks = get(json, "blocks");
    CHECK(cJSON_GetArraySize(blocks) >= 6);
    CHECK(string_is(get(cJSON_GetArrayItem(blocks, 0), "type"), "header"));
    const cJSON *fields = get(cJSON_GetArrayItem(blocks, 2), "fields");
    CHECK(cJSON_GetArraySize(fields) == 4);
    CHECK(string_is(get(cJSON_GetArrayItem(fields, 0), "text"), "*State*\nDRY"));
    CHECK(strstr(get(json, "text")->valuestring, "8:15:54 AM PhST"));
    char *body = cJSON_PrintUnformatted(json);
    CHECK(body);
    save_text("slack.json", body);
    cJSON_free(body);
    cJSON_Delete(json);
}
static void discord_has_embed_and_no_mentions(void)
{
    notification_message_t data = sample();
    cJSON *json = NULL;
    CHECK(notification_build_discord(&data, &json) == ESP_OK);
    const cJSON *embeds = get(json, "embeds");
    CHECK(cJSON_GetArraySize(embeds) == 1);
    const cJSON *embed = cJSON_GetArrayItem(embeds, 0);
    CHECK(string_is(get(embed, "title"), "Plant-01"));
    CHECK(get(embed, "color")->valueint == 0xd5a34b);
    const cJSON *fields = get(embed, "fields");
    CHECK(cJSON_GetArraySize(fields) == 7);
    CHECK(string_is(get(cJSON_GetArrayItem(fields, 0), "name"), "State"));
    CHECK(string_is(get(cJSON_GetArrayItem(fields, 0), "value"), "DRY"));
    CHECK(cJSON_IsTrue(get(cJSON_GetArrayItem(fields, 0), "inline")));
    CHECK(cJSON_GetArraySize(get(get(json, "allowed_mentions"), "parse")) == 0);
    char *body = cJSON_PrintUnformatted(json);
    CHECK(body);
    save_text("discord.json", body);
    cJSON_free(body);
    cJSON_Delete(json);
}
static void gmail_matches_the_design_and_has_time(void)
{
    notification_message_t data = sample();
    char *text = malloc(NOTIFICATION_HTML_CAPACITY);
    CHECK(text);
    CHECK(notification_format_gmail(&data, text, NOTIFICATION_HTML_CAPACITY) == ESP_OK);
    CHECK(strstr(text, "<html"));
    CHECK(strstr(text, "font-family:Inter"));
    CHECK(strstr(text, "#173f32"));
    CHECK(strstr(text, "#fbefcf"));
    CHECK(strstr(text, "cid:plant-mark"));
    CHECK(strstr(text, "8:15:54 AM PhST"));
    CHECK(strstr(text, "28.0"));
    CHECK(strstr(text, "77.0%"));
    CHECK(strstr(text, "potentiometer"));
    save_text("gmail.html", text);
    free(text);
}
static void all_conditions_keep_their_own_colors(void)
{
    notification_message_t data = sample();
    cJSON *json = NULL;
    data.state = "NORMAL";
    CHECK(notification_build_discord(&data, &json) == ESP_OK);
    CHECK(get(cJSON_GetArrayItem(get(json, "embeds"), 0), "color")->valueint == 0x3d8061);
    cJSON_Delete(json);
    data.state = "WET";
    CHECK(notification_build_discord(&data, &json) == ESP_OK);
    CHECK(get(cJSON_GetArrayItem(get(json, "embeds"), 0), "color")->valueint == 0x4089b8);
    cJSON_Delete(json);
}
static void missing_dht_and_clock_render_on_every_channel(void)
{
    notification_message_t data = sample();
    cJSON *json = NULL;
    char text[NOTIFICATION_HTML_CAPACITY];
    data.environment_valid = false;
    data.time_valid = false;
    for (int channel = 0; channel < 3; channel++) {
        esp_err_t result = channel == 0   ? notification_build_slack(&data, &json)
                           : channel == 1 ? notification_build_discord(&data, &json)
                                          : notification_build_telegram(&data, "demo", &json);
        CHECK(result == ESP_OK);
        char *body = cJSON_PrintUnformatted(json);
        CHECK(body);
        CHECK(strstr(body, "Unavailable"));
        CHECK(strstr(body, "clock not synchronized"));
        cJSON_free(body);
        cJSON_Delete(json);
        json = NULL;
    }
    CHECK(notification_format_gmail(&data, text, sizeof(text)) == ESP_OK);
    CHECK(strstr(text, "clock not synchronized"));
    CHECK(strstr(text, "Unavailable"));
    save_text("gmail-unavailable.html", text);
}
static long allocation_budget;
static long allocations;
static void *limited_malloc(size_t size)
{
    if (allocation_budget-- <= 0)
        return NULL;
    void *p = malloc(size);
    if (p)
        allocations++;
    return p;
}
static void counted_free(void *p)
{
    if (p) {
        allocations--;
        free(p);
    }
}
static void allocation_failures_leave_no_partial_payload_or_leak(void)
{
    notification_message_t data = sample();
    cJSON_Hooks hooks = {limited_malloc, counted_free};
    for (int channel = 0; channel < 3; channel++) {
        bool reached_success = false;
        for (long budget = 0; budget < 400; budget++) {
            allocation_budget = budget;
            allocations = 0;
            cJSON_InitHooks(&hooks);
            cJSON *json = NULL;
            esp_err_t result = channel == 0   ? notification_build_slack(&data, &json)
                               : channel == 1 ? notification_build_discord(&data, &json)
                                              : notification_build_telegram(&data, "demo", &json);
            bool partial = result != ESP_OK && json != NULL;
            cJSON_Delete(json);
            long leaks = allocations;
            cJSON_InitHooks(NULL);
            CHECK(!partial);
            CHECK(leaks == 0);
            if (result == ESP_OK) {
                reached_success = true;
                break;
            }
            CHECK(result == ESP_ERR_NO_MEM);
        }
        CHECK(reached_success);
    }
}
static void null_arguments_fail_safely(void)
{
    notification_message_t data = sample();
    char text[128];
    cJSON *json = NULL;
    CHECK(notification_format_time(NULL, text, sizeof(text)) == ESP_ERR_INVALID_ARG);
    CHECK(notification_format_time(&data, NULL, 1) == ESP_ERR_INVALID_ARG);
    CHECK(notification_build_slack(&data, NULL) == ESP_ERR_INVALID_ARG);
    CHECK(notification_build_telegram(&data, "", &json) == ESP_ERR_INVALID_ARG);
    CHECK(json == NULL);
}

#define RUN(test)                                                                                  \
    do {                                                                                           \
        int old = failures;                                                                        \
        cases++;                                                                                   \
        test();                                                                                    \
        printf("%s %s\n", failures == old ? "PASS" : "FAIL", #test);                               \
    } while (0)
int main(int argc, char **argv)
{
    if (argc != 2)
        return 2;
    artifacts = argv[1];
    RUN(phst_uses_captured_time);
    RUN(phst_crosses_year);
    RUN(phst_midnight_and_noon);
    RUN(unknown_clock_is_explicit);
    RUN(short_time_buffer_is_rejected);
    RUN(plain_text_has_complete_values);
    RUN(failed_dht_keeps_measured_time);
    RUN(invalid_values_are_rejected);
    RUN(formatting_does_not_publish_truncated_text);
    RUN(telegram_has_bold_labels_and_html_mode);
    RUN(slack_has_fields_and_accessible_fallback);
    RUN(discord_has_embed_and_no_mentions);
    RUN(gmail_matches_the_design_and_has_time);
    RUN(all_conditions_keep_their_own_colors);
    RUN(missing_dht_and_clock_render_on_every_channel);
    RUN(allocation_failures_leave_no_partial_payload_or_leak);
    RUN(null_arguments_fail_safely);
    printf("%d cases, %d failures\n", cases, failures);
    return failures ? 1 : 0;
}
