#pragma once

#include <cstdint>
#include <array>
#include "lvgl.h"
#include "esp_brookesia.hpp"
#include "mqtt_bridge/HomeCareMqttBridge.hpp"

class HomeCareHub: public ESP_Brookesia_PhoneApp {
public:
    HomeCareHub();
    ~HomeCareHub();

    bool run(void);
    bool back(void);
    bool close(void);
    bool init(void) override;

private:
    enum DemoMode {
        MODE_NORMAL = 0,
        MODE_FALL,
        MODE_BATHROOM,
        MODE_NIGHT,
        MODE_MAX,
    };

    struct RoomState {
        const char *name;
        const char *activity;
        const char *risk;
        int csi;
        bool occupied;
        bool privacy_zone;
        lv_color_t accent;
    };

    struct EventState {
        const char *level;
        const char *time;
        const char *text;
        lv_color_t accent;
    };

    struct DashboardState {
        const char *mode_name;
        const char *home_status;
        const char *privacy;
        const char *weather;
        const char *outdoor_temp;
        const char *humidity;
        const char *air_quality;
        const char *indoor_env;
        const char *night_hint;
        const char *car_status;
        const char *car_position;
        const char *car_target;
        const char *car_phase;
        const char *obstacle;
        const char *sensor;
        const char *voice;
        int battery;
        int route_progress;
        std::array<RoomState, 4> rooms;
        std::array<EventState, 4> events;
    };

    bool createUi(void);
    void setMode(DemoMode mode);
    void updateUi(void);
    void deleteTimer(void);
    void applyMqttMessages(void);
    void applyMqttMessage(const HomeCareMqttInboundMessage &message);
    void updatePageIndicator(int page);
    homecare_mqtt_mode_t toMqttMode(DemoMode mode) const;
    DemoMode fromMqttMode(homecare_mqtt_mode_t mode) const;

    lv_obj_t *createPanel(lv_obj_t *parent, int32_t width, int32_t height, lv_color_t bg);
    lv_obj_t *createLabel(lv_obj_t *parent, const char *text, const lv_font_t *font, lv_color_t color);
    void setCardAccent(lv_obj_t *obj, lv_color_t color);
    void styleHeader(lv_obj_t *obj);
    void styleButton(lv_obj_t *obj, lv_color_t bg, bool filled);
    void styleBar(lv_obj_t *obj, lv_color_t color);

    static void scenarioEventCb(lv_event_t *e);
    static void actionEventCb(lv_event_t *e);
    static void scrollEventCb(lv_event_t *e);
    static void timerCb(lv_timer_t *timer);

    DemoMode _mode;
    lv_timer_t *_timer;
    lv_timer_t *_mqtt_timer;
    uint16_t _width;
    uint16_t _height;
    bool _privacy_enabled;
    bool _has_mqtt_event;
    HomeCareMqttEvent _mqtt_event;
    lv_color_t _mqtt_event_color;
    bool _has_smartcar_attitude;
    HomeCareMqttSmartCarAttitude _smartcar_attitude;
    uint32_t _weather_revision;

    lv_obj_t *_root;
    lv_obj_t *_pages;
    std::array<lv_obj_t *, 3> _page_dots;
    lv_obj_t *_mode_label;
    lv_obj_t *_status_label;
    lv_obj_t *_privacy_label;
    lv_obj_t *_weather_city_label;
    lv_obj_t *_weather_label;
    lv_obj_t *_outdoor_label;
    lv_obj_t *_humidity_label;
    lv_obj_t *_air_label;
    lv_obj_t *_indoor_label;
    lv_obj_t *_night_label;
    lv_obj_t *_car_status_label;
    lv_obj_t *_car_position_label;
    lv_obj_t *_car_target_label;
    lv_obj_t *_car_phase_label;
    lv_obj_t *_obstacle_label;
    lv_obj_t *_sensor_label;
    lv_obj_t *_voice_label;
    lv_obj_t *_battery_bar;
    lv_obj_t *_battery_label;
    lv_obj_t *_route_bar;
    lv_obj_t *_route_label;

    std::array<lv_obj_t *, 4> _room_cards;
    std::array<lv_obj_t *, 4> _room_name_labels;
    std::array<lv_obj_t *, 4> _room_activity_labels;
    std::array<lv_obj_t *, 4> _room_risk_labels;
    std::array<lv_obj_t *, 4> _room_csi_labels;
    std::array<lv_obj_t *, 4> _event_cards;
    std::array<lv_obj_t *, 4> _event_level_labels;
    std::array<lv_obj_t *, 4> _event_time_labels;
    std::array<lv_obj_t *, 4> _event_text_labels;
};
