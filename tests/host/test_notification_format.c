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
    /* Require the intended error code for this deliberately invalid input or undersized buffer. */
    CHECK(notification_format_text(&data, false, text, sizeof(text)) == ESP_ERR_INVALID_ARG);
    /* Reset all dummy readings/flags before testing a different invalid input. */
    data = sample();
    /*
     * Supply an intentionally nonfinite test value to verify that invalid measurements are
     * handled safely.
     */
    data.temperature = INFINITY;
    /* Require the intended error code for this deliberately invalid input or undersized buffer. */
    CHECK(notification_format_text(&data, false, text, sizeof(text)) == ESP_ERR_INVALID_ARG);
    /* Reset all dummy readings/flags before testing a different invalid input. */
    data = sample();
    /*
     * Set this controlled test input/flag for the scenario named by the surrounding test
     * function.
     */
    data.humidity = -1.0f;
    /* Require the intended error code for this deliberately invalid input or undersized buffer. */
    CHECK(notification_format_text(&data, false, text, sizeof(text)) == ESP_ERR_INVALID_ARG);
}
/*
 * Check that formatting does not publish truncated text; use controlled inputs rather than live
 * sensors.
 */
static void formatting_does_not_publish_truncated_text(void)
{
    /* Keep this function's prepared display values or its repeatable test event. */
    notification_message_t data = sample();
    /* Reserve a bounded character array for generated text, including its final zero byte. */
    char text[12];
    /* Require the intended error code for this deliberately invalid input or undersized buffer. */
    CHECK(notification_format_text(&data, true, text, sizeof(text)) == ESP_ERR_INVALID_SIZE);
    /* Assert this expected property; a failed CHECK records the failure and leaves this case. */
    CHECK(text[0] == '\0');
    /* Require the intended error code for this deliberately invalid input or undersized buffer. */
    CHECK(notification_format_gmail(&data, text, sizeof(text)) == ESP_ERR_INVALID_SIZE);
    /* Assert this expected property; a failed CHECK records the failure and leaves this case. */
    CHECK(text[0] == '\0');
}
/*
 * Check that telegram has bold labels and html mode; use controlled inputs rather than live
 * sensors.
 */
static void telegram_has_bold_labels_and_html_mode(void)
{
    /* Keep this function's prepared display values or its repeatable test event. */
    notification_message_t data = sample();
    /* Start with no JSON tree; the builder supplies one only after it finishes successfully. */
    cJSON *json = NULL;
    /* Require successful formatting/building before examining the resulting text or JSON. */
    CHECK(notification_build_telegram(&data, "demo\"chat", &json) == ESP_OK);
    /* Require an exact match with the expected display string or JSON field value. */
    CHECK(string_is(get(json, "parse_mode"), "HTML"));
    /* Require an exact match with the expected display string or JSON field value. */
    CHECK(string_is(get(json, "chat_id"), "demo\"chat"));
    /* Look up Telegram's generated text field, then check its type and content below. */
    const cJSON *text = get(json, "text");
    /* Assert this expected property; a failed CHECK records the failure and leaves this case. */
    CHECK(cJSON_IsString(text));
    /* Require this expected content (or its absence with !) in the generated message. */
    CHECK(strstr(text->valuestring, "<b>State:</b> DRY"));
    /* Require this expected content (or its absence with !) in the generated message. */
    CHECK(strstr(text->valuestring, "<b>Measured</b>"));
    /* Require this expected content (or its absence with !) in the generated message. */
    CHECK(strstr(text->valuestring, "8:15:54 AM PhST"));
    /* Turn the JSON tree into compact request text with correctly escaped string values. */
    char *body = cJSON_PrintUnformatted(json);
    /* Assert this expected property; a failed CHECK records the failure and leaves this case. */
    CHECK(body);
    /* Read serialized JSON back into a tree to verify that escaping produced valid JSON. */
    cJSON *parsed = cJSON_Parse(body);
    /* Assert this expected property; a failed CHECK records the failure and leaves this case. */
    CHECK(parsed);
    /*
     * Free this JSON tree and all children already attached to it; this does not free serialized
     * text.
     */
    cJSON_Delete(parsed);
    /* Save this generated example so it can be inspected without sending a live message. */
    save_text("telegram.json", body);
    /* Free the separately allocated JSON request text after the client has finished using it. */
    cJSON_free(body);
    /*
     * Free this JSON tree and all children already attached to it; this does not free serialized
     * text.
     */
    cJSON_Delete(json);
}
/*
 * Check that Slack readings stack and retain their accessible fallback using controlled inputs.
 */
