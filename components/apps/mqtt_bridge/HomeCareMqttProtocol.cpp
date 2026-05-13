#include "HomeCareMqttProtocol.hpp"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static bool topic_has_suffix(const char *topic, int topic_len, const char *suffix)
{
    if (topic == nullptr || suffix == nullptr || topic_len <= 0) {
        return false;
    }

    const size_t suffix_len = strlen(suffix);
    if (suffix_len == 0 || static_cast<size_t>(topic_len) < suffix_len) {
        return false;
    }

    return strncmp(topic + topic_len - suffix_len, suffix, suffix_len) == 0;
}

static size_t trim_bounds(const char *text, size_t len, size_t *start)
{
    size_t begin = 0;
    size_t end = len;

    while (begin < len && isspace(static_cast<unsigned char>(text[begin]))) {
        ++begin;
    }
    while (end > begin && isspace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }

    *start = begin;
    return end - begin;
}

static bool text_equals(const char *text, size_t len, const char *expected)
{
    const size_t expected_len = strlen(expected);
    if (len != expected_len) {
        return false;
    }

    for (size_t i = 0; i < len; ++i) {
        if (tolower(static_cast<unsigned char>(text[i])) !=
            tolower(static_cast<unsigned char>(expected[i]))) {
            return false;
        }
    }
    return true;
}

static void copy_text(char *dest, size_t dest_size, const char *src, size_t src_len)
{
    if (dest == nullptr || dest_size == 0) {
        return;
    }
    if (src == nullptr) {
        dest[0] = '\0';
        return;
    }

    const size_t len = src_len < dest_size - 1 ? src_len : dest_size - 1;
    memcpy(dest, src, len);
    dest[len] = '\0';
}

static bool json_extract_string(const char *payload, int payload_len, const char *key,
                                char *out, size_t out_size)
{
    if (payload == nullptr || key == nullptr || out == nullptr || out_size == 0 || payload_len <= 0) {
        return false;
    }
    out[0] = '\0';

    char pattern[40] = {};
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const size_t pattern_len = strlen(pattern);

    const char *cursor = payload;
    const char *end = payload + payload_len;
    while (cursor + pattern_len < end) {
        const char *match = static_cast<const char *>(memchr(cursor, '"', end - cursor));
        if (match == nullptr || match + pattern_len > end) {
            return false;
        }
        if (strncmp(match, pattern, pattern_len) != 0) {
            cursor = match + 1;
            continue;
        }

        const char *colon = static_cast<const char *>(memchr(match + pattern_len, ':', end - (match + pattern_len)));
        if (colon == nullptr) {
            return false;
        }
        const char *value = colon + 1;
        while (value < end && isspace(static_cast<unsigned char>(*value))) {
            ++value;
        }
        if (value >= end || *value != '"') {
            return false;
        }
        ++value;

        size_t written = 0;
        bool escape = false;
        while (value < end) {
            const char ch = *value++;
            if (escape) {
                if (written + 1 < out_size) {
                    out[written++] = ch;
                }
                escape = false;
                continue;
            }
            if (ch == '\\') {
                escape = true;
                continue;
            }
            if (ch == '"') {
                out[written] = '\0';
                return true;
            }
            if (written + 1 < out_size) {
                out[written++] = ch;
            }
        }
        return false;
    }

    return false;
}

homecare_mqtt_mode_t homecare_mqtt_mode_from_text(const char *text, size_t len)
{
    if (text == nullptr) {
        return HOMECARE_MQTT_MODE_UNKNOWN;
    }

    size_t start = 0;
    const size_t trimmed_len = trim_bounds(text, len, &start);
    const char *trimmed = text + start;

    if (text_equals(trimmed, trimmed_len, "normal") || text_equals(trimmed, trimmed_len, "patrol")) {
        return HOMECARE_MQTT_MODE_NORMAL;
    }
    if (text_equals(trimmed, trimmed_len, "fall")) {
        return HOMECARE_MQTT_MODE_FALL;
    }
    if (text_equals(trimmed, trimmed_len, "bathroom")) {
        return HOMECARE_MQTT_MODE_BATHROOM;
    }
    if (text_equals(trimmed, trimmed_len, "night")) {
        return HOMECARE_MQTT_MODE_NIGHT;
    }

    return HOMECARE_MQTT_MODE_UNKNOWN;
}

