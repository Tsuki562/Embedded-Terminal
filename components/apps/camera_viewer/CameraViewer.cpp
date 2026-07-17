#include "CameraViewer.hpp"

#include <algorithm>
#include <cstdio>

#include "esp_timer.h"
#include "esp_log.h"

static const char *TAG = "CameraViewer";

CameraViewer::CameraViewer():
    ESP_Brookesia_PhoneApp("Camera", nullptr, true),
    _root(nullptr),
    _image(nullptr),
    _status_label(nullptr),
    _meta_label(nullptr),
    _test_label(nullptr),
    _empty_label(nullptr),
    _timer(nullptr),
    _last_frame_version(0),
    _camera_active(false)
{
}

CameraViewer::~CameraViewer()
{
    close();
}

bool CameraViewer::init(void)
{
    esp_err_t err = camera_mqtt_receiver_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Camera MQTT receiver init failed: %s", esp_err_to_name(err));
    }
    return true;
}

bool CameraViewer::run(void)
{
    createUi();
    startCamera();
    refreshUi(true);
    _timer = lv_timer_create(timerCb, 50, this);
    return _timer != nullptr;
}

bool CameraViewer::back(void)
{
    stopCamera();
    notifyCoreClosed();
    return true;
}

bool CameraViewer::close(void)
{
    stopCamera();
    if (_timer != nullptr) {
        lv_timer_del(_timer);
        _timer = nullptr;
    }
    _root = nullptr;
    _image = nullptr;
    _status_label = nullptr;
    _meta_label = nullptr;
    _test_label = nullptr;
    _empty_label = nullptr;
    _last_frame_version = 0;
    return true;
}

void CameraViewer::startCamera(void)
{
    if (_camera_active) {
        return;
    }
    _camera_active = true;
    camera_mqtt_receiver_set_active(true);
    esp_err_t err = camera_mqtt_receiver_publish_mode(true);
    ESP_LOGI(TAG, "camera viewer start remote: %s", esp_err_to_name(err));
}

void CameraViewer::stopCamera(void)
{
    if (!_camera_active) {
        return;
    }
    _camera_active = false;
    esp_err_t err = camera_mqtt_receiver_publish_mode(false);
    ESP_LOGI(TAG, "camera viewer stop remote: %s", esp_err_to_name(err));
    camera_mqtt_receiver_set_active(false);
}

static lv_obj_t *makeLabel(lv_obj_t *parent, const char *text, const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    return label;
}

static lv_obj_t *makeButton(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *button = lv_btn_create(parent);
    lv_obj_set_size(button, 150, 44);
    lv_obj_set_style_radius(button, 6, 0);
    lv_obj_add_event_cb(button, cb, LV_EVENT_CLICKED, user_data);
    lv_obj_t *label = makeLabel(button, text, &lv_font_montserrat_18, lv_color_white());
    lv_obj_center(label);
    return button;
}