static void slack_stacks_readings_with_accessible_fallback(void)
{
    /* Build the real payload; native section text must remain readable without a two-column grid. */
    notification_message_t data = sample();
    cJSON *json = NULL;
    CHECK(notification_build_slack(&data, &json) == ESP_OK);
    const cJSON *blocks = get(json, "blocks");
    CHECK(cJSON_IsArray(blocks));
    CHECK(string_is(get(cJSON_GetArrayItem(blocks, 0), "type"), "header"));
    /* A fields array asks Slack for columns, so none of these blocks may contain one. */
    for (int i = 0; i < cJSON_GetArraySize(blocks); i++) {
        CHECK(get(cJSON_GetArrayItem(blocks, i), "fields") == NULL);
    }
    /* Keep every reading in a full-width text section with bold labels and ordinary values. */
    const cJSON *readings = get(get(cJSON_GetArrayItem(blocks, 2), "text"), "text");
    CHECK(cJSON_IsString(readings));
    CHECK(strstr(readings->valuestring, "*State:* DRY"));
    CHECK(strstr(readings->valuestring, "*Simulated moisture:* 26%"));
    CHECK(strstr(readings->valuestring, "*Temperature:* 28.0"));
    CHECK(strstr(readings->valuestring, "*Humidity:* 77.0%"));
    /* Screen readers and notification previews still receive the complete plain-text alternative. */
    CHECK(strstr(get(json, "text")->valuestring, "8:15:54 AM PhST"));
    char *body = cJSON_PrintUnformatted(json);
    CHECK(body);
    save_text("slack.json", body);
    cJSON_free(body);
    cJSON_Delete(json);
}
/* Check that discord has embed and no mentions; use controlled inputs rather than live sensors. */
static void discord_stacks_fields_without_spacers_or_mentions(void)
{
    /* Build the same event as a native Discord card; no blank alignment field is needed. */
    notification_message_t data = sample();
    cJSON *json = NULL;
    CHECK(notification_build_discord(&data, &json) == ESP_OK);
    const cJSON *embeds = get(json, "embeds");
    CHECK(cJSON_GetArraySize(embeds) == 1);
    const cJSON *embed = cJSON_GetArrayItem(embeds, 0);
    CHECK(string_is(get(embed, "title"), "Plant-01"));
    CHECK(get(embed, "color")->valueint == 0xd5a34b);
    const cJSON *fields = get(embed, "fields");
    const char *names[] = {"State",    "Simulated moisture", "Temperature",
                           "Humidity", "Measured",           "Reason"};
    CHECK(cJSON_GetArraySize(fields) == 6);
    /* Full-width fields preserve their reading order on both narrow and wide Discord clients. */
    for (int i = 0; i < 6; i++) {
        const cJSON *field = cJSON_GetArrayItem(fields, i);
        CHECK(string_is(get(field, "name"), names[i]));
        CHECK(cJSON_IsFalse(get(field, "inline")));
    }
    CHECK(string_is(get(cJSON_GetArrayItem(fields, 0), "value"), "DRY"));
    CHECK(strstr(get(cJSON_GetArrayItem(fields, 4), "value")->valuestring, "8:15:54 AM PhST"));
    CHECK(cJSON_GetArraySize(get(get(json, "allowed_mentions"), "parse")) == 0);
    char *body = cJSON_PrintUnformatted(json);
    CHECK(body);
    save_text("discord.json", body);
    cJSON_free(body);
    cJSON_Delete(json);
}
/*
 * Check that gmail matches the design and has time; use controlled inputs rather than live
 * sensors.
 */
