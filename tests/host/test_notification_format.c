#include "notification_format.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
static int cases;
static const char *artifacts;
#define CHECK(condition)                                                                           \
    do {                                                                                           \
        if (!(condition)) {                                                                        \
            fprintf(stderr, "FAIL %s:%d: %s\n", __func__, __LINE__, #condition);                   \
            failures++;                                                                            \
            return;                                                                                \
        }                                                                                          \
    } while (0)

static notification_message_t sample(void)
{
    return (notification_message_t){.state = "DRY",
                                    .moisture_percent = 26,
                                    .temperature = 28.0f,
                                    .humidity = 77.0f,
                                    .environment_valid = true,
                                    .measured_at = (time_t)1792973754,
                                    .time_valid = true};
}

static void save_text(const char *name, const char *text)
{
    char filename[1024];
    int count = snprintf(filename, sizeof(filename), "%s/%s", artifacts, name);
    if (count < 0 || count >= (int)sizeof(filename)) {
        failures++;
        return;
    }
    FILE *file = fopen(filename, "wb");
    if (!file) {
        failures++;
        return;
    }
    fwrite(text, 1, strlen(text), file);
    fclose(file);
}

static const cJSON *get(const cJSON *object, const char *key)
{
    return cJSON_GetObjectItemCaseSensitive(object, key);
}
static bool string_is(const cJSON *value, const char *expected)
{
    return cJSON_IsString(value) && strcmp(value->valuestring, expected) == 0;
}

static void phst_uses_captured_time(void)
{
    notification_message_t data = sample();
    char text[96];
    time_t original = data.measured_at;
    CHECK(notification_format_time(&data, text, sizeof(text)) == ESP_OK);
    CHECK(strcmp(text, "26 October 2026, 8:15:54 AM PhST") == 0);
    CHECK(data.measured_at == original);
}
static void phst_crosses_year(void)
{
    notification_message_t data = sample();
    char text[96];
    data.measured_at = (time_t)1767225599;
    CHECK(notification_format_time(&data, text, sizeof(text)) == ESP_OK);
    CHECK(strcmp(text, "1 January 2026, 7:59:59 AM PhST") == 0);
}
static void phst_midnight_and_noon(void)
{
    notification_message_t data = sample();
    char text[96];
    data.measured_at = (time_t)1792944000;
    CHECK(notification_format_time(&data, text, sizeof(text)) == ESP_OK);
    CHECK(strcmp(text, "26 October 2026, 12:00:00 AM PhST") == 0);
    data.measured_at += (time_t)(12 * 60 * 60);
    CHECK(notification_format_time(&data, text, sizeof(text)) == ESP_OK);
    CHECK(strcmp(text, "26 October 2026, 12:00:00 PM PhST") == 0);
}
static void unknown_clock_is_explicit(void)
{
    notification_message_t data = sample();
    char text[96];
    data.time_valid = false;
    CHECK(notification_format_time(&data, text, sizeof(text)) == ESP_OK);
    CHECK(strcmp(text, "Unavailable (clock not synchronized)") == 0);
}
static void short_time_buffer_is_rejected(void)
{
    notification_message_t data = sample();
    char text[8];
    CHECK(notification_format_time(&data, text, sizeof(text)) == ESP_ERR_INVALID_SIZE);
    CHECK(text[0] == '\0');
}
static void plain_text_has_complete_values(void)
{
    notification_message_t data = sample();
    char text[NOTIFICATION_TEXT_CAPACITY];
    CHECK(notification_format_text(&data, false, text, sizeof(text)) == ESP_OK);
    CHECK(strstr(text, "State: DRY"));
    CHECK(strstr(text, "Simulated moisture: 26%"));
    CHECK(strstr(text, "Temperature: 28.0 \xC2\xB0"
                       "C"));
    CHECK(strstr(text, "Humidity: 77.0%"));
    CHECK(strstr(text, "26 October 2026, 8:15:54 AM PhST"));
    CHECK(strstr(text, "UTC+08:00"));
    CHECK(strstr(text, "Confirmed moisture state"));
    CHECK(strstr(text, "potentiometer"));
    save_text("notification.txt", text);
}
static void failed_dht_keeps_measured_time(void)
{
    notification_message_t data = sample();
    char text[NOTIFICATION_TEXT_CAPACITY];
    data.environment_valid = false;
    data.temperature = NAN;
    data.humidity = NAN;
    CHECK(notification_format_text(&data, false, text, sizeof(text)) == ESP_OK);
    CHECK(strstr(text, "Temperature: Unavailable"));
    CHECK(strstr(text, "Humidity: Unavailable"));
    CHECK(strstr(text, "8:15:54 AM PhST"));
    CHECK(!strstr(text, "nan"));
}
static void invalid_values_are_rejected(void)
{
    notification_message_t data = sample();
    char text[NOTIFICATION_TEXT_CAPACITY];
    data.state = "<b>WET</b>";
    CHECK(notification_format_text(&data, true, text, sizeof(text)) == ESP_ERR_INVALID_ARG);
    data = sample();
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
