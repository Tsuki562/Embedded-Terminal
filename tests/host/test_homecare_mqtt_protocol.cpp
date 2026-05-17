#include <cassert>
#include <cstring>

#include "mqtt_bridge/HomeCareMqttProtocol.hpp"

static void test_event_payload_is_parsed_from_json()
{
    HomeCareMqttInboundMessage msg = {};
    const char *topic = "homecare/hub/in/event";
    const char *payload = "{\"level\":\"L3\",\"time\":\"22:31\",\"text\":\"fall suspected\",\"mode\":\"fall\"}";

    assert(homecare_mqtt_parse_inbound(topic, static_cast<int>(std::strlen(topic)),
                                       payload, static_cast<int>(std::strlen(payload)), &msg));
    assert(msg.type == HOMECARE_MQTT_INBOUND_EVENT);
    assert(msg.has_mode);
    assert(msg.mode == HOMECARE_MQTT_MODE_FALL);
    assert(std::strcmp(msg.event.level, "L3") == 0);
    assert(std::strcmp(msg.event.time, "22:31") == 0);
    assert(std::strcmp(msg.event.text, "fall suspected") == 0);
}

static void test_mode_command_accepts_plain_payload()
{
    HomeCareMqttInboundMessage msg = {};
    const char *topic = "homecare/hub/cmd/mode";
    const char *payload = "bathroom";

    assert(homecare_mqtt_parse_inbound(topic, static_cast<int>(std::strlen(topic)),
                                       payload, static_cast<int>(std::strlen(payload)), &msg));
    assert(msg.type == HOMECARE_MQTT_INBOUND_COMMAND);
    assert(msg.has_mode);
    assert(msg.mode == HOMECARE_MQTT_MODE_BATHROOM);
}

static void test_smartcar_cmd_topic_accepts_nested_nav_json()
{
    HomeCareMqttInboundMessage msg = {};
    const char *topic = "smartcar/cmd";
    const char *payload = "{\"requestId\":\"nav_001\",\"data\":{\"nav\":\"pause\"}}";

    assert(homecare_mqtt_parse_inbound(topic, static_cast<int>(std::strlen(topic)),
                                       payload, static_cast<int>(std::strlen(payload)), &msg));
    assert(msg.type == HOMECARE_MQTT_INBOUND_EVENT);
    assert(std::strcmp(msg.event.level, "L2") == 0);
    assert(std::strcmp(msg.event.time, "nav_001") == 0);
    assert(std::strcmp(msg.event.text, "Smart car nav pause") == 0);
}

static void test_smartcar_cmd_topic_accepts_plain_stop_payload()
{
    HomeCareMqttInboundMessage msg = {};
    const char *topic = "smartcar/cmd";
    const char *payload = "stop";

    assert(homecare_mqtt_parse_inbound(topic, static_cast<int>(std::strlen(topic)),
                                       payload, static_cast<int>(std::strlen(payload)), &msg));
    assert(msg.type == HOMECARE_MQTT_INBOUND_EVENT);
    assert(std::strcmp(msg.event.level, "L2") == 0);
    assert(std::strcmp(msg.event.time, "MQTT") == 0);
    assert(std::strcmp(msg.event.text, "Smart car stop") == 0);
}

static void test_smartcar_attitude_topic_accepts_real_device_payload()
{
    HomeCareMqttInboundMessage msg = {};
    const char *topic = "smartcar/attitude";
    const char *payload = "{\"r\":1.25,\"p\":-2.50,\"y\":178.80,\"mag\":true,\"ts\":123456789}";

    assert(homecare_mqtt_parse_inbound(topic, static_cast<int>(std::strlen(topic)),
                                       payload, static_cast<int>(std::strlen(payload)), &msg));
    assert(msg.type == HOMECARE_MQTT_INBOUND_EVENT);
    assert(std::strcmp(msg.event.level, "L1") == 0);
    assert(std::strcmp(msg.event.time, "123456789") == 0);
    assert(std::strcmp(msg.event.text, "Smart car attitude r=1.25 p=-2.50 y=178.80 mag=on") == 0);
}

static void test_outbound_action_is_formatted_for_forwarding()
{
    HomeCareMqttOutboundMessage msg = {};

    assert(homecare_mqtt_format_action(HOMECARE_MQTT_ACTION_PRIVACY_TOGGLE, &msg));
    assert(std::strcmp(msg.topic_suffix, "out/action") == 0);
    assert(std::strstr(msg.payload, "\"action\":\"privacy_toggle\"") != nullptr);
}

int main()
{
    test_event_payload_is_parsed_from_json();
    test_mode_command_accepts_plain_payload();
    test_smartcar_cmd_topic_accepts_nested_nav_json();
    test_smartcar_cmd_topic_accepts_plain_stop_payload();
    test_smartcar_attitude_topic_accepts_real_device_payload();
    test_outbound_action_is_formatted_for_forwarding();
    return 0;
}