static void gmail_matches_the_design_and_has_time(void)
{
    /* Keep this function's prepared display values or its repeatable test event. */
    notification_message_t data = sample();
    /* Reserve a bounded character array for generated text, including its final zero byte. */
    char *text = malloc(NOTIFICATION_HTML_CAPACITY);
    /* Assert this expected property; a failed CHECK records the failure and leaves this case. */
    CHECK(text);
    /* Require successful formatting/building before examining the resulting text or JSON. */
    CHECK(notification_format_gmail(&data, text, NOTIFICATION_HTML_CAPACITY) == ESP_OK);
    /* Require this expected content (or its absence with !) in the generated message. */
    CHECK(strstr(text, "<html"));
    /* Require this expected content (or its absence with !) in the generated message. */
    CHECK(strstr(text, "font-family:Inter"));
    /* Require this expected content (or its absence with !) in the generated message. */
    CHECK(strstr(text, "#173f32"));
    /* Require this expected content (or its absence with !) in the generated message. */
    CHECK(strstr(text, "#fbefcf"));
    /* Require this expected content (or its absence with !) in the generated message. */
    CHECK(strstr(text, "cid:plant-mark"));
    /* Require this expected content (or its absence with !) in the generated message. */
    CHECK(strstr(text, "8:15:54 AM PhST"));
    /* Require this expected content (or its absence with !) in the generated message. */
    CHECK(strstr(text, "28.0"));
    /* Require this expected content (or its absence with !) in the generated message. */
    CHECK(strstr(text, "77.0%"));
    /* Require this expected content (or its absence with !) in the generated message. */
    CHECK(strstr(text, "potentiometer"));
    /* Save this generated example so it can be inspected without sending a live message. */
    save_text("gmail.html", text);
    /* Release this temporary heap allocation; free(NULL) is safe for partial-allocation cleanup. */
    free(text);
}
/*
 * Check that all conditions keep their own colors; use controlled inputs rather than live
 * sensors.
 */
static void all_conditions_keep_their_own_colors(void)
{
    /* Keep this function's prepared display values or its repeatable test event. */
    notification_message_t data = sample();
    /* Start with no JSON tree; the builder supplies one only after it finishes successfully. */
    cJSON *json = NULL;
    /*
     * Set this controlled test input/flag for the scenario named by the surrounding test
     * function.
     */
    data.state = "NORMAL";
    data.moisture_percent = 50;
    /* Require successful formatting/building before examining the resulting text or JSON. */
    CHECK(notification_build_discord(&data, &json) == ESP_OK);
    /* Assert this expected property; a failed CHECK records the failure and leaves this case. */
    CHECK(get(cJSON_GetArrayItem(get(json, "embeds"), 0), "color")->valueint == 0x3d8061);
    /*
     * Free this JSON tree and all children already attached to it; this does not free serialized
     * text.
     */
    cJSON_Delete(json);
    /*
     * Set this controlled test input/flag for the scenario named by the surrounding test
     * function.
     */
    /* Save real NORMAL HTML so the longer status badge is checked on a narrow screen. */
    char html[NOTIFICATION_HTML_CAPACITY];
    CHECK(notification_format_gmail(&data, html, sizeof(html)) == ESP_OK);
    save_text("gmail-normal-state.html", html);
    data.state = "WET";
    data.moisture_percent = 100;
    /* Require successful formatting/building before examining the resulting text or JSON. */
    CHECK(notification_build_discord(&data, &json) == ESP_OK);
    /* Assert this expected property; a failed CHECK records the failure and leaves this case. */
    CHECK(get(cJSON_GetArrayItem(get(json, "embeds"), 0), "color")->valueint == 0x4089b8);
    /*
     * Free this JSON tree and all children already attached to it; this does not free serialized
     * text.
     */
    cJSON_Delete(json);
    /* A three-digit moisture value must also fit without squeezing the air readings. */
    CHECK(notification_format_gmail(&data, html, sizeof(html)) == ESP_OK);
    save_text("gmail-wet-state.html", html);
}
/*
 * Check that missing dht and clock render on every channel; use controlled inputs rather than
 * live sensors.
 */
