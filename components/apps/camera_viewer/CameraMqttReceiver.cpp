#include "CameraMqttReceiver.hpp"

#include <cstring>

#include "cJSON.h"
#include "esp_check.h"
#include "esp_crt_bundle.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mqtt_bridge/HomeCareMqttBridge.hpp"
#include "jpeg_decoder.h"
#include "mqtt_client.h"
#include "sdkconfig.h"

#ifndef CONFIG_CAMERA_MQTT_ENABLE
#define CONFIG_CAMERA_MQTT_ENABLE 0
#endif

#ifndef CONFIG_CAMERA_MQTT_BROKER_URI
#define CONFIG_CAMERA_MQTT_BROKER_URI ""
#endif

#ifndef CONFIG_CAMERA_MQTT_CLIENT_ID
#define CONFIG_CAMERA_MQTT_CLIENT_ID "brookesia-camera-viewer"
#endif

#ifndef CONFIG_CAMERA_MQTT_USERNAME
#define CONFIG_CAMERA_MQTT_USERNAME ""
#endif

#ifndef CONFIG_CAMERA_MQTT_PASSWORD
#define CONFIG_CAMERA_MQTT_PASSWORD ""
#endif

#ifndef CONFIG_CAMERA_MQTT_JPEG_TOPIC
#define CONFIG_CAMERA_MQTT_JPEG_TOPIC "cam/jpeg"
#endif

#ifndef CONFIG_CAMERA_MQTT_CONTROL_TOPIC
#define CONFIG_CAMERA_MQTT_CONTROL_TOPIC ""
#endif

#ifndef CONFIG_CAMERA_MQTT_STATUS_TOPIC
#define CONFIG_CAMERA_MQTT_STATUS_TOPIC ""
#endif

#ifndef CONFIG_HOMECARE_MQTT_BROKER_URI
#define CONFIG_HOMECARE_MQTT_BROKER_URI ""
#endif

#ifndef CONFIG_CAMERA_MQTT_MAX_JPEG_BYTES
#define CONFIG_CAMERA_MQTT_MAX_JPEG_BYTES 262144
#endif

#ifndef CONFIG_CAMERA_MQTT_BUFFER_SIZE
#define CONFIG_CAMERA_MQTT_BUFFER_SIZE 8192
#endif

#ifndef CONFIG_CAMERA_MQTT_DECODE_TASK_STACK
#define CONFIG_CAMERA_MQTT_DECODE_TASK_STACK 8192
#endif

#ifndef CONFIG_CAMERA_MQTT_DECODE_TASK_PRIORITY
#define CONFIG_CAMERA_MQTT_DECODE_TASK_PRIORITY 5
#endif

static const char *TAG = "camera_mqtt_rx";
static constexpr uint32_t JPEG_RX_DUMP_MAX_BYTES = 4096;
static constexpr uint32_t JPEG_RX_DUMP_MAX_CHUNKS = 12;
static constexpr size_t JPEG_WORK_BUFFER_SIZE = 64 * 1024;

static SemaphoreHandle_t s_lock;
static TaskHandle_t s_decode_task;
static esp_mqtt_client_handle_t s_client;
static bool s_initialized;
static bool s_started;
static bool s_connected;
static bool s_enabled;
static bool s_use_shared_mqtt;
static bool s_active;
static bool s_rx_overflow;
static bool s_jpeg_part_in_progress;

static uint8_t *s_rx_buf;
static uint8_t *s_latest_jpeg;
static uint8_t *s_decode_jpeg;
static uint8_t *s_jpeg_work_buf;
static size_t s_latest_jpeg_len;
static size_t s_last_jpeg_bytes;

static uint8_t *s_rgb_buf[2];
static size_t s_rgb_cap[2];
static lv_img_dsc_t s_img_dsc[2];
static int s_display_index = -1;
static int s_ready_index = -1;

static uint32_t s_frame_version;
static uint32_t s_jpeg_frames;
static uint32_t s_decoded_frames;
static uint32_t s_dropped_frames;
static uint32_t s_decode_errors;
static uint32_t s_status_messages;
static uint32_t s_control_requests;
static uint32_t s_jpeg_rx_chunks;
static uint32_t s_jpeg_rx_dump_chunks;
static uint32_t s_jpeg_rx_dumped_bytes;
static uint32_t s_jpeg_decode_logs;
static int s_last_control_result = ESP_ERR_INVALID_STATE;
static uint32_t s_last_jpeg_ms;
static uint32_t s_last_status_ms;
static uint32_t s_last_control_ms;
static char s_device_mode[12] = "unknown";
static char s_control_topic[96];
static char s_status_topic[96];

