#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const lv_img_dsc_t *image;
    uint32_t frame_version;
    uint32_t jpeg_frames;
    uint32_t decoded_frames;
    uint32_t dropped_frames;
    uint32_t decode_errors;
    uint32_t status_messages;
    uint32_t control_requests;
    int last_control_result;
    uint32_t last_jpeg_ms;
    uint32_t last_status_ms;
    uint32_t last_control_ms;
    size_t last_jpeg_bytes;
    uint16_t width;
    uint16_t height;
    bool mqtt_connected;
    bool receiver_enabled;
    char device_mode[12];
} CameraMqttSnapshot;

esp_err_t camera_mqtt_receiver_init(void);
void camera_mqtt_receiver_set_active(bool active);
bool camera_mqtt_receiver_get_snapshot(CameraMqttSnapshot *out, uint32_t last_seen_version);
void camera_mqtt_receiver_get_status(CameraMqttSnapshot *out);
esp_err_t camera_mqtt_receiver_publish_mode(bool remote_mqtt);
bool camera_mqtt_receiver_accepts_topic(const char *topic, int topic_len);
void camera_mqtt_receiver_handle_mqtt_data(const char *topic, int topic_len,
                                           const char *data, int data_len,
                                           int total_data_len, int current_data_offset);
void camera_mqtt_receiver_set_mqtt_connected(bool connected);

#ifdef __cplusplus
}
#endif