static void missing_dht_and_clock_render_on_every_channel(void)
{
    /* Keep this function's prepared display values or its repeatable test event. */
    notification_message_t data = sample();
    /* Start with no JSON tree; the builder supplies one only after it finishes successfully. */
    cJSON *json = NULL;
    /* Reserve a bounded character array for generated text, including its final zero byte. */
    char text[NOTIFICATION_HTML_CAPACITY];
    /*
     * Set this controlled test input/flag for the scenario named by the surrounding test
     * function.
     */
    data.environment_valid = false;
    /*
     * Set this controlled test input/flag for the scenario named by the surrounding test
     * function.
     */
    data.time_valid = false;
    /* Run the same offline checks for Slack, Discord, and Telegram JSON builders. */
    for (int channel = 0; channel < 3; channel++) {
        /* Build Slack fields/fallback from this captured event; check its result before sending. */
        esp_err_t result = channel == 0 ? notification_build_slack(&data, &json)
                           /*
                            * Build the Discord embed from this captured event; the caller owns a
                            * successful tree.
                            */
                           : channel == 1 ? notification_build_discord(&data, &json)
                                          /*
                                           * Build Telegram chat_id/text/parse_mode fields from
                                           * this captured event.
                                           */
                                          : notification_build_telegram(&data, "demo", &json);
        /* Require successful formatting/building before examining the resulting text or JSON. */
        CHECK(result == ESP_OK);
        /* Turn the JSON tree into compact request text with correctly escaped string values. */
        char *body = cJSON_PrintUnformatted(json);
        /*
         * Assert this expected property; a failed CHECK records the failure and leaves this
         * case.
         */
        CHECK(body);
        /* Require this expected content (or its absence with !) in the generated message. */
        CHECK(strstr(body, "Unavailable"));
        /* Require this expected content (or its absence with !) in the generated message. */
        CHECK(strstr(body, "clock not synchronized"));
        /*
         * Free the separately allocated JSON request text after the client has finished using
         * it.
         */
        cJSON_free(body);
        /*
         * Free this JSON tree and all children already attached to it; this does not free
         * serialized text.
         */
        cJSON_Delete(json);
        /* Forget the already-deleted JSON pointer before building the next test payload. */
        json = NULL;
    }
    /* Require successful formatting/building before examining the resulting text or JSON. */
    CHECK(notification_format_gmail(&data, text, sizeof(text)) == ESP_OK);
    /* Require this expected content (or its absence with !) in the generated message. */
    CHECK(strstr(text, "clock not synchronized"));
    /* Require this expected content (or its absence with !) in the generated message. */
    CHECK(strstr(text, "Unavailable"));
    /* Save this generated example so it can be inspected without sending a live message. */
    save_text("gmail-unavailable.html", text);
}
/* Limit how many allocations the next test builder is allowed to make. */
static long allocation_budget;
/* Count live test allocations so cleanup can be checked for leaks. */
static long allocations;
/* Deliberately fail allocations after a chosen budget to test low-memory error paths. */
static void *limited_malloc(size_t size)
{
    /* Spend one allocation slot; return NULL once the deliberate test budget is exhausted. */
    if (allocation_budget-- <= 0)
        /* Return no object/pointer; the caller must handle this failed creation. */
        return NULL;
    /* Try this budgeted allocation and retain the pointer for the leak counter. */
    void *p = malloc(size);
    /* Only count a test allocation when malloc actually returned storage. */
    if (p)
        /* Count this successful test allocation so its matching free can be verified. */
        allocations++;
    /* Return the successful test allocation (or NULL if allocation failed). */
    return p;
}
/* Free a test allocation and reduce the count used to detect leaked JSON memory. */
static void counted_free(void *p)
{
    /* Only decrement the live-allocation count for a pointer that really owns memory. */
    if (p) {
        /* Record that one tracked allocation has been released. */
        allocations--;
        /*
         * Release this temporary heap allocation; free(NULL) is safe for partial-allocation
         * cleanup.
         */
        free(p);
    }
}
/*
 * Check that allocation failures leave no partial payload or leak; use controlled inputs rather
 * than live sensors.
 */
