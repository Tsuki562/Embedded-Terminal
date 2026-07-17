#include "HomeCareMqttBridge.hpp"

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_check.h"
#include "esp_crt_bundle.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "mqtt_client.h"
#include "camera_viewer/CameraMqttReceiver.hpp"

#ifndef CONFIG_HOMECARE_MQTT_BROKER_URI
#define CONFIG_HOMECARE_MQTT_BROKER_URI ""
#endif

#ifndef CONFIG_HOMECARE_MQTT_CLIENT_ID
#define CONFIG_HOMECARE_MQTT_CLIENT_ID "homecare-hub"
#endif

#ifndef CONFIG_HOMECARE_MQTT_USERNAME
#define CONFIG_HOMECARE_MQTT_USERNAME ""
#endif

#ifndef CONFIG_HOMECARE_MQTT_PASSWORD
#define CONFIG_HOMECARE_MQTT_PASSWORD ""
#endif

#ifndef CONFIG_HOMECARE_MQTT_INBOUND_CMD_TOPIC
#define CONFIG_HOMECARE_MQTT_INBOUND_CMD_TOPIC "smartcar/cmd"
#endif

#ifndef CONFIG_HOMECARE_MQTT_INBOUND_SMARTCAR_DATA_TOPIC
#define CONFIG_HOMECARE_MQTT_INBOUND_SMARTCAR_DATA_TOPIC "smartcar/attitude"
#endif

#ifndef CONFIG_HOMECARE_MQTT_INBOUND_SYSTEM_STATUS_TOPIC
#define CONFIG_HOMECARE_MQTT_INBOUND_SYSTEM_STATUS_TOPIC "smartcar/system/status"
#endif

#ifndef CONFIG_HOMECARE_MQTT_INBOUND_AUX_TOPIC
#define CONFIG_HOMECARE_MQTT_INBOUND_AUX_TOPIC ""
#endif

#ifndef CONFIG_HOMECARE_MQTT_BASE_TOPIC
#define CONFIG_HOMECARE_MQTT_BASE_TOPIC "homecare/hub"
#endif

#ifndef CONFIG_HOMECARE_MQTT_QUEUE_SIZE
#define CONFIG_HOMECARE_MQTT_QUEUE_SIZE 8
#endif

#ifndef CONFIG_CAMERA_MQTT_ENABLE
#define CONFIG_CAMERA_MQTT_ENABLE 0
#endif

#ifndef CONFIG_CAMERA_MQTT_BROKER_URI
#define CONFIG_CAMERA_MQTT_BROKER_URI ""
#endif

#ifndef CONFIG_CAMERA_MQTT_JPEG_TOPIC
#define CONFIG_CAMERA_MQTT_JPEG_TOPIC "cam/jpeg"
#endif

#ifndef CONFIG_CAMERA_MQTT_STATUS_TOPIC
#define CONFIG_CAMERA_MQTT_STATUS_TOPIC ""
#endif

static const char *TAG = "homecare_mqtt";
static QueueHandle_t s_inbound_queue = nullptr;
static esp_mqtt_client_handle_t s_client = nullptr;
static bool s_initialized = false;
static bool s_started = false;
static bool s_connected = false;

static void log_internal_dma_heap(const char *label)
{
    ESP_LOGI(TAG, "%s: dma_free=%u dma_largest=%u internal_free=%u internal_largest=%u",
             label,
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT)),
             static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT)),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
             static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
}

static void build_topic(char *out, size_t out_size, const char *suffix)
{
    if (out == nullptr || out_size == 0) {
        return;
    }
    snprintf(out, out_size, "%s/%s", CONFIG_HOMECARE_MQTT_BASE_TOPIC, suffix);
}

static bool broker_uri_is_valid(const char *uri)
{
    if (uri == nullptr || uri[0] == '\0') {
        return false;
    }

    const char *scheme_end = strstr(uri, "://");
    if (scheme_end == nullptr || scheme_end == uri) {
        return false;
    }

    const char *host = scheme_end + 3;
    if (host[0] == '\0' || host[0] == ':' || host[0] == '/') {
        return false;
    }

    return true;
}

