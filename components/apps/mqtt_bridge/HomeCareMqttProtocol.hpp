#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HOMECARE_MQTT_TOPIC_SUFFIX_MAX_LEN 64
#define HOMECARE_MQTT_TOPIC_MAX_LEN 96
#define HOMECARE_MQTT_PAYLOAD_MAX_LEN 256
#define HOMECARE_MQTT_TEXT_MAX_LEN 96
#define HOMECARE_MQTT_FIELD_MAX_LEN 24

typedef enum {
    HOMECARE_MQTT_MODE_NORMAL = 0,
    HOMECARE_MQTT_MODE_FALL,
    HOMECARE_MQTT_MODE_BATHROOM,
    HOMECARE_MQTT_MODE_NIGHT,
    HOMECARE_MQTT_MODE_UNKNOWN,
} homecare_mqtt_mode_t;

typedef enum {
    HOMECARE_MQTT_SYSTEM_STATUS_UNKNOWN = 0,
    HOMECARE_MQTT_SYSTEM_STATUS_CRUISE,
    HOMECARE_MQTT_SYSTEM_STATUS_ABNORMAL_RUNNING,
    HOMECARE_MQTT_SYSTEM_STATUS_ABNORMAL_READY,
    HOMECARE_MQTT_SYSTEM_STATUS_RETURN_RUNNING,
} homecare_mqtt_system_status_t;

typedef enum {
    HOMECARE_MQTT_INBOUND_NONE = 0,
    HOMECARE_MQTT_INBOUND_EVENT,
    HOMECARE_MQTT_INBOUND_COMMAND,
} homecare_mqtt_inbound_type_t;

typedef enum {
    HOMECARE_MQTT_ACTION_PATROL = 0,
    HOMECARE_MQTT_ACTION_RECHARGE,
    HOMECARE_MQTT_ACTION_STOP,
    HOMECARE_MQTT_ACTION_ABNORMAL_BATHROOM,
    HOMECARE_MQTT_ACTION_ABNORMAL_BEDROOM,
    HOMECARE_MQTT_ACTION_ABNORMAL_KITCHEN,
    HOMECARE_MQTT_ACTION_PRIVACY_TOGGLE,
    HOMECARE_MQTT_ACTION_CALL_FAMILY,
} homecare_mqtt_action_t;

typedef struct {
    char level[HOMECARE_MQTT_FIELD_MAX_LEN];
    char time[HOMECARE_MQTT_FIELD_MAX_LEN];
    char text[HOMECARE_MQTT_TEXT_MAX_LEN];
} HomeCareMqttEvent;

typedef struct {
    bool valid;
    float roll_deg;
    float pitch_deg;
    float yaw_deg;
    bool has_mag;
    long long timestamp_ms;
    bool has_battery_percent;
    int battery_percent;
    bool has_route_progress;
    int route_progress;
} HomeCareMqttSmartCarAttitude;

typedef struct {
    homecare_mqtt_inbound_type_t type;
    char topic[HOMECARE_MQTT_TOPIC_MAX_LEN];
    bool has_mode;
    homecare_mqtt_mode_t mode;
    bool has_system_status;
    homecare_mqtt_system_status_t system_status;
    HomeCareMqttEvent event;
    bool has_smartcar_attitude;
    HomeCareMqttSmartCarAttitude smartcar_attitude;
} HomeCareMqttInboundMessage;

typedef struct {
    char topic_suffix[HOMECARE_MQTT_TOPIC_SUFFIX_MAX_LEN];
    char payload[HOMECARE_MQTT_PAYLOAD_MAX_LEN];
    int qos;
    int retain;
} HomeCareMqttOutboundMessage;

bool homecare_mqtt_parse_inbound(const char *topic, int topic_len,
                                 const char *payload, int payload_len,
                                 HomeCareMqttInboundMessage *out);
bool homecare_mqtt_format_action(homecare_mqtt_action_t action,
                                 HomeCareMqttOutboundMessage *out);
bool homecare_mqtt_format_action_with_request_id(homecare_mqtt_action_t action,
                                                 const char *request_id,
                                                 HomeCareMqttOutboundMessage *out);
bool homecare_mqtt_format_mode(homecare_mqtt_mode_t mode,
                               HomeCareMqttOutboundMessage *out);
homecare_mqtt_mode_t homecare_mqtt_mode_from_text(const char *text, size_t len);
const char *homecare_mqtt_mode_to_text(homecare_mqtt_mode_t mode);

#ifdef __cplusplus
}
#endif