static void allocation_failures_leave_no_partial_payload_or_leak(void)
{
    /* Keep this function's prepared display values or its repeatable test event. */
    notification_message_t data = sample();
    /*
     * Replace cJSON allocation/free functions temporarily with the deliberate-failure test
     * versions.
     */
    cJSON_Hooks hooks = {limited_malloc, counted_free};
    /* Run the same offline checks for Slack, Discord, and Telegram JSON builders. */
    for (int channel = 0; channel < 3; channel++) {
        /* Track that increasing memory budgets eventually allows this builder to finish. */
        bool reached_success = false;
        /*
         * Increase the allowed allocation count to exercise every failure point before the
         * builder succeeds.
         */
        for (long budget = 0; budget < 400; budget++) {
            /* Set how many cJSON allocations this fault-injection attempt is allowed to make. */
            allocation_budget = budget;
            /* Start this fault-injection attempt with no live tracked allocations. */
            allocations = 0;
            /* Use the test allocator so this run can fail at a chosen memory-allocation step. */
            cJSON_InitHooks(&hooks);
            /*
             * Start with no JSON tree; the builder supplies one only after it finishes
             * successfully.
             */
            cJSON *json = NULL;
            /*
             * Build Slack fields/fallback from this captured event; check its result before
             * sending.
             */
            esp_err_t result = channel == 0 ? notification_build_slack(&data, &json)
                               /*
                                * Build the Discord embed from this captured event; the caller
                                * owns a successful tree.
                                */
                               : channel == 1 ? notification_build_discord(&data, &json)
                                              /*
                                               * Build Telegram chat_id/text/parse_mode fields
                                               * from this captured event.
                                               */
                                              : notification_build_telegram(&data, "demo", &json);
            /* Detect an invalid result that still exposes a partially built JSON payload. */
            bool partial = result != ESP_OK && json != NULL;
            /*
             * Free this JSON tree and all children already attached to it; this does not free
             * serialized text.
             */
            cJSON_Delete(json);
            /*
             * Save the outstanding allocation count before restoring cJSON's normal memory
             * functions.
             */
            long leaks = allocations;
            /* Restore cJSON's normal allocation/free functions after the fault-injection check. */
            cJSON_InitHooks(NULL);
            /*
             * Assert this expected property; a failed CHECK records the failure and leaves this
             * case.
             */
            CHECK(!partial);
            /* Require that all test allocations were released after this success/failure path. */
            CHECK(leaks == 0);
            /* Continue to the next step only if the preceding operation succeeded. */
            if (result == ESP_OK) {
                /*
                 * Remember that the builder eventually completed with sufficient allocation
                 * budget.
                 */
                reached_success = true;
                /*
                 * Leave the nearest loop or switch; do not execute its remaining steps for this
                 * item.
                 */
                break;
            }
            /*
             * Require the intended error code for this deliberately invalid input or undersized
             * buffer.
             */
            CHECK(result == ESP_ERR_NO_MEM);
        }
        /*
         * Assert this expected property; a failed CHECK records the failure and leaves this
         * case.
         */
        CHECK(reached_success);
    }
}
/* Check that null arguments fail safely; use controlled inputs rather than live sensors. */
static void null_arguments_fail_safely(void)
{
    /* Keep this function's prepared display values or its repeatable test event. */
    notification_message_t data = sample();
    /* Reserve a bounded character array for generated text, including its final zero byte. */
    char text[128];
    /* Start with no JSON tree; the builder supplies one only after it finishes successfully. */
    cJSON *json = NULL;
    /* Require the intended error code for this deliberately invalid input or undersized buffer. */
    CHECK(notification_format_time(NULL, text, sizeof(text)) == ESP_ERR_INVALID_ARG);
    /* Require the intended error code for this deliberately invalid input or undersized buffer. */
    CHECK(notification_format_time(&data, NULL, 1) == ESP_ERR_INVALID_ARG);
    /* Require the intended error code for this deliberately invalid input or undersized buffer. */
    CHECK(notification_build_slack(&data, NULL) == ESP_ERR_INVALID_ARG);
    /* Require the intended error code for this deliberately invalid input or undersized buffer. */
    CHECK(notification_build_telegram(&data, "", &json) == ESP_ERR_INVALID_ARG);
    /* Assert this expected property; a failed CHECK records the failure and leaves this case. */
    CHECK(json == NULL);
}

