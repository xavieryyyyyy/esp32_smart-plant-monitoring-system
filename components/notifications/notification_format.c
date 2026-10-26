/*
 * Turn one captured reading into four readable service layouts.
 * app_tasks.c passes the original values; the sender files use these prepared messages.
 * No clock, sensor, network, or secret is changed here.
 */
#include "notification_format.h"
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    char measured[96];
    char moisture[16];
    char temperature[32];
    char humidity[24];
    const char *offset;
    const char *badge_background;
    const char *badge_foreground;
    int color;
} display_fields_t;

static const char *const months[] = {"January",   "February", "March",    "April",
                                     "May",       "June",     "July",     "August",
                                     "September", "October",  "November", "December"};

/* Leave an empty output on failure so a caller cannot accidentally send cut-off text. */
static esp_err_t write_text(char *text, size_t size, const char *format, ...)
    __attribute__((format(printf, 3, 4)));
static esp_err_t write_text(char *text, size_t size, const char *format, ...)
{
    if (text == NULL || size == 0)
        return ESP_ERR_INVALID_ARG;
    text[0] = '\0';
    va_list args;
    va_start(args, format);
    int length = vsnprintf(text, size, format, args);
    va_end(args);
    if (length < 0 || (size_t)length >= size) {
        text[0] = '\0';
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

esp_err_t notification_format_time(const notification_message_t *message, char *text, size_t size)
{
    if (text == NULL || size == 0)
        return ESP_ERR_INVALID_ARG;
    text[0] = '\0';
    if (message == NULL)
        return ESP_ERR_INVALID_ARG;
    if (!message->time_valid)
        return write_text(text, size, "Unavailable (clock not synchronized)");

    /* Add eight hours to a COPY. Never change the captured timestamp or global timezone. */
    int64_t seconds = (int64_t)message->measured_at;
    if (seconds > INT64_MAX - 28800)
        return ESP_ERR_INVALID_ARG;
    int64_t phst_seconds = seconds + 28800;
    time_t phst = (time_t)phst_seconds;
    struct tm local;
    if ((int64_t)phst != phst_seconds || gmtime_r(&phst, &local) == NULL || local.tm_mon < 0 ||
        local.tm_mon > 11 || local.tm_year > INT_MAX - 1900)
        return ESP_ERR_INVALID_ARG;
    int hour = local.tm_hour % 12;
    return write_text(text, size, "%d %s %d, %d:%02d:%02d %s PhST", local.tm_mday,
                      months[local.tm_mon], local.tm_year + 1900, hour == 0 ? 12 : hour,
                      local.tm_min, local.tm_sec, local.tm_hour < 12 ? "AM" : "PM");
}

static esp_err_t prepare(const notification_message_t *message, display_fields_t *fields)
{
    if (message == NULL || message->state == NULL || message->moisture_percent < 0 ||
        message->moisture_percent > 100)
        return ESP_ERR_INVALID_ARG;
    if (strcmp(message->state, "DRY") == 0) {
        fields->color = 0xd5a34b;
        fields->badge_background = "#fbefcf";
        fields->badge_foreground = "#6c4308";
    } else if (strcmp(message->state, "NORMAL") == 0) {
        fields->color = 0x3d8061;
        fields->badge_background = "#e4f2e8";
        fields->badge_foreground = "#235c3a";
    } else if (strcmp(message->state, "WET") == 0) {
        fields->color = 0x4089b8;
        fields->badge_background = "#e7f1fb";
        fields->badge_foreground = "#205578";
    } else {
        /* Only these three fixed words may enter the service's formatting tags. */
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t result =
        notification_format_time(message, fields->measured, sizeof(fields->measured));
    if (result != ESP_OK)
        return result;
    fields->offset = message->time_valid ? "\nUTC+08:00" : "";
    result =
        write_text(fields->moisture, sizeof(fields->moisture), "%d%%", message->moisture_percent);
    if (result != ESP_OK)
        return result;
    if (!message->environment_valid) {
        strcpy(fields->temperature, "Unavailable");
        strcpy(fields->humidity, "Unavailable");
        return ESP_OK;
    }
    if (!isfinite(message->temperature) || !isfinite(message->humidity) || message->humidity < 0 ||
        message->humidity > 100)
        return ESP_ERR_INVALID_ARG;
    result = write_text(fields->temperature, sizeof(fields->temperature),
                        "%.1f \xC2\xB0"
                        "C",
                        (double)message->temperature);
    if (result != ESP_OK)
        return result;
    return write_text(fields->humidity, sizeof(fields->humidity), "%.1f%%",
                      (double)message->humidity);
}

esp_err_t notification_format_text(const notification_message_t *message, bool html, char *text,
                                   size_t size)
{
    if (text == NULL || size == 0)
        return ESP_ERR_INVALID_ARG;
    text[0] = '\0';
    display_fields_t fields;
    esp_err_t result = prepare(message, &fields);
    if (result != ESP_OK)
        return result;
    /* All inserted words are validated states, formatted numbers, or our own fixed text. */
    if (html) {
        return write_text(text, size,
                          "<b>Plant-01</b>\n<i>Condition update</i>\n\n"
                          "<b>State:</b> %s\n<b>Simulated moisture:</b> %s\n\n"
                          "<b>Temperature:</b> %s\n<b>Humidity:</b> %s\n\n"
                          "<b>Measured</b>\n%s%s\n\n<b>Reason</b>\nConfirmed moisture state\n\n"
                          "<i>Moisture is simulated using a potentiometer.</i>",
                          message->state, fields.moisture, fields.temperature, fields.humidity,
                          fields.measured, fields.offset);
    }
    return write_text(
        text, size,
        "Plant-01\nCondition update\n\nState: %s\nSimulated moisture: %s\n"
        "Temperature: %s\nHumidity: %s\n\nMeasured: %s%s\n"
        "Reason: Confirmed moisture state\nMoisture is simulated using a potentiometer.",
        message->state, fields.moisture, fields.temperature, fields.humidity, fields.measured,
        fields.offset);
}

esp_err_t notification_format_gmail(const notification_message_t *message, char *html, size_t size)
{
    if (html == NULL || size == 0)
        return ESP_ERR_INVALID_ARG;
    html[0] = '\0';
    display_fields_t fields;
    esp_err_t result = prepare(message, &fields);
    if (result != ESP_OK)
        return result;
    /* Inline styles and presentation tables work in email clients without JavaScript. */
    return write_text(
        html, size,
        "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"></head>"
        "<body "
        "style=\"margin:0;background:#f5f8f5;color:#172d25;font-family:Inter,Arial,sans-serif\">"
        "<table role=\"presentation\" width=\"100%%\" cellpadding=\"0\" cellspacing=\"0\"><tr>"
        "<td align=\"center\" style=\"padding:20px 12px\">"
        "<table role=\"presentation\" width=\"100%%\" cellpadding=\"0\" cellspacing=\"0\" "
        "style=\"max-width:620px;background:#ffffff;border-radius:12px;overflow:hidden\">"
        "<tr><td colspan=\"2\" style=\"padding:26px;background:#173f32;color:#ffffff\">"
        "<div style=\"font-size:14px;color:#d3e5d7;margin-bottom:22px\">"
        "<img src=\"cid:plant-mark\" width=\"20\" height=\"20\" alt=\"\" "
        "style=\"vertical-align:middle;margin-right:8px\">Plant Monitor</div>"
        "<table role=\"presentation\" width=\"100%%\"><tr><td>"
        "<h1 style=\"margin:0;color:#ffffff;font-size:32px;font-weight:600\">Plant-01</h1>"
        "<p style=\"margin:8px 0 0;font-size:15px;color:#d3e5d7\">Condition update</p></td>"
        "<td align=\"right\"><span style=\"display:inline-block;padding:8px 14px;border-radius:5px;"
        "background:%s;color:%s;font-size:16px;font-weight:600\">%s</span></td></tr></table></td></"
        "tr>"
        "<tr><td colspan=\"2\" style=\"padding:26px\">"
        "<table role=\"presentation\" width=\"100%%\" cellpadding=\"0\" cellspacing=\"0\">"
        "<tr><td style=\"width:45%%;vertical-align:top\">"
        "<div style=\"font-size:14px;font-weight:600;color:#4e6256\">Simulated moisture</div>"
        "<div "
        "style=\"font-size:64px;line-height:1.2;font-weight:500;margin-top:12px\">%s</div></td>"
        "<td style=\"vertical-align:top;padding-left:18px\">"
        "<div style=\"font-size:14px;font-weight:600;color:#4e6256\">Temperature</div>"
        "<div style=\"font-size:23px;margin:8px 0 18px\">%s</div>"
        "<div style=\"font-size:14px;font-weight:600;color:#4e6256\">Humidity</div>"
        "<div style=\"font-size:23px;margin-top:8px\">%s</div></td></tr></table>"
        "<div style=\"border-top:1px solid "
        "#d8e1db;padding-top:22px;margin-top:22px;font-size:15px;line-height:1.6\">"
        "<b>Measured</b><br>%s%s<br><br><b>Reason</b><br>Confirmed moisture state</div>"
        "<p style=\"border-top:1px solid #d8e1db;margin:22px 0 0;padding-top:18px;"
        "font-size:13px;line-height:1.6;color:#53695b\">Moisture is simulated using a "
        "potentiometer.</p>"
        "</td></tr></table></td></tr></table></body></html>",
        fields.badge_background, fields.badge_foreground, message->state, fields.moisture,
        fields.temperature, fields.humidity, fields.measured,
        message->time_valid ? "<br><span style=\"color:#53695b;font-size:13px\">UTC+08:00</span>"
                            : "");
}

/* These helpers attach children immediately, so deleting the root cleans up any failure. */
static cJSON *slack_block(cJSON *blocks, const char *type)
{
    cJSON *block = cJSON_CreateObject();
    if (block == NULL)
        return NULL;
    if (!cJSON_AddItemToArray(blocks, block)) {
        cJSON_Delete(block);
        return NULL;
    }
    return cJSON_AddStringToObject(block, "type", type) != NULL ? block : NULL;
}
static bool slack_text(cJSON *parent, const char *key, const char *type, const char *text)
{
    cJSON *object = cJSON_AddObjectToObject(parent, key);
    return object != NULL && cJSON_AddStringToObject(object, "type", type) != NULL &&
           cJSON_AddStringToObject(object, "text", text) != NULL;
}
static bool slack_field(cJSON *fields, const char *label, const char *value)
{
    char text[192];
    if (write_text(text, sizeof(text), "*%s*\n%s", label, value) != ESP_OK)
        return false;
    cJSON *field = cJSON_CreateObject();
    if (field == NULL)
        return false;
    if (!cJSON_AddItemToArray(fields, field)) {
        cJSON_Delete(field);
        return false;
    }
    return cJSON_AddStringToObject(field, "type", "mrkdwn") != NULL &&
           cJSON_AddStringToObject(field, "text", text) != NULL;
}

esp_err_t notification_build_slack(const notification_message_t *message, cJSON **json)
{
    if (json == NULL)
        return ESP_ERR_INVALID_ARG;
    *json = NULL;
    display_fields_t data;
    esp_err_t result = prepare(message, &data);
    if (result != ESP_OK)
        return result;
    char fallback[NOTIFICATION_TEXT_CAPACITY];
    result = notification_format_text(message, false, fallback, sizeof(fallback));
    if (result != ESP_OK)
        return result;
    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
        return ESP_ERR_NO_MEM;
    if (cJSON_AddStringToObject(root, "text", fallback) == NULL)
        goto no_memory;
    cJSON *blocks = cJSON_AddArrayToObject(root, "blocks");
    if (blocks == NULL)
        goto no_memory;
    cJSON *block = slack_block(blocks, "header");
    if (block == NULL || !slack_text(block, "text", "plain_text", "Plant-01"))
        goto no_memory;
    block = slack_block(blocks, "section");
    if (block == NULL || !slack_text(block, "text", "mrkdwn", "Condition update"))
        goto no_memory;
    block = slack_block(blocks, "section");
    if (block == NULL)
        goto no_memory;
    cJSON *fields = cJSON_AddArrayToObject(block, "fields");
    if (fields == NULL || !slack_field(fields, "State", message->state) ||
        !slack_field(fields, "Simulated moisture", data.moisture) ||
        !slack_field(fields, "Temperature", data.temperature) ||
        !slack_field(fields, "Humidity", data.humidity))
        goto no_memory;
    if (slack_block(blocks, "divider") == NULL)
        goto no_memory;
    char time[192];
    if (write_text(time, sizeof(time), "*Measured*\n%s%s", data.measured, data.offset) != ESP_OK)
        goto no_memory;
    block = slack_block(blocks, "section");
    if (block == NULL || !slack_text(block, "text", "mrkdwn", time))
        goto no_memory;
    block = slack_block(blocks, "section");
    if (block == NULL || !slack_text(block, "text", "mrkdwn", "*Reason*\nConfirmed moisture state"))
        goto no_memory;
    block = slack_block(blocks, "context");
    if (block == NULL)
        goto no_memory;
    cJSON *elements = cJSON_AddArrayToObject(block, "elements");
    cJSON *element = cJSON_CreateObject();
    if (element == NULL)
        goto no_memory;
    if (elements == NULL || !cJSON_AddItemToArray(elements, element)) {
        cJSON_Delete(element);
        goto no_memory;
    }
    if (cJSON_AddStringToObject(element, "type", "plain_text") == NULL ||
        cJSON_AddStringToObject(element, "text", "Moisture is simulated using a potentiometer.") ==
            NULL)
        goto no_memory;
    *json = root;
    return ESP_OK;
no_memory:
    cJSON_Delete(root);
    return ESP_ERR_NO_MEM;
}

static bool discord_field(cJSON *fields, const char *label, const char *value, bool inline_field)
{
    cJSON *field = cJSON_CreateObject();
    if (field == NULL)
        return false;
    if (!cJSON_AddItemToArray(fields, field)) {
        cJSON_Delete(field);
        return false;
    }
    return cJSON_AddStringToObject(field, "name", label) != NULL &&
           cJSON_AddStringToObject(field, "value", value) != NULL &&
           cJSON_AddBoolToObject(field, "inline", inline_field) != NULL;
}
esp_err_t notification_build_discord(const notification_message_t *message, cJSON **json)
{
    if (json == NULL)
        return ESP_ERR_INVALID_ARG;
    *json = NULL;
    display_fields_t data;
    esp_err_t result = prepare(message, &data);
    if (result != ESP_OK)
        return result;
    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
        return ESP_ERR_NO_MEM;
    cJSON *mentions = cJSON_AddObjectToObject(root, "allowed_mentions");
    if (mentions == NULL || cJSON_AddArrayToObject(mentions, "parse") == NULL)
        goto no_memory;
    cJSON *embeds = cJSON_AddArrayToObject(root, "embeds");
    cJSON *embed = cJSON_CreateObject();
    if (embed == NULL)
        goto no_memory;
    if (embeds == NULL || !cJSON_AddItemToArray(embeds, embed)) {
        cJSON_Delete(embed);
        goto no_memory;
    }
    if (cJSON_AddStringToObject(embed, "title", "Plant-01") == NULL ||
        cJSON_AddStringToObject(embed, "description", "Condition update") == NULL ||
        cJSON_AddNumberToObject(embed, "color", data.color) == NULL)
        goto no_memory;
    cJSON *fields = cJSON_AddArrayToObject(embed, "fields");
    char time[192];
    if (write_text(time, sizeof(time), "%s%s", data.measured, data.offset) != ESP_OK)
        goto no_memory;
    /* The invisible third field keeps the two main readings paired on wide clients. */
    if (fields == NULL || !discord_field(fields, "State", message->state, true) ||
        !discord_field(fields, "Simulated moisture", data.moisture, true) ||
        !discord_field(fields, "\xE2\x80\x8B", "\xE2\x80\x8B", true) ||
        !discord_field(fields, "Temperature", data.temperature, true) ||
        !discord_field(fields, "Humidity", data.humidity, true) ||
        !discord_field(fields, "Measured", time, false) ||
        !discord_field(fields, "Reason", "Confirmed moisture state", false))
        goto no_memory;
    cJSON *footer = cJSON_AddObjectToObject(embed, "footer");
    if (footer == NULL ||
        cJSON_AddStringToObject(footer, "text", "Moisture is simulated using a potentiometer.") ==
            NULL)
        goto no_memory;
    *json = root;
    return ESP_OK;
no_memory:
    cJSON_Delete(root);
    return ESP_ERR_NO_MEM;
}

esp_err_t notification_build_telegram(const notification_message_t *message, const char *chat_id,
                                      cJSON **json)
{
    if (json == NULL)
        return ESP_ERR_INVALID_ARG;
    *json = NULL;
    if (chat_id == NULL || chat_id[0] == '\0')
        return ESP_ERR_INVALID_ARG;
    char text[NOTIFICATION_TEXT_CAPACITY];
    esp_err_t result = notification_format_text(message, true, text, sizeof(text));
    if (result != ESP_OK)
        return result;
    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
        return ESP_ERR_NO_MEM;
    if (cJSON_AddStringToObject(root, "chat_id", chat_id) == NULL ||
        cJSON_AddStringToObject(root, "text", text) == NULL ||
        cJSON_AddStringToObject(root, "parse_mode", "HTML") == NULL) {
        cJSON_Delete(root);
        return ESP_ERR_NO_MEM;
    }
    *json = root;
    return ESP_OK;
}

/* Lucide Sprout, embedded for Gmail; see assets/NOTICE.md. */
const char notification_mark_base64[] =
    "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\r\n"
    "AAAAAElFTkSuQmCC\r\n";