static bool camera_uses_shared_client(void)
{
    return CONFIG_CAMERA_MQTT_ENABLE &&
           broker_uri_is_valid(CONFIG_HOMECARE_MQTT_BROKER_URI) &&
           broker_uri_is_valid(CONFIG_CAMERA_MQTT_BROKER_URI) &&
           strcmp(CONFIG_HOMECARE_MQTT_BROKER_URI, CONFIG_CAMERA_MQTT_BROKER_URI) == 0;
}

static void build_camera_status_topic(char *out, size_t out_size)
{
    if (out == nullptr || out_size == 0) {
        return;
    }
    if (CONFIG_CAMERA_MQTT_STATUS_TOPIC[0] != '\0') {
        snprintf(out, out_size, "%s", CONFIG_CAMERA_MQTT_STATUS_TOPIC);
    } else {
        snprintf(out, out_size, "%s/status", CONFIG_CAMERA_MQTT_JPEG_TOPIC);
    }
}

static bool station_has_ip(void)
{
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == nullptr) {
        return false;
    }

    esp_netif_ip_info_t ip_info = {};
    if (esp_netif_get_ip_info(netif, &ip_info) != ESP_OK) {
        return false;
    }
    return ip_info.ip.addr != 0;
}

static esp_err_t publish_outbound(const HomeCareMqttOutboundMessage *msg)
{
    if (msg == nullptr || s_client == nullptr || !s_connected) {
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

    log_internal_dma_heap("before mqtt start");
    esp_err_t err = esp_mqtt_client_start(s_client);
    if (err == ESP_OK) {
        s_started = true;
        ESP_LOGI(TAG, "MQTT client started");
        log_internal_dma_heap("after mqtt start");
    } else {
        ESP_LOGW(TAG, "MQTT client start failed: %s", esp_err_to_name(err));
    }
}

static esp_err_t publish_absolute(const HomeCareMqttOutboundMessage *msg)
{
    if (msg == nullptr || s_client == nullptr || !s_connected) {
        return ESP_ERR_INVALID_STATE;
    }
    const int id = esp_mqtt_client_publish(s_client, msg->topic_suffix, msg->payload,
                                           0, msg->qos, msg->retain);
    return id >= 0 ? ESP_OK : ESP_FAIL;
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

static void subscribe_topic_if_set(esp_mqtt_client_handle_t client, const char *topic_filter, int qos = 1)
{
    if (client == nullptr || topic_filter == nullptr || topic_filter[0] == '\0') {
        return;
    }

    const int mid = esp_mqtt_client_subscribe(client, topic_filter, qos);
    if (mid < 0) {
        ESP_LOGW(TAG, "subscribe %s failed", topic_filter);
    } else {
        ESP_LOGI(TAG, "subscribed: %s", topic_filter);
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
        subscribe_topic_if_set(event->client, topic);
        build_topic(topic, sizeof(topic), "cmd/#");
        subscribe_topic_if_set(event->client, topic);
        subscribe_topic_if_set(event->client, CONFIG_HOMECARE_MQTT_INBOUND_CMD_TOPIC);
        subscribe_topic_if_set(event->client, CONFIG_HOMECARE_MQTT_INBOUND_SMARTCAR_DATA_TOPIC);
        subscribe_topic_if_set(event->client, CONFIG_HOMECARE_MQTT_INBOUND_SYSTEM_STATUS_TOPIC);
        subscribe_topic_if_set(event->client, CONFIG_HOMECARE_MQTT_INBOUND_AUX_TOPIC);
        if (camera_uses_shared_client()) {
            char camera_status_topic[96] = {};
            build_camera_status_topic(camera_status_topic, sizeof(camera_status_topic));
            subscribe_topic_if_set(event->client, CONFIG_CAMERA_MQTT_JPEG_TOPIC, 0);
            subscribe_topic_if_set(event->client, camera_status_topic);
            camera_mqtt_receiver_set_mqtt_connected(true);
        }
        build_topic(topic, sizeof(topic), "out/status");
        esp_mqtt_client_publish(event->client, topic, "{\"status\":\"online\"}", 0, 1, 1);
        ESP_LOGI(TAG, "MQTT connected and subscribed");
        break;
    }
    case MQTT_EVENT_DISCONNECTED:
        s_connected = false;
        camera_mqtt_receiver_set_mqtt_connected(false);
        ESP_LOGI(TAG, "MQTT disconnected");
        break;
    case MQTT_EVENT_DATA: {
        if (camera_mqtt_receiver_accepts_topic(event->topic, event->topic_len)) {
            camera_mqtt_receiver_handle_mqtt_data(event->topic, event->topic_len,
                                                  event->data, event->data_len,
                                                  event->total_data_len,
                                                  event->current_data_offset);
            break;
        }
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

    if (!broker_uri_is_valid(CONFIG_HOMECARE_MQTT_BROKER_URI)) {
        s_initialized = true;
        ESP_LOGW(TAG, "MQTT disabled: invalid broker URI");
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
    mqtt_cfg.session.protocol_ver = MQTT_PROTOCOL_V_5;
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
    mqtt_cfg.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
#endif
    mqtt_cfg.credentials.client_id = CONFIG_HOMECARE_MQTT_CLIENT_ID;
    if (CONFIG_HOMECARE_MQTT_USERNAME[0] != '\0') {
        mqtt_cfg.credentials.username = CONFIG_HOMECARE_MQTT_USERNAME;
    }
    if (CONFIG_HOMECARE_MQTT_PASSWORD[0] != '\0') {
        mqtt_cfg.credentials.authentication.password = CONFIG_HOMECARE_MQTT_PASSWORD;
    }
    mqtt_cfg.session.last_will.topic = CONFIG_HOMECARE_MQTT_BASE_TOPIC "/out/status";
    mqtt_cfg.session.last_will.msg = "{\"status\":\"offline\"}";
    mqtt_cfg.session.last_will.qos = 1;
    mqtt_cfg.session.last_will.retain = 1;
    mqtt_cfg.network.timeout_ms = 6000;
    mqtt_cfg.network.reconnect_timeout_ms = 30000;
    mqtt_cfg.task.stack_size = 4096;
    mqtt_cfg.buffer.size = 768;
    mqtt_cfg.buffer.out_size = 512;
    mqtt_cfg.outbox.limit = 2048;

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
    ESP_LOGI(TAG, "MQTT bridge initialized, broker configured");
    if (station_has_ip()) {
        start_client_if_ready();
    }
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
    if (action <= HOMECARE_MQTT_ACTION_ABNORMAL_KITCHEN) {
        char request_id[HOMECARE_MQTT_FIELD_MAX_LEN] = {};
        const char *prefix = action == HOMECARE_MQTT_ACTION_STOP ? "drv" : "sys";
        snprintf(request_id, sizeof(request_id), "%s_%lld", prefix,
                 static_cast<long long>(esp_timer_get_time() / 1000));
        if (!homecare_mqtt_format_action_with_request_id(action, request_id, &msg)) {
            return ESP_ERR_INVALID_ARG;
        }
        return publish_absolute(&msg);
    }
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

esp_err_t homecare_mqtt_bridge_publish_raw(const char *topic, const char *payload,
                                           int qos, int retain)
{
    if (topic == nullptr || payload == nullptr || s_client == nullptr || !s_connected) {
        return ESP_ERR_INVALID_STATE;
    }

    const int id = esp_mqtt_client_publish(s_client, topic, payload, 0, qos, retain);
    return id >= 0 ? ESP_OK : ESP_FAIL;
}

esp_err_t homecare_mqtt_bridge_publish_event(const HomeCareMqttEvent *event,
                                             homecare_mqtt_mode_t mode)
{
    if (event == nullptr || s_client == nullptr || !s_connected) {
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