/* Remember the failure count, execute one case, and print whether that case added a failure. */
#define RUN(test)                                                                                  \
    do {                                                                                           \
        int old = failures;                                                                        \
        cases++;                                                                                   \
        test();                                                                                    \
        printf("%s %s\n", failures == old ? "PASS" : "FAIL", #test);                               \
    } while (0)
/*
 * Computer-side test entry point: validate arguments, run this executable's checks, and return
 * pass/fail.
 */
int main(int argc, char **argv)
{
    /* Require the expected test command-line arguments before reading argv entries. */
    if (argc != 2)
        /*
         * Return a nonzero process status so the computer-side runner treats this test
         * invocation as failed.
         */
        return 2;
    /* Use the output folder supplied by the Python runner for generated example files. */
    artifacts = argv[1];
    /* Run this named host test and print PASS/FAIL while updating the case count. */
    RUN(phst_uses_captured_time);
    /* Run this named host test and print PASS/FAIL while updating the case count. */
    RUN(phst_crosses_year);
    /* Run this named host test and print PASS/FAIL while updating the case count. */
    RUN(phst_midnight_and_noon);
    /* Run this named host test and print PASS/FAIL while updating the case count. */
    RUN(unknown_clock_is_explicit);
    /* Run this named host test and print PASS/FAIL while updating the case count. */
    RUN(short_time_buffer_is_rejected);
    /* Run this named host test and print PASS/FAIL while updating the case count. */
    RUN(plain_text_has_complete_values);
    /* Run this named host test and print PASS/FAIL while updating the case count. */
    RUN(failed_dht_keeps_measured_time);
    /* Run this named host test and print PASS/FAIL while updating the case count. */
    RUN(invalid_values_are_rejected);
    /* Run this named host test and print PASS/FAIL while updating the case count. */
    RUN(formatting_does_not_publish_truncated_text);
    /* Run this named host test and print PASS/FAIL while updating the case count. */
    RUN(telegram_has_bold_labels_and_html_mode);
    /* Run this named host test and print PASS/FAIL while updating the case count. */
    RUN(slack_stacks_readings_with_accessible_fallback);
    /* Run this named host test and print PASS/FAIL while updating the case count. */
    RUN(discord_stacks_fields_without_spacers_or_mentions);
    /* Run this named host test and print PASS/FAIL while updating the case count. */
    RUN(gmail_matches_the_design_and_has_time);
    /* Run this named host test and print PASS/FAIL while updating the case count. */
    RUN(all_conditions_keep_their_own_colors);
    /* Run this named host test and print PASS/FAIL while updating the case count. */
    RUN(missing_dht_and_clock_render_on_every_channel);
    /* Run this named host test and print PASS/FAIL while updating the case count. */
    RUN(allocation_failures_leave_no_partial_payload_or_leak);
    /* Run this named host test and print PASS/FAIL while updating the case count. */
    RUN(null_arguments_fail_safely);
    /* Print the test progress/summary; the process exit code still determines pass or fail. */
    printf("%d cases, %d failures\n", cases, failures);
    /* Return a failing exit status if any host test failed; otherwise return success. */
    return failures ? 1 : 0;
}
