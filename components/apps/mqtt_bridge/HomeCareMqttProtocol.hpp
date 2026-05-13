#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HOMECARE_MQTT_TOPIC_SUFFIX_MAX_LEN 64
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
    HOMECARE_MQTT_INBOUND_NONE = 0,
    HOMECARE_MQTT_INBOUND_EVENT,
    HOMECARE_MQTT_INBOUND_COMMAND,
} homecare_mqtt_inbound_type_t;

typedef enum {
    HOMECARE_MQTT_ACTION_PATROL = 0,
    HOMECARE_MQTT_ACTION_RECHARGE,
    HOMECARE_MQTT_ACTION_PRIVACY_TOGGLE,
    HOMECARE_MQTT_ACTION_CALL_FAMILY,
} homecare_mqtt_action_t;

typedef struct {
    char level[HOMECARE_MQTT_FIELD_MAX_LEN];
    char time[HOMECARE_MQTT_FIELD_MAX_LEN];
    char text[HOMECARE_MQTT_TEXT_MAX_LEN];
} HomeCareMqttEvent;

typedef struct {
    homecare_mqtt_inbound_type_t type;
    bool has_mode;
    homecare_mqtt_mode_t mode;
    HomeCareMqttEvent event;
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
bool homecare_mqtt_format_mode(homecare_mqtt_mode_t mode,
                               HomeCareMqttOutboundMessage *out);
homecare_mqtt_mode_t homecare_mqtt_mode_from_text(const char *text, size_t len);
const char *homecare_mqtt_mode_to_text(homecare_mqtt_mode_t mode);

#ifdef __cplusplus
}
#endif