void CameraViewer::createUi(void)
{
    lv_obj_t *screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x07090f), 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    _root = lv_obj_create(screen);
    lv_obj_set_size(_root, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(_root, lv_color_hex(0x07090f), 0);
    lv_obj_set_style_border_width(_root, 0, 0);
    lv_obj_set_style_pad_all(_root, 18, 0);
    lv_obj_clear_flag(_root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *header = lv_obj_create(_root);
    lv_obj_set_size(header, lv_pct(100), 58);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *title = makeLabel(header, "CameraMode Stream", &lv_font_montserrat_28, lv_color_white());
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 2);

    _status_label = makeLabel(header, "MQTT waiting", &lv_font_montserrat_16, lv_color_hex(0x9ca3af));
    lv_obj_align(_status_label, LV_ALIGN_BOTTOM_LEFT, 2, -2);

    lv_obj_t *actions = lv_obj_create(header);
    lv_obj_set_size(actions, 320, 48);
    lv_obj_set_style_bg_opa(actions, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(actions, 0, 0);
    lv_obj_set_style_pad_all(actions, 0, 0);
    lv_obj_set_style_pad_column(actions, 10, 0);
    lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW);
    lv_obj_align(actions, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_clear_flag(actions, LV_OBJ_FLAG_SCROLLABLE);
    makeButton(actions, "Remote", remoteModeCb, this);
    makeButton(actions, "Local", localModeCb, this);

    lv_obj_t *stage = lv_obj_create(_root);
    int32_t root_h = lv_obj_get_height(_root);
    if (root_h <= 0) {
        root_h = 560;
    }
    lv_obj_set_size(stage, lv_pct(100), std::max<int32_t>(120, root_h - 90));
    lv_obj_set_style_bg_color(stage, lv_color_black(), 0);
    lv_obj_set_style_border_color(stage, lv_color_hex(0x1f2937), 0);
    lv_obj_set_style_border_width(stage, 1, 0);
    lv_obj_set_style_radius(stage, 8, 0);
    lv_obj_set_style_pad_all(stage, 0, 0);
    lv_obj_align(stage, LV_ALIGN_TOP_MID, 0, 72);
    lv_obj_clear_flag(stage, LV_OBJ_FLAG_SCROLLABLE);

    _image = lv_img_create(stage);
    lv_obj_center(_image);

    _empty_label = makeLabel(stage, "No JPEG frame yet", &lv_font_montserrat_24, lv_color_hex(0x9ca3af));
    lv_obj_center(_empty_label);

    _meta_label = makeLabel(stage, "topic: cam/jpeg", &lv_font_montserrat_14, lv_color_hex(0x9ca3af));
    lv_obj_align(_meta_label, LV_ALIGN_BOTTOM_LEFT, 16, -12);

    _test_label = makeLabel(stage, "Camera test: no MQTT packet from camera", &lv_font_montserrat_14,
                            lv_color_hex(0xfbbf24));
    lv_obj_set_width(_test_label, 380);
    lv_label_set_long_mode(_test_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(_test_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(_test_label, LV_ALIGN_BOTTOM_RIGHT, -16, -12);
}

void CameraViewer::setStatusText(const CameraMqttSnapshot &snapshot)
{
    if (_status_label == nullptr || _meta_label == nullptr || _test_label == nullptr) {
        return;
    }
    const char *conn = snapshot.mqtt_connected ? "connected" : "disconnected";
    const char *enabled = snapshot.receiver_enabled ? "enabled" : "disabled";
    const uint32_t now = static_cast<uint32_t>(esp_timer_get_time() / 1000);
    const uint32_t jpeg_age = snapshot.last_jpeg_ms == 0 ? 0 : now - snapshot.last_jpeg_ms;
    const uint32_t status_age = snapshot.last_status_ms == 0 ? 0 : now - snapshot.last_status_ms;
    const uint32_t control_age = snapshot.last_control_ms == 0 ? 0 : now - snapshot.last_control_ms;
    lv_label_set_text_fmt(_status_label, "receiver %s | MQTT %s | device %s",
                          enabled, conn, snapshot.device_mode[0] ? snapshot.device_mode : "unknown");
    lv_label_set_text_fmt(_meta_label, "JPEG %lu | decoded %lu | dropped %lu | errors %lu | last %u B",
                          static_cast<unsigned long>(snapshot.jpeg_frames),
                          static_cast<unsigned long>(snapshot.decoded_frames),
                          static_cast<unsigned long>(snapshot.dropped_frames),
                          static_cast<unsigned long>(snapshot.decode_errors),
                          static_cast<unsigned>(snapshot.last_jpeg_bytes));
    if (snapshot.jpeg_frames > 0) {
        lv_label_set_text_fmt(_test_label, "Camera test: JPEG received %lums ago",
                              static_cast<unsigned long>(jpeg_age));
        lv_obj_set_style_text_color(_test_label, lv_color_hex(0x34d399), 0);
    } else if (snapshot.status_messages > 0) {
        lv_label_set_text_fmt(_test_label, "Camera test: status received %lums ago, no JPEG yet",
                              static_cast<unsigned long>(status_age));
        lv_obj_set_style_text_color(_test_label, lv_color_hex(0xfbbf24), 0);
    } else if (snapshot.control_requests > 0) {
        lv_label_set_text_fmt(_test_label, "Camera test: control %s %lums ago, no camera packet",
                              snapshot.last_control_result == ESP_OK ? "sent" : "failed",
                              static_cast<unsigned long>(control_age));
        lv_obj_set_style_text_color(_test_label,
                                    snapshot.last_control_result == ESP_OK ?
                                    lv_color_hex(0xfbbf24) : lv_color_hex(0xf87171), 0);
    } else {
        lv_label_set_text(_test_label, "Camera test: press Remote, waiting for cam/jpeg or status");
        lv_obj_set_style_text_color(_test_label, lv_color_hex(0xfbbf24), 0);
    }
}

void CameraViewer::refreshUi(bool force)
{
    CameraMqttSnapshot snapshot = {};
    bool has_new_frame = camera_mqtt_receiver_get_snapshot(&snapshot, _last_frame_version);
    if (!has_new_frame) {
        camera_mqtt_receiver_get_status(&snapshot);
    }
    setStatusText(snapshot);

    if (!has_new_frame || snapshot.image == nullptr || _image == nullptr) {
        return;
    }

    _last_frame_version = snapshot.frame_version;
    lv_img_set_src(_image, snapshot.image);
    lv_img_set_pivot(_image, snapshot.width / 2, snapshot.height / 2);
    lv_img_set_angle(_image, 1800);

    lv_obj_t *stage = lv_obj_get_parent(_image);
    const int32_t stage_w = lv_obj_get_width(stage);
    const int32_t stage_h = lv_obj_get_height(stage);
    const int32_t usable_w = std::max<int32_t>(1, stage_w - 24);
    const int32_t usable_h = std::max<int32_t>(1, stage_h - 48);
    int32_t zoom_w = static_cast<int32_t>(usable_w * 256 / std::max<uint16_t>(1, snapshot.width));
    int32_t zoom_h = static_cast<int32_t>(usable_h * 256 / std::max<uint16_t>(1, snapshot.height));
    int32_t zoom = std::min<int32_t>(zoom_w, zoom_h);
    zoom = std::max<int32_t>(64, std::min<int32_t>(zoom, 768));
    lv_img_set_zoom(_image, static_cast<uint16_t>(zoom));
    lv_obj_center(_image);
    if (_empty_label != nullptr) {
        lv_obj_add_flag(_empty_label, LV_OBJ_FLAG_HIDDEN);
    }
    if (force) {
        lv_obj_invalidate(_image);
    }
}

void CameraViewer::timerCb(lv_timer_t *timer)
{
    CameraViewer *app = static_cast<CameraViewer *>(timer->user_data);
    if (app != nullptr) {
        app->refreshUi(false);
    }
}

void CameraViewer::remoteModeCb(lv_event_t *event)
{
    CameraViewer *app = static_cast<CameraViewer *>(lv_event_get_user_data(event));
    (void)app;
    camera_mqtt_receiver_publish_mode(true);
}

void CameraViewer::localModeCb(lv_event_t *event)
{
    CameraViewer *app = static_cast<CameraViewer *>(lv_event_get_user_data(event));
    (void)app;
    camera_mqtt_receiver_publish_mode(false);
}
