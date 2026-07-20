#include <cassert>
#include <cstdio>
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

static void test_smartcar_system_status_maps_to_terminal_modes()
{
    struct TestCase {
        const char *state;
        homecare_mqtt_mode_t expected_mode;
    };
    const TestCase cases[] = {
        {"cruise", HOMECARE_MQTT_MODE_NORMAL},
        {"abnormal_running", HOMECARE_MQTT_MODE_FALL},
        {"abnormal_ready", HOMECARE_MQTT_MODE_FALL},
        {"return_running", HOMECARE_MQTT_MODE_NORMAL},
    };

    const char *topic = "smartcar/system/status";
    for (const TestCase &test_case : cases) {
        char payload[96] = {};
        std::snprintf(payload, sizeof(payload), "{\"state\":\"%s\",\"place\":\"bathroom\"}",
                      test_case.state);
        HomeCareMqttInboundMessage msg = {};
        assert(homecare_mqtt_parse_inbound(topic, static_cast<int>(std::strlen(topic)),
                                           payload, static_cast<int>(std::strlen(payload)), &msg));
        assert(msg.type == HOMECARE_MQTT_INBOUND_COMMAND);
        assert(msg.has_mode);
        assert(msg.mode == test_case.expected_mode);
        assert(msg.has_system_status);
    }
}

static void test_csi_summary_topic_accepts_console_payload()
{
    HomeCareMqttInboundMessage msg = {};
    const char *topic = "devices/esp32-aabbcc/csi/summary";
    const char *payload =
        "{\"type\":\"CSI_SUMMARY\",\"device_id\":\"esp32-aabbcc\","
        "\"seq\":\"100\",\"timestamp\":\"124000\",\"rssi\":\"-45\","
        "\"channel\":\"6\",\"len\":\"4\",\"amp\":[12,15,18,20],"
        "\"energy\":\"16.54\"}";

    assert(homecare_mqtt_parse_inbound(topic, static_cast<int>(std::strlen(topic)),
                                       payload, static_cast<int>(std::strlen(payload)), &msg));
    assert(msg.type == HOMECARE_MQTT_INBOUND_EVENT);
    assert(msg.has_csi_summary);
    assert(std::strcmp(msg.csi_summary.device_id, "esp32-aabbcc") == 0);
    assert(msg.csi_summary.seq == 100);
    assert(msg.csi_summary.timestamp_ms == 124000);
    assert(msg.csi_summary.rssi == -45);
    assert(msg.csi_summary.channel == 6);
    assert(msg.csi_summary.len == 4);
    assert(msg.csi_summary.amp_count == 4);
    assert(msg.csi_summary.amp[0] == 12);
    assert(msg.csi_summary.amp[3] == 20);
    assert(msg.csi_summary.energy > 16.53f && msg.csi_summary.energy < 16.55f);
    assert(std::strcmp(msg.event.level, "L1") == 0);
    assert(std::strstr(msg.event.text, "CSI summary") != nullptr);
}

static void test_radar_result_topic_accepts_console_payload()
{
    HomeCareMqttInboundMessage msg = {};
    const char *topic = "devices/esp32-aabbcc/radar/result";
    const char *payload =
        "{\"type\":\"RADAR_DADA\",\"canonical_type\":\"RADAR_DATA\","
        "\"device_id\":\"esp32-aabbcc\",\"seq\":\"42\",\"timestamp\":\"123456\","
        "\"waveform_wander\":\"0.123456\",\"waveform_jitter\":\"0.010000\","
        "\"waveform_wander_threshold\":\"0.500000\","
        "\"waveform_jitter_threshold\":\"0.020000\","
        "\"someone_status\":\"1\",\"move_status\":\"0\"}";

    assert(homecare_mqtt_parse_inbound(topic, static_cast<int>(std::strlen(topic)),
                                       payload, static_cast<int>(std::strlen(payload)), &msg));
    assert(msg.type == HOMECARE_MQTT_INBOUND_EVENT);
    assert(msg.has_radar_result);
    assert(std::strcmp(msg.radar_result.device_id, "esp32-aabbcc") == 0);
    assert(msg.radar_result.seq == 42);
    assert(msg.radar_result.timestamp_ms == 123456);
    assert(msg.radar_result.someone_status == 1);
    assert(msg.radar_result.move_status == 0);
    assert(msg.radar_result.waveform_wander > 0.123f && msg.radar_result.waveform_wander < 0.124f);
    assert(msg.radar_result.waveform_jitter > 0.009f && msg.radar_result.waveform_jitter < 0.011f);
    assert(std::strcmp(msg.event.level, "L1") == 0);
    assert(std::strstr(msg.event.text, "Radar") != nullptr);
}

static void test_outbound_action_is_formatted_for_forwarding()
{
    HomeCareMqttOutboundMessage msg = {};

    assert(homecare_mqtt_format_action(HOMECARE_MQTT_ACTION_PRIVACY_TOGGLE, &msg));
    assert(std::strcmp(msg.topic_suffix, "out/action") == 0);
    assert(std::strstr(msg.payload, "\"action\":\"privacy_toggle\"") != nullptr);

    assert(homecare_mqtt_format_action_with_request_id(HOMECARE_MQTT_ACTION_PATROL, "sys_100", &msg));
    assert(std::strcmp(msg.topic_suffix, "smartcar/cmd") == 0);
    assert(std::strcmp(msg.payload,
                       "{\"cmd\":\"system\",\"state\":\"cruise\",\"place\":\"\",\"requestId\":\"sys_100\"}") == 0);

    assert(homecare_mqtt_format_action_with_request_id(HOMECARE_MQTT_ACTION_RECHARGE, "sys_101", &msg));
    assert(std::strcmp(msg.payload,
                       "{\"cmd\":\"system\",\"state\":\"return_home\",\"place\":\"\",\"requestId\":\"sys_101\"}") == 0);

    assert(homecare_mqtt_format_action_with_request_id(HOMECARE_MQTT_ACTION_STOP, "nav_102", &msg));
    assert(std::strcmp(msg.payload,
                       "{\"requestId\":\"nav_102\",\"data\":{\"action\":\"stop\"}}") == 0);

    assert(homecare_mqtt_format_action_with_request_id(HOMECARE_MQTT_ACTION_ABNORMAL_BATHROOM, "sys_103", &msg));
    assert(std::strcmp(msg.payload,
                       "{\"cmd\":\"system\",\"state\":\"abnormal\",\"place\":\"bathroom\",\"requestId\":\"sys_103\"}") == 0);
}

int main()
{
    test_event_payload_is_parsed_from_json();
    test_mode_command_accepts_plain_payload();
    test_smartcar_cmd_topic_accepts_nested_nav_json();
    test_smartcar_cmd_topic_accepts_plain_stop_payload();
    test_smartcar_attitude_topic_accepts_real_device_payload();
    test_smartcar_system_status_maps_to_terminal_modes();
    test_csi_summary_topic_accepts_console_payload();
    test_radar_result_topic_accepts_console_payload();
    test_outbound_action_is_formatted_for_forwarding();
    return 0;
}
