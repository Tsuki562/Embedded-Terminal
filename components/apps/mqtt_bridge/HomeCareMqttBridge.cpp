#include "HomeCareMqttBridge.hpp"

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "mqtt_client.h"

#ifndef CONFIG_HOMECARE_MQTT_BROKER_URI
#define CONFIG_HOMECARE_MQTT_BROKER_URI "mqtt://broker.hivemq.com"
#endif

#ifndef CONFIG_HOMECARE_MQTT_CLIENT_ID
#define CONFIG_HOMECARE_MQTT_CLIENT_ID "esp-brookesia-homecare"
#endif

#ifndef CONFIG_HOMECARE_MQTT_BASE_TOPIC
#define CONFIG_HOMECARE_MQTT_BASE_TOPIC "homecare/hub"
#endif

#ifndef CONFIG_HOMECARE_MQTT_QUEUE_SIZE
#define CONFIG_HOMECARE_MQTT_QUEUE_SIZE 8
#endif

static const char *TAG = "homecare_mqtt";
static QueueHandle_t s_inbound_queue = nullptr;
static esp_mqtt_client_handle_t s_client = nullptr;
static bool s_initialized = false;
static bool s_started = false;
static bool s_connected = false;

static void build_topic(char *out, size_t out_size, const char *suffix)
{
    if (out == nullptr || out_size == 0) {
        return;
    }
    snprintf(out, out_size, "%s/%s", CONFIG_HOMECARE_MQTT_BASE_TOPIC, suffix);
}

static esp_err_t publish_outbound(const HomeCareMqttOutboundMessage *msg)
{
    if (msg == nullptr || s_client == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    char topic[128] = {};
    build_topic(topic, sizeof(topic), msg->topic_suffix);
    const int id = esp_mqtt_client_publish(s_client, topic, msg->payload, 0, msg->qos, msg->retain);
    return id >= 0 ? ESP_OK : ESP_FAIL;
}

static void start_client_if_ready(void)
{
    if (s_client == nullptr || s_started) {
        return;
    }

    esp_err_t err = esp_mqtt_client_start(s_client);
    if (err == ESP_OK) {
        s_started = true;
        ESP_LOGI(TAG, "MQTT client started");
    } else {
        ESP_LOGW(TAG, "MQTT client start failed: %s", esp_err_to_name(err));
    }
}

static void stop_client_if_started(void)
{
    if (s_client == nullptr || !s_started) {
        return;
    }

    esp_err_t err = esp_mqtt_client_stop(s_client);
    if (err == ESP_OK) {
        s_started = false;
        s_connected = false;
        ESP_LOGI(TAG, "MQTT client stopped");
    } else {
        ESP_LOGW(TAG, "MQTT client stop failed: %s", esp_err_to_name(err));
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    (void)handler_args;
    (void)base;

    esp_mqtt_event_handle_t event = static_cast<esp_mqtt_event_handle_t>(event_data);
    if (event == nullptr) {
        return;
    }

    switch (static_cast<esp_mqtt_event_id_t>(event_id)) {
    case MQTT_EVENT_CONNECTED: {
        s_connected = true;
        char topic[128] = {};
        build_topic(topic, sizeof(topic), "in/#");
        esp_mqtt_client_subscribe(event->client, topic, 1);
        build_topic(topic, sizeof(topic), "cmd/#");
        esp_mqtt_client_subscribe(event->client, topic, 1);
        build_topic(topic, sizeof(topic), "out/status");
        esp_mqtt_client_publish(event->client, topic, "{\"status\":\"online\"}", 0, 1, 1);
        ESP_LOGI(TAG, "MQTT connected and subscribed");
        break;
    }
    case MQTT_EVENT_DISCONNECTED:
        s_connected = false;
        ESP_LOGI(TAG, "MQTT disconnected");
        break;
    case MQTT_EVENT_DATA: {
        HomeCareMqttInboundMessage message = {};
        if (homecare_mqtt_parse_inbound(event->topic, event->topic_len,
                                        event->data, event->data_len, &message)) {
            if (s_inbound_queue != nullptr &&
                xQueueSend(s_inbound_queue, &message, 0) != pdTRUE) {
                ESP_LOGW(TAG, "Inbound MQTT queue full, dropping message");
            }
        }
        break;
    }
    case MQTT_EVENT_ERROR:
        ESP_LOGW(TAG, "MQTT transport error");
        break;
    default:
        break;
    }
}

static void network_event_handler(void *handler_args, esp_event_base_t event_base,
                                  int32_t event_id, void *event_data)
{
    (void)handler_args;
    (void)event_data;

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        start_client_if_ready();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        stop_client_if_started();
    }
}

esp_err_t homecare_mqtt_bridge_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    s_inbound_queue = xQueueCreate(CONFIG_HOMECARE_MQTT_QUEUE_SIZE, sizeof(HomeCareMqttInboundMessage));
    if (s_inbound_queue == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = CONFIG_HOMECARE_MQTT_BROKER_URI;
    mqtt_cfg.credentials.client_id = CONFIG_HOMECARE_MQTT_CLIENT_ID;
    mqtt_cfg.session.last_will.topic = CONFIG_HOMECARE_MQTT_BASE_TOPIC "/out/status";
    mqtt_cfg.session.last_will.msg = "{\"status\":\"offline\"}";
    mqtt_cfg.session.last_will.qos = 1;
    mqtt_cfg.session.last_will.retain = 1;

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_client == nullptr) {
        return ESP_FAIL;
    }

    ESP_RETURN_ON_ERROR(esp_mqtt_client_register_event(s_client, static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID),
                                                       mqtt_event_handler, nullptr),
                        TAG, "register mqtt event failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                           network_event_handler, nullptr, nullptr),
                        TAG, "register ip event failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                                           network_event_handler, nullptr, nullptr),
                        TAG, "register wifi event failed");

    s_initialized = true;
    ESP_LOGI(TAG, "MQTT bridge initialized, broker=%s", CONFIG_HOMECARE_MQTT_BROKER_URI);
    return ESP_OK;
}