static void *camera_malloc(size_t size)
{
    void *ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (ptr == nullptr) {
        ptr = heap_caps_malloc(size, MALLOC_CAP_8BIT);
    }
    return ptr;
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
    return host[0] != '\0' && host[0] != ':' && host[0] != '/';
}

static bool use_shared_homecare_client(void)
{
    return broker_uri_is_valid(CONFIG_HOMECARE_MQTT_BROKER_URI) &&
           broker_uri_is_valid(CONFIG_CAMERA_MQTT_BROKER_URI) &&
           strcmp(CONFIG_HOMECARE_MQTT_BROKER_URI, CONFIG_CAMERA_MQTT_BROKER_URI) == 0;
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

static uint32_t now_ms(void)
{
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

static void build_topics(void)
{
    if (s_control_topic[0] == '\0') {
        if (CONFIG_CAMERA_MQTT_CONTROL_TOPIC[0] != '\0') {
            snprintf(s_control_topic, sizeof(s_control_topic), "%s", CONFIG_CAMERA_MQTT_CONTROL_TOPIC);
        } else {
            snprintf(s_control_topic, sizeof(s_control_topic), "%s/control", CONFIG_CAMERA_MQTT_JPEG_TOPIC);
        }
    }
    if (s_status_topic[0] == '\0') {
        if (CONFIG_CAMERA_MQTT_STATUS_TOPIC[0] != '\0') {
            snprintf(s_status_topic, sizeof(s_status_topic), "%s", CONFIG_CAMERA_MQTT_STATUS_TOPIC);
        } else {
            snprintf(s_status_topic, sizeof(s_status_topic), "%s/status", CONFIG_CAMERA_MQTT_JPEG_TOPIC);
        }
    }
}

static bool topic_equals(const char *topic, int topic_len, const char *expected)
{
    if (topic == nullptr || expected == nullptr || topic_len < 0) {
        return false;
    }
    const size_t expected_len = strlen(expected);
    return static_cast<size_t>(topic_len) == expected_len &&
           strncmp(topic, expected, expected_len) == 0;
}

static void parse_status_payload(const char *data, int data_len)
{
    if (data == nullptr || data_len <= 0) {
        return;
    }

    if (s_lock != nullptr && xSemaphoreTake(s_lock, pdMS_TO_TICKS(10)) == pdPASS) {
        s_status_messages++;
        s_last_status_ms = now_ms();
        xSemaphoreGive(s_lock);
    }

    char json[96] = {};
    const size_t len = data_len < static_cast<int>(sizeof(json) - 1) ?
                       static_cast<size_t>(data_len) : sizeof(json) - 1;
    memcpy(json, data, len);

    cJSON *root = cJSON_Parse(json);
    cJSON *mode = root ? cJSON_GetObjectItemCaseSensitive(root, "mode") : nullptr;
    if (cJSON_IsString(mode) && mode->valuestring != nullptr) {
        if (s_lock != nullptr && xSemaphoreTake(s_lock, pdMS_TO_TICKS(50)) == pdPASS) {
            snprintf(s_device_mode, sizeof(s_device_mode), "%s", mode->valuestring);
            xSemaphoreGive(s_lock);
        }
    }
    cJSON_Delete(root);
}

static void copy_snapshot_locked(CameraMqttSnapshot *out)
{
    memset(out, 0, sizeof(*out));
    out->image = (s_ready_index >= 0) ? &s_img_dsc[s_ready_index] : nullptr;
    out->frame_version = s_frame_version;
    out->jpeg_frames = s_jpeg_frames;
    out->decoded_frames = s_decoded_frames;
    out->dropped_frames = s_dropped_frames;
    out->decode_errors = s_decode_errors;
    out->status_messages = s_status_messages;
    out->control_requests = s_control_requests;
    out->last_control_result = s_last_control_result;
    out->last_jpeg_ms = s_last_jpeg_ms;
    out->last_status_ms = s_last_status_ms;
    out->last_control_ms = s_last_control_ms;
    out->last_jpeg_bytes = s_last_jpeg_bytes;
    out->width = (s_ready_index >= 0) ? s_img_dsc[s_ready_index].header.w : 0;
    out->height = (s_ready_index >= 0) ? s_img_dsc[s_ready_index].header.h : 0;
    out->mqtt_connected = s_connected;
    out->receiver_enabled = s_enabled && s_active;
    snprintf(out->device_mode, sizeof(out->device_mode), "%s", s_device_mode);
}

static bool ensure_rgb_buffer(int index, size_t need)
{
    if (index < 0 || index > 1) {
        return false;
    }
    if (s_rgb_buf[index] != nullptr && s_rgb_cap[index] >= need) {
        return true;
    }
    if (s_rgb_buf[index] != nullptr) {
        heap_caps_free(s_rgb_buf[index]);
        s_rgb_buf[index] = nullptr;
        s_rgb_cap[index] = 0;
    }
    s_rgb_buf[index] = static_cast<uint8_t *>(camera_malloc(need));
    if (s_rgb_buf[index] == nullptr) {
        ESP_LOGE(TAG, "RGB buffer alloc failed, need=%u", static_cast<unsigned>(need));
        return false;
    }
    s_rgb_cap[index] = need;
    return true;
}

static void decode_task(void *arg)
{
    (void)arg;

    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        size_t jpeg_len = 0;
        if (xSemaphoreTake(s_lock, portMAX_DELAY) == pdPASS) {
            jpeg_len = s_latest_jpeg_len;
            if (jpeg_len > 0 && jpeg_len <= CONFIG_CAMERA_MQTT_MAX_JPEG_BYTES) {
                memcpy(s_decode_jpeg, s_latest_jpeg, jpeg_len);
            } else {
                jpeg_len = 0;
            }
            xSemaphoreGive(s_lock);
        }
        if (jpeg_len == 0) {
            continue;
        }

        esp_jpeg_image_cfg_t info_cfg = {};
        info_cfg.indata = s_decode_jpeg;
        info_cfg.indata_size = jpeg_len;
        info_cfg.out_format = JPEG_IMAGE_FORMAT_RGB565;
        info_cfg.out_scale = JPEG_IMAGE_SCALE_0;
        info_cfg.flags.swap_color_bytes = 0;
        info_cfg.advanced.working_buffer = s_jpeg_work_buf;
        info_cfg.advanced.working_buffer_size = JPEG_WORK_BUFFER_SIZE;

        esp_jpeg_image_output_t out_info = {};
        esp_err_t err = esp_jpeg_get_image_info(&info_cfg, &out_info);
        if (err != ESP_OK || out_info.output_len == 0 || out_info.width == 0 || out_info.height == 0) {
            if (xSemaphoreTake(s_lock, portMAX_DELAY) == pdPASS) {
                s_decode_errors++;
                xSemaphoreGive(s_lock);
            }
            ESP_LOGW(TAG, "JPEG info failed: %s", esp_err_to_name(err));
            continue;
        }

        int target = 0;
        if (xSemaphoreTake(s_lock, portMAX_DELAY) == pdPASS) {
            target = (s_display_index == 0) ? 1 : 0;
            xSemaphoreGive(s_lock);
        }

        if (!ensure_rgb_buffer(target, out_info.output_len)) {
            if (xSemaphoreTake(s_lock, portMAX_DELAY) == pdPASS) {
                s_decode_errors++;
                xSemaphoreGive(s_lock);
            }
            continue;
        }

        esp_jpeg_image_cfg_t decode_cfg = info_cfg;
        decode_cfg.outbuf = s_rgb_buf[target];
        decode_cfg.outbuf_size = s_rgb_cap[target];
        err = esp_jpeg_decode(&decode_cfg, &out_info);
        if (err != ESP_OK) {
            if (xSemaphoreTake(s_lock, portMAX_DELAY) == pdPASS) {
                s_decode_errors++;
                xSemaphoreGive(s_lock);
            }
            ESP_LOGW(TAG, "JPEG decode failed: %s", esp_err_to_name(err));
            continue;
        }

        s_img_dsc[target].header.always_zero = 0;
        s_img_dsc[target].header.w = out_info.width;
        s_img_dsc[target].header.h = out_info.height;
        s_img_dsc[target].header.cf = LV_IMG_CF_TRUE_COLOR;
        s_img_dsc[target].data_size = out_info.output_len;
        s_img_dsc[target].data = s_rgb_buf[target];

        if (xSemaphoreTake(s_lock, portMAX_DELAY) == pdPASS) {
            s_ready_index = target;
            s_frame_version++;
            s_decoded_frames++;
            if (s_jpeg_decode_logs < 5 || (s_decoded_frames % 30) == 0) {
                s_jpeg_decode_logs++;
                ESP_LOGI(TAG, "JPEG decoded: %ux%u out=%u decoded=%lu",
                         static_cast<unsigned>(out_info.width),
                         static_cast<unsigned>(out_info.height),
                         static_cast<unsigned>(out_info.output_len),
                         static_cast<unsigned long>(s_decoded_frames));
            }
            xSemaphoreGive(s_lock);
        }
    }
}

static void handle_jpeg_data_parts(const char *data, int data_len, int total_data_len, int current_data_offset)
{
    if (data == nullptr || data_len <= 0 || total_data_len <= 0) {
        return;
    }
    if (total_data_len > CONFIG_CAMERA_MQTT_MAX_JPEG_BYTES) {
        if (!s_rx_overflow || current_data_offset == 0) {
            ESP_LOGW(TAG, "cam/jpeg drop oversize: total=%d max=%d offset=%d chunk=%d",
                     total_data_len, CONFIG_CAMERA_MQTT_MAX_JPEG_BYTES,
                     current_data_offset, data_len);
        }
        s_rx_overflow = true;
        if (current_data_offset + data_len >= total_data_len) {
            s_rx_overflow = false;
            if (s_lock != nullptr && xSemaphoreTake(s_lock, pdMS_TO_TICKS(10)) == pdPASS) {
                s_dropped_frames++;
                xSemaphoreGive(s_lock);
            }
        }
        return;
    }
    if (s_rx_overflow) {
        if (current_data_offset + data_len >= total_data_len) {
            s_rx_overflow = false;
        }
        return;
    }
    if (current_data_offset < 0 ||
        current_data_offset + data_len > CONFIG_CAMERA_MQTT_MAX_JPEG_BYTES) {
        return;
    }

    memcpy(s_rx_buf + current_data_offset, data, data_len);
    if (current_data_offset + data_len < total_data_len) {
        return;
    }

    const size_t frame_len = static_cast<size_t>(total_data_len);
    if (frame_len < 4 || s_rx_buf[0] != 0xff || s_rx_buf[1] != 0xd8) {
        ESP_LOGW(TAG, "cam/jpeg drop invalid header: len=%u head=%02x %02x tail=%02x %02x",
                 static_cast<unsigned>(frame_len),
                 frame_len > 0 ? static_cast<unsigned>(s_rx_buf[0]) : 0,
                 frame_len > 1 ? static_cast<unsigned>(s_rx_buf[1]) : 0,
                 frame_len > 1 ? static_cast<unsigned>(s_rx_buf[frame_len - 2]) : 0,
                 frame_len > 0 ? static_cast<unsigned>(s_rx_buf[frame_len - 1]) : 0);
        if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(10)) == pdPASS) {
            s_dropped_frames++;
            xSemaphoreGive(s_lock);
        }
        return;
    }

    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(50)) == pdPASS) {
        memcpy(s_latest_jpeg, s_rx_buf, frame_len);
        s_latest_jpeg_len = frame_len;
        s_last_jpeg_bytes = frame_len;
        s_jpeg_frames++;
        s_last_jpeg_ms = now_ms();
        if (s_jpeg_frames <= 5 || (s_jpeg_frames % 30) == 0) {
            ESP_LOGI(TAG, "cam/jpeg frame accepted: len=%u frames=%lu",
                     static_cast<unsigned>(frame_len),
                     static_cast<unsigned long>(s_jpeg_frames));
        }
        xSemaphoreGive(s_lock);
        if (s_decode_task != nullptr) {
            xTaskNotifyGive(s_decode_task);
        }
    }
}