const char *homecare_mqtt_mode_to_text(homecare_mqtt_mode_t mode)
{
    switch (mode) {
    case HOMECARE_MQTT_MODE_NORMAL:
        return "normal";
    case HOMECARE_MQTT_MODE_FALL:
        return "fall";
    case HOMECARE_MQTT_MODE_BATHROOM:
        return "bathroom";
    case HOMECARE_MQTT_MODE_NIGHT:
        return "night";
    default:
        return "unknown";
    }
}

bool homecare_mqtt_parse_inbound(const char *topic, int topic_len,
                                 const char *payload, int payload_len,
                                 HomeCareMqttInboundMessage *out)
{
    if (topic == nullptr || payload == nullptr || out == nullptr ||
        topic_len <= 0 || payload_len < 0) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    out->mode = HOMECARE_MQTT_MODE_UNKNOWN;

    if (topic_has_suffix(topic, topic_len, "/event")) {
        out->type = HOMECARE_MQTT_INBOUND_EVENT;

        bool has_any_field = false;
        has_any_field |= json_extract_string(payload, payload_len, "level",
                                             out->event.level, sizeof(out->event.level));
        has_any_field |= json_extract_string(payload, payload_len, "time",
                                             out->event.time, sizeof(out->event.time));
        has_any_field |= json_extract_string(payload, payload_len, "text",
                                             out->event.text, sizeof(out->event.text));
        if (!has_any_field) {
            copy_text(out->event.level, sizeof(out->event.level), "L1", 2);
            copy_text(out->event.text, sizeof(out->event.text), payload, static_cast<size_t>(payload_len));
        }

        char mode_text[HOMECARE_MQTT_FIELD_MAX_LEN] = {};
        if (json_extract_string(payload, payload_len, "mode", mode_text, sizeof(mode_text))) {
            out->mode = homecare_mqtt_mode_from_text(mode_text, strlen(mode_text));
            out->has_mode = (out->mode != HOMECARE_MQTT_MODE_UNKNOWN);
        }
        return true;
    }

    if (topic_has_suffix(topic, topic_len, "/cmd/mode")) {
        out->type = HOMECARE_MQTT_INBOUND_COMMAND;
        char mode_text[HOMECARE_MQTT_FIELD_MAX_LEN] = {};
        if (!json_extract_string(payload, payload_len, "mode", mode_text, sizeof(mode_text))) {
            copy_text(mode_text, sizeof(mode_text), payload, static_cast<size_t>(payload_len));
        }
        out->mode = homecare_mqtt_mode_from_text(mode_text, strlen(mode_text));
        out->has_mode = (out->mode != HOMECARE_MQTT_MODE_UNKNOWN);
        return out->has_mode;
    }

    return false;
}

static const char *action_to_text(homecare_mqtt_action_t action)
{
    switch (action) {
    case HOMECARE_MQTT_ACTION_PATROL:
        return "patrol";
    case HOMECARE_MQTT_ACTION_RECHARGE:
        return "recharge";
    case HOMECARE_MQTT_ACTION_PRIVACY_TOGGLE:
        return "privacy_toggle";
    case HOMECARE_MQTT_ACTION_CALL_FAMILY:
        return "call_family";
    default:
        return nullptr;
    }
}

bool homecare_mqtt_format_action(homecare_mqtt_action_t action,
                                 HomeCareMqttOutboundMessage *out)
{
    if (out == nullptr) {
        return false;
    }

    const char *action_text = action_to_text(action);
    if (action_text == nullptr) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    copy_text(out->topic_suffix, sizeof(out->topic_suffix), "out/action", strlen("out/action"));
    snprintf(out->payload, sizeof(out->payload), "{\"action\":\"%s\"}", action_text);
    out->qos = 1;
    out->retain = 0;
    return true;
}

bool homecare_mqtt_format_mode(homecare_mqtt_mode_t mode,
                               HomeCareMqttOutboundMessage *out)
{
    if (out == nullptr || mode == HOMECARE_MQTT_MODE_UNKNOWN) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    copy_text(out->topic_suffix, sizeof(out->topic_suffix), "out/mode", strlen("out/mode"));
    snprintf(out->payload, sizeof(out->payload), "{\"mode\":\"%s\"}", homecare_mqtt_mode_to_text(mode));
    out->qos = 1;
    out->retain = 1;
    return true;
}
