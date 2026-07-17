#pragma once

#include "CameraMqttReceiver.hpp"
#include "esp_brookesia.hpp"
#include "lvgl.h"

class CameraViewer: public ESP_Brookesia_PhoneApp {
public:
    CameraViewer();
    ~CameraViewer();

    bool init(void) override;
    bool run(void) override;
    bool back(void) override;
    bool close(void) override;

private:
    void createUi(void);
    void startCamera(void);
    void stopCamera(void);
    void refreshUi(bool force);
    void setStatusText(const CameraMqttSnapshot &snapshot);
    static void timerCb(lv_timer_t *timer);
    static void remoteModeCb(lv_event_t *event);
    static void localModeCb(lv_event_t *event);

    lv_obj_t *_root;
    lv_obj_t *_image;
    lv_obj_t *_status_label;
    lv_obj_t *_meta_label;
    lv_obj_t *_test_label;
    lv_obj_t *_empty_label;
    lv_timer_t *_timer;
    uint32_t _last_frame_version;
    bool _camera_active;
};