static void handle_jpeg_data(const esp_mqtt_event_handle_t event)
{
    if (event == nullptr) {
        return;
    }
    handle_jpeg_data_parts(event->data, event->data_len, event->total_data_len,
                           event->current_data_offset);
}

static esp_err_t start_client_if_ready(void)
{
    if (s_use_shared_mqtt || s_client == nullptr || s_started || !s_enabled) {
        return ESP_OK;
    }
    esp_err_t err = esp_mqtt_client_start(s_client);
    if (err == ESP_OK) {
        s_started = true;
        ESP_LOGI(TAG, "MQTT receiver started");
    } else {
        ESP_LOGW(TAG, "MQTT receiver start failed: %s", esp_err_to_name(err));
    }
    return err;
}

static void stop_client_if_started(void)
{
    if (s_use_shared_mqtt || s_client == nullptr || !s_started) {
        return;
    }
    esp_err_t err = esp_mqtt_client_stop(s_client);
    if (err == ESP_OK) {
        s_started = false;
        s_connected = false;
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)handler_args;
    (void)base;
    esp_mqtt_event_handle_t event = static_cast<esp_mqtt_event_handle_t>(event_data);
    if (event == nullptr) {
        return;
    }

    switch (static_cast<esp_mqtt_event_id_t>(event_id)) {
    case MQTT_EVENT_CONNECTED:
        s_connected = true;
        build_topics();
        esp_mqtt_client_subscribe(event->client, CONFIG_CAMERA_MQTT_JPEG_TOPIC, 0);
        esp_mqtt_client_subscribe(event->client, s_status_topic, 1);
        ESP_LOGI(TAG, "subscribed JPEG=%s status=%s", CONFIG_CAMERA_MQTT_JPEG_TOPIC, s_status_topic);
        break;
    case MQTT_EVENT_DISCONNECTED:
        s_connected = false;
        break;
    case MQTT_EVENT_DATA:
        build_topics();
        if (topic_equals(event->topic, event->topic_len, CONFIG_CAMERA_MQTT_JPEG_TOPIC)) {
            handle_jpeg_data(event);
        } else if (topic_equals(event->topic, event->topic_len, s_status_topic) &&
                   event->current_data_offset == 0) {
            parse_status_payload(event->data, event->data_len);
        }
        break;
    case MQTT_EVENT_ERROR:
        s_connected = false;
        ESP_LOGW(TAG, "MQTT receiver transport error");
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

esp_err_t camera_mqtt_receiver_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }
    s_initialized = true;

    s_enabled = CONFIG_CAMERA_MQTT_ENABLE && broker_uri_is_valid(CONFIG_CAMERA_MQTT_BROKER_URI);
    s_use_shared_mqtt = s_enabled && use_shared_homecare_client();
    if (!s_enabled) {
        ESP_LOGW(TAG, "Camera MQTT receiver disabled: configure broker URI to enable");
        return ESP_OK;
    }

    s_lock = xSemaphoreCreateMutex();
    if (s_lock == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    s_rx_buf = static_cast<uint8_t *>(camera_malloc(CONFIG_CAMERA_MQTT_MAX_JPEG_BYTES));
    s_latest_jpeg = static_cast<uint8_t *>(camera_malloc(CONFIG_CAMERA_MQTT_MAX_JPEG_BYTES));
    s_decode_jpeg = static_cast<uint8_t *>(camera_malloc(CONFIG_CAMERA_MQTT_MAX_JPEG_BYTES));
    s_jpeg_work_buf = static_cast<uint8_t *>(camera_malloc(JPEG_WORK_BUFFER_SIZE));
    if (s_rx_buf == nullptr || s_latest_jpeg == nullptr || s_decode_jpeg == nullptr ||
        s_jpeg_work_buf == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    BaseType_t task_ok = xTaskCreate(decode_task, "cam_jpeg_decode",
                                     CONFIG_CAMERA_MQTT_DECODE_TASK_STACK,
                                     nullptr, CONFIG_CAMERA_MQTT_DECODE_TASK_PRIORITY,
                                     &s_decode_task);
    if (task_ok != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    if (s_use_shared_mqtt) {
        build_topics();
        ESP_LOGI(TAG, "Camera MQTT receiver ready on shared HomeCare MQTT client, JPEG=%s status=%s",
                 CONFIG_CAMERA_MQTT_JPEG_TOPIC, s_status_topic);
        return ESP_OK;
    }

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    const char *username = CONFIG_CAMERA_MQTT_USERNAME[0] != '\0' ? CONFIG_CAMERA_MQTT_USERNAME : nullptr;
    const char *password = CONFIG_CAMERA_MQTT_PASSWORD[0] != '\0' ? CONFIG_CAMERA_MQTT_PASSWORD : nullptr;

    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = CONFIG_CAMERA_MQTT_BROKER_URI;
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
    mqtt_cfg.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
#endif
    mqtt_cfg.credentials.client_id = CONFIG_CAMERA_MQTT_CLIENT_ID;
    mqtt_cfg.credentials.username = username;
    mqtt_cfg.credentials.authentication.password = password;
    mqtt_cfg.buffer.size = CONFIG_CAMERA_MQTT_BUFFER_SIZE;
    mqtt_cfg.buffer.out_size = 1024;
    mqtt_cfg.outbox.limit = 4096;
    mqtt_cfg.network.timeout_ms = 6000;
    mqtt_cfg.network.reconnect_timeout_ms = 10000;
    mqtt_cfg.task.stack_size = 6144;

    // esp-mqtt allocates its input buffer with plain malloc() from internal RAM.
    // If that allocation fails, the managed component's esp_mqtt_set_config()
    // returns ESP_OK anyway (the ESP_MEM_CHECK path leaves err untouched) and
    // esp_mqtt_client_init() hands back a non-NULL but half-destroyed client with
    // client->config == NULL. A later esp_mqtt_client_register_event() then
    // dereferences that NULL config and panics with a Load access fault. Guard
    // against it by refusing to init when there is not enough contiguous internal
    // memory for the buffers up front.
    const size_t needed = static_cast<size_t>(mqtt_cfg.buffer.size) +
                          static_cast<size_t>(mqtt_cfg.buffer.out_size) + 4096;
    const size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (largest < needed) {
        ESP_LOGE(TAG, "Not enough internal memory for MQTT (largest free=%u, need=%u); skipping camera MQTT",
                 static_cast<unsigned>(largest), static_cast<unsigned>(needed));
        return ESP_ERR_NO_MEM;
    }

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_client == nullptr) {
        return ESP_FAIL;
    }
    ESP_RETURN_ON_ERROR(esp_mqtt_client_register_event(s_client,
                                                       static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID),
                                                       mqtt_event_handler, nullptr),
                        TAG, "register mqtt event failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                           network_event_handler, nullptr, nullptr),
                        TAG, "register ip event failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                                           network_event_handler, nullptr, nullptr),
                        TAG, "register wifi event failed");

    ESP_LOGI(TAG, "Camera MQTT receiver ready, broker=%s topic=%s",
             CONFIG_CAMERA_MQTT_BROKER_URI, CONFIG_CAMERA_MQTT_JPEG_TOPIC);
    if (station_has_ip()) {
        start_client_if_ready();
    }
    return ESP_OK;
}

bool camera_mqtt_receiver_get_snapshot(CameraMqttSnapshot *out, uint32_t last_seen_version)
{
    if (out == nullptr || s_lock == nullptr) {
        return false;
    }
    bool has_new = false;
    if (xSemaphoreTake(s_lock, pdMS_TO_TICKS(20)) == pdPASS) {
        has_new = s_frame_version != 0 && s_frame_version != last_seen_version && s_ready_index >= 0;
        if (has_new) {
            s_display_index = s_ready_index;
        }
        copy_snapshot_locked(out);
        xSemaphoreGive(s_lock);
    }
    return has_new;
}

void camera_mqtt_receiver_get_status(CameraMqttSnapshot *out)
{
    if (out == nullptr) {
        return;
    }
    if (s_lock != nullptr && xSemaphoreTake(s_lock, pdMS_TO_TICKS(20)) == pdPASS) {
        copy_snapshot_locked(out);
        xSemaphoreGive(s_lock);
        return;
    }
    memset(out, 0, sizeof(*out));
    out->receiver_enabled = s_enabled && s_active;
    out->mqtt_connected = s_connected;
}

void camera_mqtt_receiver_set_active(bool active)
{
    if (!s_enabled) {
        return;
    }
    s_active = active;
    s_jpeg_part_in_progress = false;
    if (active) {
        s_jpeg_rx_dump_chunks = 0;
        s_jpeg_rx_dumped_bytes = 0;
    }
    ESP_LOGI(TAG, "camera receiver %s", active ? "active" : "inactive");
}

esp_err_t camera_mqtt_receiver_publish_mode(bool remote_mqtt)
{
    build_topics();
    const char *payload = remote_mqtt ?
                          "{\"cmd\":\"system\",\"method\":\"start_scan\",\"place\":\"bathroom\"}" :
                          "{\"cmd\":\"system\",\"method\":\"normal\",\"place\":\"\"}";
    esp_err_t err = ESP_ERR_INVALID_STATE;
    if (s_use_shared_mqtt) {
        err = homecare_mqtt_bridge_publish_raw(s_control_topic, payload, 1, 0);
    } else if (s_client != nullptr && s_connected) {
        int msg_id = esp_mqtt_client_publish(s_client, s_control_topic, payload, 0, 1, 0);
        err = msg_id >= 0 ? ESP_OK : ESP_FAIL;
    }

    if (s_lock != nullptr && xSemaphoreTake(s_lock, pdMS_TO_TICKS(20)) == pdPASS) {
        s_control_requests++;
        s_last_control_result = err;
        s_last_control_ms = now_ms();
        xSemaphoreGive(s_lock);
    }
    ESP_LOGI(TAG, "camera control publish %s: topic=%s result=%s",
             remote_mqtt ? "remote" : "local", s_control_topic, esp_err_to_name(err));
    return err;
}

bool camera_mqtt_receiver_accepts_topic(const char *topic, int topic_len)
{
    if (!s_enabled || !s_active || topic_len < 0) {
        return false;
    }
    if (topic_len == 0) {
        return s_jpeg_part_in_progress;
    }
    if (topic == nullptr) {
        return false;
    }
    build_topics();
    return topic_equals(topic, topic_len, CONFIG_CAMERA_MQTT_JPEG_TOPIC) ||
           topic_equals(topic, topic_len, s_status_topic);
}

void camera_mqtt_receiver_handle_mqtt_data(const char *topic, int topic_len,
                                           const char *data, int data_len,
                                           int total_data_len, int current_data_offset)
{
    if (!s_enabled || !s_active || !s_use_shared_mqtt || data == nullptr) {
        return;
    }
    build_topics();
    const bool jpeg_topic = (topic_len == 0 && s_jpeg_part_in_progress) ||
                            (topic != nullptr && topic_equals(topic, topic_len, CONFIG_CAMERA_MQTT_JPEG_TOPIC));
    if (jpeg_topic) {
        s_jpeg_part_in_progress = current_data_offset + data_len < total_data_len;
        s_jpeg_rx_chunks++;
        if (s_jpeg_rx_chunks <= 20 || (current_data_offset == 0 && (s_jpeg_rx_chunks % 30) == 0)) {
            ESP_LOGI(TAG, "cam/jpeg rx: chunk=%d total=%d offset=%d head=%02x %02x",
                     data_len, total_data_len, current_data_offset,
                     data_len > 0 ? static_cast<unsigned>(static_cast<uint8_t>(data[0])) : 0,
                     data_len > 1 ? static_cast<unsigned>(static_cast<uint8_t>(data[1])) : 0);
        }
        if (s_jpeg_rx_dump_chunks < JPEG_RX_DUMP_MAX_CHUNKS &&
            s_jpeg_rx_dumped_bytes < JPEG_RX_DUMP_MAX_BYTES) {
            const uint32_t remaining = JPEG_RX_DUMP_MAX_BYTES - s_jpeg_rx_dumped_bytes;
            const uint32_t dump_len = static_cast<uint32_t>(data_len) < remaining ?
                                      static_cast<uint32_t>(data_len) : remaining;
            if (dump_len > 0) {
                s_jpeg_rx_dump_chunks++;
                s_jpeg_rx_dumped_bytes += dump_len;
                ESP_LOGI(TAG, "cam/jpeg bytes: chunk_no=%lu offset=%d chunk=%d total=%d dump=%lu dumped=%lu",
                         static_cast<unsigned long>(s_jpeg_rx_dump_chunks),
                         current_data_offset, data_len, total_data_len,
                         static_cast<unsigned long>(dump_len),
                         static_cast<unsigned long>(s_jpeg_rx_dumped_bytes));
                ESP_LOG_BUFFER_HEXDUMP(TAG, data, dump_len, ESP_LOG_INFO);
            }
        }
        handle_jpeg_data_parts(data, data_len, total_data_len, current_data_offset);
    } else if (topic_equals(topic, topic_len, s_status_topic) && current_data_offset == 0) {
        ESP_LOGI(TAG, "cam/jpeg/status rx: len=%d", data_len);
        parse_status_payload(data, data_len);
    }
}

void camera_mqtt_receiver_set_mqtt_connected(bool connected)
{
    if (s_enabled && s_use_shared_mqtt) {
        s_connected = connected;
    }
}
