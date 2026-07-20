#include "HomeCareMqttProtocol.hpp"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
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

static const char *json_find_value_start(const char *payload, int payload_len, const char *key)
{
    if (payload == nullptr || key == nullptr || payload_len <= 0) {
        return nullptr;
    }

    char pattern[40] = {};
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const size_t pattern_len = strlen(pattern);

    const char *cursor = payload;
    const char *end = payload + payload_len;
    while (cursor + pattern_len < end) {
        const char *match = static_cast<const char *>(memchr(cursor, '"', end - cursor));
        if (match == nullptr || match + pattern_len > end) {
            return nullptr;
        }
        if (strncmp(match, pattern, pattern_len) != 0) {
            cursor = match + 1;
            continue;
        }

        const char *colon = static_cast<const char *>(memchr(match + pattern_len, ':', end - (match + pattern_len)));
        if (colon == nullptr) {
            return nullptr;
        }

        const char *value = colon + 1;
        while (value < end && isspace(static_cast<unsigned char>(*value))) {
            ++value;
        }
        return value < end ? value : nullptr;
    }

    return nullptr;
}

static bool json_extract_string(const char *payload, int payload_len, const char *key,
                                char *out, size_t out_size)
{
    if (payload == nullptr || key == nullptr || out == nullptr || out_size == 0 || payload_len <= 0) {
        return false;
    }
    out[0] = '\0';
    const char *end = payload + payload_len;
    const char *value = json_find_value_start(payload, payload_len, key);
    if (value == nullptr || *value != '"') {
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

static bool json_extract_bool(const char *payload, int payload_len, const char *key, bool *out)
{
    if (payload == nullptr || key == nullptr || out == nullptr || payload_len <= 0) {
        return false;
    }
    const char *end = payload + payload_len;
    const char *value = json_find_value_start(payload, payload_len, key);
    if (value == nullptr) {
        return false;
    }

    if ((end - value) >= 4 && strncmp(value, "true", 4) == 0) {
        *out = true;
        return true;
    }
    if ((end - value) >= 5 && strncmp(value, "false", 5) == 0) {
        *out = false;
        return true;
    }

    return false;
}

static bool json_extract_float(const char *payload, int payload_len, const char *key, float *out)
{
    if (payload == nullptr || key == nullptr || out == nullptr || payload_len <= 0) {
        return false;
    }

    const char *value = json_find_value_start(payload, payload_len, key);
    if (value == nullptr) {
        return false;
    }
    if (*value == '"') {
        ++value;
    }

    char number[32] = {};
    size_t written = 0;
    const char *end = payload + payload_len;
    while (value < end && written + 1 < sizeof(number)) {
        const char ch = *value;
        if (!(isdigit(static_cast<unsigned char>(ch)) || ch == '-' || ch == '+' || ch == '.')) {
            break;
        }
        number[written++] = ch;
        ++value;
    }
    if (written == 0) {
        return false;
    }

    char *parse_end = nullptr;
    const float parsed = strtof(number, &parse_end);
    if (parse_end == number) {
        return false;
    }

    *out = parsed;
    return true;
}

static bool json_extract_i64(const char *payload, int payload_len, const char *key, long long *out)
{
    if (payload == nullptr || key == nullptr || out == nullptr || payload_len <= 0) {
        return false;
    }

    const char *value = json_find_value_start(payload, payload_len, key);
    if (value == nullptr) {
        return false;
    }
    if (*value == '"') {
        ++value;
    }

    char number[32] = {};
    size_t written = 0;
    const char *end = payload + payload_len;
    while (value < end && written + 1 < sizeof(number)) {
        const char ch = *value;
        if (!(isdigit(static_cast<unsigned char>(ch)) || ch == '-' || ch == '+')) {
            break;
        }
        number[written++] = ch;
        ++value;
    }
    if (written == 0) {
        return false;
    }

    char *parse_end = nullptr;
    const long long parsed = strtoll(number, &parse_end, 10);
    if (parse_end == number) {
        return false;
    }

    *out = parsed;
    return true;
}

static bool json_extract_int_array(const char *payload, int payload_len, const char *key,
                                   int *out, int out_count, int *parsed_count)
{
    if (payload == nullptr || key == nullptr || out == nullptr || parsed_count == nullptr ||
        payload_len <= 0 || out_count <= 0) {
        return false;
    }

    const char *value = json_find_value_start(payload, payload_len, key);
    const char *end = payload + payload_len;
    if (value == nullptr || value >= end || *value != '[') {
        return false;
    }
    ++value;

    int count = 0;
    while (value < end && count < out_count) {
        while (value < end && (isspace(static_cast<unsigned char>(*value)) || *value == ',')) {
            ++value;
        }
        if (value >= end || *value == ']') {
            break;
        }

        char number[16] = {};
        size_t written = 0;
        while (value < end && written + 1 < sizeof(number)) {
            const char ch = *value;
            if (!(isdigit(static_cast<unsigned char>(ch)) || ch == '-' || ch == '+')) {
                break;
            }
            number[written++] = ch;
            ++value;
        }
        if (written == 0) {
            return false;
        }

        char *parse_end = nullptr;
        const long parsed = strtol(number, &parse_end, 10);
        if (parse_end == number) {
            return false;
        }
        out[count++] = static_cast<int>(parsed);

        while (value < end && isspace(static_cast<unsigned char>(*value))) {
            ++value;
        }
        if (value < end && *value == ']') {
            break;
        }
        if (value < end && *value != ',') {
            return false;
        }
    }

    *parsed_count = count;
    return count > 0;
}

static int clamp_percent_value(long long value)
{
    if (value < 0) {
        return 0;
    }
    if (value > 100) {
        return 100;
    }
    return static_cast<int>(value);
}

static bool json_extract_percent_any(const char *payload, int payload_len,
                                     const char *const *keys, size_t key_count, int *out)
{
    if (payload == nullptr || keys == nullptr || out == nullptr || payload_len <= 0) {
        return false;
    }

    for (size_t i = 0; i < key_count; ++i) {
        long long int_value = 0;
        if (json_extract_i64(payload, payload_len, keys[i], &int_value)) {
            *out = clamp_percent_value(int_value);
            return true;
        }

        float float_value = 0.0f;
        if (json_extract_float(payload, payload_len, keys[i], &float_value)) {
            *out = clamp_percent_value(static_cast<long long>(float_value + (float_value >= 0.0f ? 0.5f : -0.5f)));
            return true;
        }
    }

    return false;
}

static const char *smartcar_command_level(const char *field, const char *value)
{
    if (field == nullptr || value == nullptr) {
        return "L1";
    }

    const size_t field_len = strlen(field);
    const size_t value_len = strlen(value);
    if (text_equals(value, value_len, "pause") ||
        text_equals(value, value_len, "stop") ||
        text_equals(value, value_len, "off") ||
        text_equals(value, value_len, "drive:stop")) {
        return "L2";
    }

    if (text_equals(field, field_len, "run") && text_equals(value, value_len, "false")) {
        return "L2";
    }

    return "L1";
}

static bool parse_smartcar_command_payload(const char *payload, int payload_len,
                                           HomeCareMqttInboundMessage *out)
{
    if (payload == nullptr || out == nullptr || payload_len < 0) {
        return false;
    }

    char request_id[HOMECARE_MQTT_FIELD_MAX_LEN] = {};
    char value[HOMECARE_MQTT_TEXT_MAX_LEN] = {};
    const char *field = nullptr;

    json_extract_string(payload, payload_len, "requestId", request_id, sizeof(request_id));

    if (json_extract_string(payload, payload_len, "nav", value, sizeof(value))) {
        field = "nav";
    } else if (json_extract_string(payload, payload_len, "action", value, sizeof(value))) {
        field = "action";
    } else if (json_extract_string(payload, payload_len, "switch", value, sizeof(value))) {
        field = "switch";
    } else if (json_extract_string(payload, payload_len, "drive", value, sizeof(value))) {
        field = "drive";
    } else if (json_extract_string(payload, payload_len, "move", value, sizeof(value))) {
        field = "move";
    } else if (json_extract_string(payload, payload_len, "motion", value, sizeof(value))) {
        field = "motion";
    } else if (json_extract_string(payload, payload_len, "cmd", value, sizeof(value))) {
        field = "cmd";
    } else {
        bool run = false;
        if (json_extract_bool(payload, payload_len, "run", &run)) {
            field = "run";
            copy_text(value, sizeof(value), run ? "true" : "false", run ? 4 : 5);
        } else {
            size_t start = 0;
            const size_t trimmed_len = trim_bounds(payload, static_cast<size_t>(payload_len), &start);
            if (trimmed_len == 0) {
                return false;
            }
            field = "raw";
            copy_text(value, sizeof(value), payload + start, trimmed_len);
        }
    }

    out->type = HOMECARE_MQTT_INBOUND_EVENT;
    const char *level = smartcar_command_level(field, value);
    copy_text(out->event.level, sizeof(out->event.level), level, strlen(level));
    if (request_id[0] != '\0') {
        copy_text(out->event.time, sizeof(out->event.time), request_id, strlen(request_id));
    } else {
        copy_text(out->event.time, sizeof(out->event.time), "MQTT", 4);
    }

    if (text_equals(field, strlen(field), "raw")) {
        snprintf(out->event.text, sizeof(out->event.text), "Smart car %s", value);
    } else {
        snprintf(out->event.text, sizeof(out->event.text), "Smart car %s %s", field, value);
    }
    return true;
}

static bool parse_smartcar_attitude_payload(const char *payload, int payload_len,
                                            HomeCareMqttInboundMessage *out)
{
    if (payload == nullptr || out == nullptr || payload_len < 0) {
        return false;
    }

    float roll = 0.0f;
    float pitch = 0.0f;
    float yaw = 0.0f;
    bool has_mag = false;
    bool mag = false;
    long long ts_ms = 0;
    bool has_ts = false;
    int battery_percent = 0;
    int route_progress = 0;
    const char *const battery_keys[] = {"battery_percent", "battery", "bat", "b"};
    const char *const route_keys[] = {"route_progress", "route_percent", "route", "progress"};

    if (!json_extract_float(payload, payload_len, "r", &roll) ||
        !json_extract_float(payload, payload_len, "p", &pitch) ||
        !json_extract_float(payload, payload_len, "y", &yaw)) {
        return false;
    }
    has_mag = json_extract_bool(payload, payload_len, "mag", &mag);
    has_ts = json_extract_i64(payload, payload_len, "ts", &ts_ms);
    const bool has_battery = json_extract_percent_any(payload, payload_len, battery_keys,
                                                      sizeof(battery_keys) / sizeof(battery_keys[0]),
                                                      &battery_percent);
    const bool has_route = json_extract_percent_any(payload, payload_len, route_keys,
                                                    sizeof(route_keys) / sizeof(route_keys[0]),
                                                    &route_progress);

    out->type = HOMECARE_MQTT_INBOUND_EVENT;
    out->has_smartcar_attitude = true;
    out->smartcar_attitude.valid = true;
    out->smartcar_attitude.roll_deg = roll;
    out->smartcar_attitude.pitch_deg = pitch;
    out->smartcar_attitude.yaw_deg = yaw;
    out->smartcar_attitude.has_mag = has_mag ? mag : false;
    out->smartcar_attitude.timestamp_ms = has_ts ? ts_ms : 0;
    out->smartcar_attitude.has_battery_percent = has_battery;
    out->smartcar_attitude.battery_percent = has_battery ? battery_percent : 0;
    out->smartcar_attitude.has_route_progress = has_route;
    out->smartcar_attitude.route_progress = has_route ? route_progress : 0;

    copy_text(out->event.level, sizeof(out->event.level), "L1", 2);
    if (has_ts) {
        snprintf(out->event.time, sizeof(out->event.time), "%lld", ts_ms);
    } else {
        copy_text(out->event.time, sizeof(out->event.time), "MQTT", 4);
    }
    snprintf(out->event.text, sizeof(out->event.text),
             "Smart car attitude r=%.2f p=%.2f y=%.2f mag=%s",
             roll, pitch, yaw, (has_mag && mag) ? "on" : "off");
    return true;
}

static bool parse_generic_event_payload(const char *payload, int payload_len,
                                        HomeCareMqttInboundMessage *out)
{
    if (payload == nullptr || out == nullptr || payload_len < 0) {
        return false;
    }

    char text[HOMECARE_MQTT_TEXT_MAX_LEN] = {};
    char time[HOMECARE_MQTT_FIELD_MAX_LEN] = {};
    char level[HOMECARE_MQTT_FIELD_MAX_LEN] = {};
    long long ts_ms = 0;

    bool has_text = false;
    has_text |= json_extract_string(payload, payload_len, "text", text, sizeof(text));
    if (!has_text) {
        has_text |= json_extract_string(payload, payload_len, "message", text, sizeof(text));
    }
    if (!has_text) {
        has_text |= json_extract_string(payload, payload_len, "status", text, sizeof(text));
    }
    if (!has_text) {
        has_text |= json_extract_string(payload, payload_len, "state", text, sizeof(text));
    }
    if (!has_text) {
        size_t start = 0;
        const size_t trimmed_len = trim_bounds(payload, static_cast<size_t>(payload_len), &start);
        if (trimmed_len == 0) {
            return false;
        }
        copy_text(text, sizeof(text), payload + start, trimmed_len);
        has_text = true;
    }

    out->type = HOMECARE_MQTT_INBOUND_EVENT;
    if (!json_extract_string(payload, payload_len, "level", level, sizeof(level))) {
        copy_text(level, sizeof(level), "L1", 2);
    }
    copy_text(out->event.level, sizeof(out->event.level), level, strlen(level));

    if (json_extract_string(payload, payload_len, "time", time, sizeof(time))) {
        copy_text(out->event.time, sizeof(out->event.time), time, strlen(time));
    } else if (json_extract_i64(payload, payload_len, "ts", &ts_ms)) {
        snprintf(out->event.time, sizeof(out->event.time), "%lld", ts_ms);
    } else {
        copy_text(out->event.time, sizeof(out->event.time), "MQTT", 4);
    }

    if (has_text) {
        copy_text(out->event.text, sizeof(out->event.text), text, strlen(text));
    }
    return true;
}

static bool parse_smartcar_system_status_payload(const char *payload, int payload_len,
                                                 HomeCareMqttInboundMessage *out)
{
    char state[HOMECARE_MQTT_FIELD_MAX_LEN] = {};
    if (payload == nullptr || out == nullptr || payload_len < 0 ||
        !json_extract_string(payload, payload_len, "state", state, sizeof(state))) {
        return false;
    }

    out->type = HOMECARE_MQTT_INBOUND_COMMAND;
    if (text_equals(state, strlen(state), "cruise") ||
        text_equals(state, strlen(state), "return_running")) {
        out->mode = HOMECARE_MQTT_MODE_NORMAL;
        out->system_status = text_equals(state, strlen(state), "cruise")
                                 ? HOMECARE_MQTT_SYSTEM_STATUS_CRUISE
                                 : HOMECARE_MQTT_SYSTEM_STATUS_RETURN_RUNNING;
    } else if (text_equals(state, strlen(state), "abnormal_running") ||
               text_equals(state, strlen(state), "abnormal_ready")) {
        out->mode = HOMECARE_MQTT_MODE_FALL;
        out->system_status = text_equals(state, strlen(state), "abnormal_ready")
                                 ? HOMECARE_MQTT_SYSTEM_STATUS_ABNORMAL_READY
                                 : HOMECARE_MQTT_SYSTEM_STATUS_ABNORMAL_RUNNING;
    } else {
        return false;
    }
    out->has_mode = true;
    out->has_system_status = true;
    return true;
}

static bool parse_csi_summary_payload(const char *payload, int payload_len,
                                      HomeCareMqttInboundMessage *out)
{
    if (payload == nullptr || out == nullptr || payload_len < 0) {
        return false;
    }

    HomeCareMqttCsiSummary summary = {};
    long long value = 0;
    if (!json_extract_string(payload, payload_len, "device_id",
                             summary.device_id, sizeof(summary.device_id))) {
        copy_text(summary.device_id, sizeof(summary.device_id), "unknown", strlen("unknown"));
    }
    if (json_extract_i64(payload, payload_len, "seq", &value)) {
        summary.seq = value;
    }
    if (json_extract_i64(payload, payload_len, "timestamp", &value) ||
        json_extract_i64(payload, payload_len, "ts", &value)) {
        summary.timestamp_ms = value;
    }
    if (json_extract_i64(payload, payload_len, "rssi", &value)) {
        summary.rssi = static_cast<int>(value);
    }
    if (json_extract_i64(payload, payload_len, "channel", &value)) {
        summary.channel = static_cast<int>(value);
    }
    if (json_extract_i64(payload, payload_len, "len", &value)) {
        summary.len = static_cast<int>(value);
    }
    json_extract_float(payload, payload_len, "energy", &summary.energy);

    int amp_count = 0;
    if (!json_extract_int_array(payload, payload_len, "amp", summary.amp,
                                HOMECARE_MQTT_CSI_AMP_MAX_COUNT, &amp_count)) {
        return false;
    }
    summary.amp_count = amp_count;
    if (summary.len <= 0) {
        summary.len = amp_count;
    }
    summary.valid = true;

    out->type = HOMECARE_MQTT_INBOUND_EVENT;
    out->has_csi_summary = true;
    out->csi_summary = summary;
    copy_text(out->event.level, sizeof(out->event.level), "L1", 2);
    if (summary.timestamp_ms > 0) {
        snprintf(out->event.time, sizeof(out->event.time), "%lld", summary.timestamp_ms);
    } else {
        copy_text(out->event.time, sizeof(out->event.time), "MQTT", 4);
    }
    snprintf(out->event.text, sizeof(out->event.text),
             "CSI summary %s energy %.2f len %d",
             summary.device_id, summary.energy, summary.amp_count);
    return true;
}

static bool parse_radar_result_payload(const char *payload, int payload_len,
                                       HomeCareMqttInboundMessage *out)
{
    if (payload == nullptr || out == nullptr || payload_len < 0) {
        return false;
    }

    HomeCareMqttRadarResult result = {};
    long long int_value = 0;
    if (!json_extract_string(payload, payload_len, "device_id",
                             result.device_id, sizeof(result.device_id))) {
        copy_text(result.device_id, sizeof(result.device_id), "unknown", strlen("unknown"));
    }
    if (json_extract_i64(payload, payload_len, "seq", &int_value)) {
        result.seq = int_value;
    }
    if (json_extract_i64(payload, payload_len, "timestamp", &int_value) ||
        json_extract_i64(payload, payload_len, "ts", &int_value)) {
        result.timestamp_ms = int_value;
    }
    json_extract_float(payload, payload_len, "waveform_wander", &result.waveform_wander);
    json_extract_float(payload, payload_len, "waveform_jitter", &result.waveform_jitter);
    json_extract_float(payload, payload_len, "waveform_wander_threshold",
                       &result.waveform_wander_threshold);
    json_extract_float(payload, payload_len, "waveform_jitter_threshold",
                       &result.waveform_jitter_threshold);
    if (json_extract_i64(payload, payload_len, "someone_status", &int_value)) {
        result.someone_status = static_cast<int>(int_value);
    }
    if (json_extract_i64(payload, payload_len, "move_status", &int_value)) {
        result.move_status = static_cast<int>(int_value);
    }
    result.valid = true;

    out->type = HOMECARE_MQTT_INBOUND_EVENT;
    out->has_radar_result = true;
    out->radar_result = result;
    copy_text(out->event.level, sizeof(out->event.level),
              (result.someone_status || result.move_status) ? "L1" : "L0",
              (result.someone_status || result.move_status) ? 2 : 2);
    if (result.timestamp_ms > 0) {
        snprintf(out->event.time, sizeof(out->event.time), "%lld", result.timestamp_ms);
    } else {
        copy_text(out->event.time, sizeof(out->event.time), "MQTT", 4);
    }
    /* 用整数表示（×1e6），避免 %.3f 触发 _dtoa_r 的大栈消耗 */
    snprintf(out->event.text, sizeof(out->event.text),
             "Radar %s s=%d m=%d w=%de-6 j=%de-6",
             result.device_id, result.someone_status, result.move_status,
             static_cast<int>(result.waveform_wander * 1e6f),
             static_cast<int>(result.waveform_jitter * 1e6f));
    return true;
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
    copy_text(out->topic, sizeof(out->topic), topic, static_cast<size_t>(topic_len));

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

    if (topic_has_suffix(topic, topic_len, "/system/status")) {
        return parse_smartcar_system_status_payload(payload, payload_len, out);
    }

    if (topic_has_suffix(topic, topic_len, "/csi/summary")) {
        return parse_csi_summary_payload(payload, payload_len, out);
    }

    if (topic_has_suffix(topic, topic_len, "/radar/result")) {
        return parse_radar_result_payload(payload, payload_len, out);
    }

    if (topic_has_suffix(topic, topic_len, "/attitude") || strcmp(out->topic, "smartcar/attitude") == 0) {
        return parse_smartcar_attitude_payload(payload, payload_len, out);
    }

    if (topic_has_suffix(topic, topic_len, "/cmd")) {
        return parse_smartcar_command_payload(payload, payload_len, out);
    }

    return parse_generic_event_payload(payload, payload_len, out);
}

static const char *action_to_text(homecare_mqtt_action_t action)
{
    switch (action) {
    case HOMECARE_MQTT_ACTION_PATROL:
        return "patrol";
    case HOMECARE_MQTT_ACTION_RECHARGE:
        return "recharge";
    case HOMECARE_MQTT_ACTION_STOP:
        return "stop";
    case HOMECARE_MQTT_ACTION_PRIVACY_TOGGLE:
        return "privacy_toggle";
    case HOMECARE_MQTT_ACTION_CALL_FAMILY:
        return "call_family";
    default:
        return nullptr;
    }
}

bool homecare_mqtt_format_action_with_request_id(homecare_mqtt_action_t action,
                                                 const char *request_id,
                                                 HomeCareMqttOutboundMessage *out)
{
    if (request_id == nullptr || request_id[0] == '\0' || out == nullptr) {
        return false;
    }

    const char *state = nullptr;
    const char *place = "";
    bool nav_stop = false;
    switch (action) {
    case HOMECARE_MQTT_ACTION_PATROL: state = "cruise"; break;
    case HOMECARE_MQTT_ACTION_RECHARGE: state = "return_home"; break;
    case HOMECARE_MQTT_ACTION_STOP: nav_stop = true; break;
    case HOMECARE_MQTT_ACTION_ABNORMAL_BATHROOM: state = "abnormal"; place = "bathroom"; break;
    case HOMECARE_MQTT_ACTION_ABNORMAL_BEDROOM: state = "abnormal"; place = "bedroom"; break;
    case HOMECARE_MQTT_ACTION_ABNORMAL_KITCHEN: state = "abnormal"; place = "kitchen"; break;
    default: return false;
    }

    memset(out, 0, sizeof(*out));
    copy_text(out->topic_suffix, sizeof(out->topic_suffix), "smartcar/cmd", strlen("smartcar/cmd"));
    if (nav_stop) {
        snprintf(out->payload, sizeof(out->payload),
                 "{\"requestId\":\"%s\",\"data\":{\"action\":\"stop\"}}", request_id);
    } else {
        snprintf(out->payload, sizeof(out->payload),
                 "{\"cmd\":\"system\",\"state\":\"%s\",\"place\":\"%s\",\"requestId\":\"%s\"}",
                 state, place, request_id);
    }
    out->qos = 1;
    out->retain = 0;
    return true;
}

bool homecare_mqtt_format_action(homecare_mqtt_action_t action,
                                 HomeCareMqttOutboundMessage *out)
{
    if (out == nullptr) {
        return false;
    }

    if (action <= HOMECARE_MQTT_ACTION_ABNORMAL_KITCHEN) {
        return homecare_mqtt_format_action_with_request_id(action, "terminal", out);
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
