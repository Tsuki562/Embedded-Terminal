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
    test_outbound_action_is_formatted_for_forwarding();
    return 0;
}