bool homecare_mqtt_bridge_is_connected(void)
{
    return s_connected;
}

bool homecare_mqtt_bridge_receive(HomeCareMqttInboundMessage *out)
{
    if (out == nullptr || s_inbound_queue == nullptr) {
        return false;
    }
    return xQueueReceive(s_inbound_queue, out, 0) == pdTRUE;
}

esp_err_t homecare_mqtt_bridge_publish_action(homecare_mqtt_action_t action)
{
    HomeCareMqttOutboundMessage msg = {};
    if (!homecare_mqtt_format_action(action, &msg)) {
        return ESP_ERR_INVALID_ARG;
    }
    return publish_outbound(&msg);
}

esp_err_t homecare_mqtt_bridge_publish_mode(homecare_mqtt_mode_t mode)
{
    HomeCareMqttOutboundMessage msg = {};
    if (!homecare_mqtt_format_mode(mode, &msg)) {
        return ESP_ERR_INVALID_ARG;
    }
    return publish_outbound(&msg);
}

esp_err_t homecare_mqtt_bridge_publish_event(const HomeCareMqttEvent *event,
                                             homecare_mqtt_mode_t mode)
{
    if (event == nullptr || s_client == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    char topic[128] = {};
    char payload[HOMECARE_MQTT_PAYLOAD_MAX_LEN] = {};
    build_topic(topic, sizeof(topic), "out/event");
    snprintf(payload, sizeof(payload),
             "{\"level\":\"%s\",\"time\":\"%s\",\"text\":\"%s\",\"mode\":\"%s\"}",
             event->level, event->time, event->text, homecare_mqtt_mode_to_text(mode));

    const int id = esp_mqtt_client_publish(s_client, topic, payload, 0, 1, 0);
    return id >= 0 ? ESP_OK : ESP_FAIL;
}
