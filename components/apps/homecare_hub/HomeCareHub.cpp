#include <cstdio>
#include <cstdint>
#include <cstring>
#include <ctime>
#include "HomeCareHub.hpp"
#include "HomeCareWeather.hpp"
#include "HomeCareWeatherCity.hpp"

LV_IMG_DECLARE(img_app_setting);
LV_FONT_DECLARE(homecare_font_simsun_14);
LV_FONT_DECLARE(homecare_font_simsun_16);
LV_FONT_DECLARE(homecare_font_simsun_20);
LV_FONT_DECLARE(homecare_font_simsun_28);

#define HUB_BG_COLOR           lv_color_hex(0x0C0F10)
#define HUB_BG_GRAD_COLOR      lv_color_hex(0x080A0B)
#define HUB_PANEL_COLOR        lv_color_hex(0x121718)
#define HUB_PANEL_SOLID_COLOR  lv_color_hex(0x151B1D)
#define HUB_LINE_COLOR         lv_color_hex(0x2B3435)
#define HUB_LINE_STRONG_COLOR  lv_color_hex(0x465254)
#define HUB_SHADOW_COLOR       lv_color_hex(0x020303)
#define HUB_TEXT_COLOR         lv_color_hex(0xF7FBFA)
#define HUB_MUTED_COLOR        lv_color_hex(0x9BA8A6)
#define HUB_FAINT_COLOR        lv_color_hex(0x687674)
#define HUB_CYAN_COLOR         lv_color_hex(0x3FE0D0)
#define HUB_GREEN_COLOR        lv_color_hex(0x9BE86F)
#define HUB_AMBER_COLOR        lv_color_hex(0xFFC35A)
#define HUB_CORAL_COLOR        lv_color_hex(0xFF7F73)
#define HUB_BLUE_COLOR         lv_color_hex(0x78A8FF)

#define HUB_FONT_TITLE         (&homecare_font_simsun_28)
#define HUB_FONT_HEAD          (&homecare_font_simsun_20)
#define HUB_FONT_BODY          (&homecare_font_simsun_16)
#define HUB_FONT_SMALL         (&homecare_font_simsun_14)
#define HUB_PAGE_COUNT         (3)
#define HUB_PAGE_GAP           (12)

static void applyLabelStyle(lv_obj_t *label, const lv_font_t *font, lv_color_t color)
{
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_letter_space(label, 0, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
}

static void styleFlatContainer(lv_obj_t *obj)
{
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

static lv_obj_t *createIconLabel(lv_obj_t *parent, const char *symbol, lv_color_t color)
{
    lv_obj_t *icon = lv_label_create(parent);
    lv_label_set_text(icon, symbol);
    lv_obj_set_style_text_font(icon, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_color(icon, color, 0);
    return icon;
}

static lv_obj_t *createCameraPreview(lv_obj_t *parent, int32_t width, int32_t height)
{
    lv_obj_t *camera = lv_obj_create(parent);
    lv_obj_set_size(camera, width, height);
    lv_obj_set_style_bg_color(camera, lv_color_hex(0x0A0E0F), 0);
    lv_obj_set_style_bg_grad_color(camera, lv_color_hex(0x1A2425), 0);
    lv_obj_set_style_bg_grad_dir(camera, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_border_width(camera, 1, 0);
    lv_obj_set_style_border_color(camera, HUB_LINE_COLOR, 0);
    lv_obj_set_style_radius(camera, 8, 0);
    lv_obj_set_style_pad_all(camera, 0, 0);
    lv_obj_clear_flag(camera, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *cross_1 = lv_obj_create(camera);
    lv_obj_set_size(cross_1, width - 28, 1);
    lv_obj_align(cross_1, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(cross_1, lv_color_hex(0x263435), 0);
    lv_obj_set_style_border_width(cross_1, 0, 0);
    lv_obj_set_style_pad_all(cross_1, 0, 0);
    lv_obj_clear_flag(cross_1, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *cross_2 = lv_obj_create(camera);
    lv_obj_set_size(cross_2, 1, height - 16);
    lv_obj_align(cross_2, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(cross_2, lv_color_hex(0x263435), 0);
    lv_obj_set_style_border_width(cross_2, 0, 0);
    lv_obj_set_style_pad_all(cross_2, 0, 0);
    lv_obj_clear_flag(cross_2, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *live = lv_label_create(camera);
    lv_label_set_text(live, "LIVE");
    applyLabelStyle(live, HUB_FONT_SMALL, HUB_CORAL_COLOR);
    lv_obj_set_style_bg_color(live, HUB_CORAL_COLOR, 0);
    lv_obj_set_style_bg_opa(live, LV_OPA_20, 0);
    lv_obj_set_style_border_width(live, 1, 0);
    lv_obj_set_style_border_color(live, HUB_CORAL_COLOR, 0);
    lv_obj_set_style_radius(live, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_left(live, 7, 0);
    lv_obj_set_style_pad_right(live, 7, 0);
    lv_obj_set_style_pad_top(live, 2, 0);
    lv_obj_set_style_pad_bottom(live, 2, 0);
    lv_obj_align(live, LV_ALIGN_TOP_RIGHT, -8, 7);
    return camera;
}

HomeCareHub::HomeCareHub():
    ESP_Brookesia_PhoneApp("家庭终端", &img_app_setting, true),
    _mode(MODE_NORMAL),
    _timer(nullptr),
    _mqtt_timer(nullptr),
    _width(1024),
    _height(600),
    _privacy_enabled(true),
    _has_mqtt_event(false),
    _mqtt_event({}),
    _mqtt_event_color(HUB_BLUE_COLOR),
    _has_smartcar_attitude(false),
    _smartcar_attitude({}),
    _weather_revision(0),
    _root(nullptr),
    _pages(nullptr),
    _page_dots({}),
    _time_label(nullptr),
    _date_label(nullptr),
    _top_weather_label(nullptr),
    _mode_label(nullptr),
    _status_label(nullptr),
    _privacy_label(nullptr),
    _weather_city_label(nullptr),
    _weather_label(nullptr),
    _weather_temp_label(nullptr),
    _weather_summary_label(nullptr),
    _aqi_label(nullptr),
    _co2_label(nullptr),
    _noise_label(nullptr),
    _outdoor_label(nullptr),
    _humidity_label(nullptr),
    _air_label(nullptr),
    _indoor_label(nullptr),
    _night_label(nullptr),
    _hero_kicker_label(nullptr),
    _hero_room_label(nullptr),
    _hero_text_label(nullptr),
    _security_chip_label(nullptr),
    _comfort_temp_label(nullptr),
    _comfort_status_label(nullptr),
    _temp_arc(nullptr),
    _light_label(nullptr),
    _power_label(nullptr),
    _online_count_label(nullptr),
    _camera_status_label(nullptr),
    _assistant_text_label(nullptr),
    _car_status_label(nullptr),
    _car_position_label(nullptr),
    _car_target_label(nullptr),
    _car_phase_label(nullptr),
    _obstacle_label(nullptr),
    _sensor_label(nullptr),
    _voice_label(nullptr),
    _battery_bar(nullptr),
    _battery_label(nullptr),
    _route_bar(nullptr),
    _route_label(nullptr),
    _room_cards({}),
    _room_accent_bars({}),
    _room_name_labels({}),
    _room_activity_labels({}),
    _room_risk_labels({}),
    _room_csi_labels({}),
    _event_cards({}),
    _event_dot_labels({}),
    _event_level_labels({}),
    _event_time_labels({}),
    _event_text_labels({}),
    _scene_cards({}),
    _scene_name_labels({}),
    _scene_desc_labels({}),
    _device_cards({}),
    _device_state_labels({}),
    _device_switches({}),
    _energy_bars({}),
    _energy_value_labels({})
{
}

HomeCareHub::~HomeCareHub()
{
    deleteTimer();
}

bool HomeCareHub::init(void)
{
    homecare_mqtt_bridge_init();
    homecare_weather_service_init();
    return true;
}

bool HomeCareHub::run(void)
{
    if (_root != nullptr) {
        close();
    }

    lv_area_t area = getVisualArea();
    const int32_t visual_w = area.x2 >= area.x1 ? (area.x2 - area.x1 + 1) : 0;
    const int32_t visual_h = area.y2 >= area.y1 ? (area.y2 - area.y1 + 1) : 0;
    _width = visual_w > 0 ? static_cast<uint16_t>(visual_w) : static_cast<uint16_t>(lv_disp_get_hor_res(nullptr));
    _height = visual_h > 0 ? static_cast<uint16_t>(visual_h) : static_cast<uint16_t>(lv_disp_get_ver_res(nullptr));

    createUi();
    setMode(MODE_NORMAL);
    _timer = lv_timer_create(timerCb, 6000, this);
    _mqtt_timer = lv_timer_create(timerCb, 1000, this);
    return true;
}

bool HomeCareHub::back(void)
{
    notifyCoreClosed();
    return true;
}

bool HomeCareHub::close(void)
{
    deleteTimer();
    if (_root != nullptr) {
        lv_obj_del(_root);
        _root = nullptr;
    }
    _pages = nullptr;
    _page_dots.fill(nullptr);
    _time_label = nullptr;
    _date_label = nullptr;
    _top_weather_label = nullptr;
    _mode_label = nullptr;
    _status_label = nullptr;
    _privacy_label = nullptr;
    _weather_city_label = nullptr;
    _weather_label = nullptr;
    _weather_temp_label = nullptr;
    _weather_summary_label = nullptr;
    _aqi_label = nullptr;
    _co2_label = nullptr;
    _noise_label = nullptr;
    _outdoor_label = nullptr;
    _humidity_label = nullptr;
    _air_label = nullptr;
    _indoor_label = nullptr;
    _night_label = nullptr;
    _hero_kicker_label = nullptr;
    _hero_room_label = nullptr;
    _hero_text_label = nullptr;
    _security_chip_label = nullptr;
    _comfort_temp_label = nullptr;
    _comfort_status_label = nullptr;
    _temp_arc = nullptr;
    _light_label = nullptr;
    _power_label = nullptr;
    _online_count_label = nullptr;
    _camera_status_label = nullptr;
    _assistant_text_label = nullptr;
    _car_status_label = nullptr;
    _car_position_label = nullptr;
    _car_target_label = nullptr;
    _car_phase_label = nullptr;
    _obstacle_label = nullptr;
    _sensor_label = nullptr;
    _voice_label = nullptr;
    _battery_bar = nullptr;
    _battery_label = nullptr;
    _route_bar = nullptr;
    _route_label = nullptr;
    _room_cards.fill(nullptr);
    _room_accent_bars.fill(nullptr);
    _room_name_labels.fill(nullptr);
    _room_activity_labels.fill(nullptr);
    _room_risk_labels.fill(nullptr);
    _room_csi_labels.fill(nullptr);
    _event_cards.fill(nullptr);
    _event_dot_labels.fill(nullptr);
    _event_level_labels.fill(nullptr);
    _event_time_labels.fill(nullptr);
    _event_text_labels.fill(nullptr);
    _scene_cards.fill(nullptr);
    _scene_name_labels.fill(nullptr);
    _scene_desc_labels.fill(nullptr);
    _device_cards.fill(nullptr);
    _device_state_labels.fill(nullptr);
    _device_switches.fill(nullptr);
    _energy_bars.fill(nullptr);
    _energy_value_labels.fill(nullptr);
    return true;
}

void HomeCareHub::deleteTimer(void)
{
    if (_timer != nullptr) {
        lv_timer_del(_timer);
        _timer = nullptr;
    }
    if (_mqtt_timer != nullptr) {
        lv_timer_del(_mqtt_timer);
        _mqtt_timer = nullptr;
    }
}

lv_obj_t *HomeCareHub::createPanel(lv_obj_t *parent, int32_t width, int32_t height, lv_color_t bg)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, width, height);
    lv_obj_set_style_bg_color(panel, bg, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, HUB_LINE_COLOR, 0);
    lv_obj_set_style_border_opa(panel, LV_OPA_80, 0);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_set_style_pad_top(panel, 12, 0);
    lv_obj_set_style_pad_bottom(panel, 12, 0);
    lv_obj_set_style_pad_left(panel, 12, 0);
    lv_obj_set_style_pad_right(panel, 12, 0);
    lv_obj_set_style_shadow_width(panel, 18, 0);
    lv_obj_set_style_shadow_ofs_y(panel, 8, 0);
    lv_obj_set_style_shadow_color(panel, HUB_SHADOW_COLOR, 0);
    lv_obj_set_style_shadow_opa(panel, LV_OPA_40, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    return panel;
}

lv_obj_t *HomeCareHub::createLabel(lv_obj_t *parent, const char *text, const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    applyLabelStyle(label, font, color);
    return label;
}

lv_obj_t *HomeCareHub::createStatusPill(lv_obj_t *parent, const char *icon, const char *text, lv_color_t color)
{
    lv_obj_t *pill = lv_obj_create(parent);
    lv_obj_set_size(pill, LV_SIZE_CONTENT, 34);
    lv_obj_set_style_bg_color(pill, HUB_PANEL_SOLID_COLOR, 0);
    lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pill, 1, 0);
    lv_obj_set_style_border_color(pill, HUB_LINE_COLOR, 0);
    lv_obj_set_style_radius(pill, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_left(pill, 10, 0);
    lv_obj_set_style_pad_right(pill, 10, 0);
    lv_obj_set_style_pad_top(pill, 5, 0);
    lv_obj_set_style_pad_bottom(pill, 5, 0);
    lv_obj_set_style_pad_column(pill, 6, 0);
    lv_obj_set_flex_flow(pill, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pill, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(pill, LV_OBJ_FLAG_SCROLLABLE);

    createIconLabel(pill, icon, color);
    lv_obj_t *label = createLabel(pill, text, HUB_FONT_SMALL, color);
    if (text == nullptr || text[0] == '\0') {
        lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
    }
    return label;
}

lv_obj_t *HomeCareHub::createSectionHeader(lv_obj_t *parent, const char *title, const char *note,
                                           int32_t width, int32_t height)
{
    lv_obj_t *header = lv_obj_create(parent);
    lv_obj_set_size(header, width, height);
    styleFlatContainer(header);
    lv_obj_t *title_label = createLabel(header, title, HUB_FONT_BODY, HUB_TEXT_COLOR);
    lv_obj_align(title_label, LV_ALIGN_LEFT_MID, 0, 0);
    if (note != nullptr) {
        lv_obj_t *note_label = createLabel(header, note, HUB_FONT_SMALL, HUB_MUTED_COLOR);
        lv_obj_align(note_label, LV_ALIGN_RIGHT_MID, 0, 0);
    }
    return header;
}

void HomeCareHub::setCardAccent(lv_obj_t *obj, lv_obj_t *accent, lv_color_t color)
{
    if (obj == nullptr || accent == nullptr) {
        return;
    }

    lv_obj_set_size(accent, 3, lv_obj_get_height(obj));
    lv_obj_align(accent, LV_ALIGN_LEFT_MID, -12, 0);
    lv_obj_set_style_bg_color(accent, color, 0);
    lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(accent, 0, 0);
    lv_obj_set_style_shadow_color(obj, lv_color_mix(color, HUB_SHADOW_COLOR, 14), 0);
}

void HomeCareHub::styleHeader(lv_obj_t *obj)
{
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_border_side(obj, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, HUB_LINE_COLOR, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

void HomeCareHub::styleButton(lv_obj_t *obj, lv_color_t bg, bool filled)
{
    lv_obj_set_style_radius(obj, 8, 0);
    lv_obj_set_style_bg_color(obj, filled ? bg : HUB_PANEL_SOLID_COLOR, 0);
    lv_obj_set_style_bg_opa(obj, filled ? LV_OPA_COVER : LV_OPA_80, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, filled ? bg : HUB_LINE_COLOR, 0);
    lv_obj_set_style_shadow_width(obj, filled ? 8 : 0, 0);
    lv_obj_set_style_shadow_ofs_y(obj, 4, 0);
    lv_obj_set_style_shadow_color(obj, HUB_SHADOW_COLOR, 0);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_30, 0);
    lv_obj_set_style_bg_color(obj, lv_color_mix(lv_color_black(), bg, 74), LV_STATE_PRESSED);
    lv_obj_set_style_transform_height(obj, -1, LV_STATE_PRESSED);
}

void HomeCareHub::styleBar(lv_obj_t *obj, lv_color_t color)
{
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x283132), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(obj, color, LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_color(obj, lv_color_mix(lv_color_white(), color, 40), LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
}

void HomeCareHub::updatePageIndicator(int page)
{
    for (int i = 0; i < HUB_PAGE_COUNT; ++i) {
        if (_page_dots[i] == nullptr) {
            continue;
        }
        const bool active = (i == page);
        lv_obj_set_size(_page_dots[i], active ? 22 : 8, 8);
        lv_obj_set_style_bg_color(_page_dots[i], active ? HUB_CYAN_COLOR : HUB_FAINT_COLOR, 0);
    }
}

bool HomeCareHub::createUi(void)
{
    _root = lv_obj_create(lv_scr_act());
    lv_obj_set_size(_root, _width, _height);
    lv_obj_align(_root, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(_root, HUB_BG_COLOR, 0);
    lv_obj_set_style_bg_grad_color(_root, HUB_BG_GRAD_COLOR, 0);
    lv_obj_set_style_bg_grad_dir(_root, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_width(_root, 0, 0);
    lv_obj_set_style_radius(_root, 0, 0);
    lv_obj_set_style_pad_all(_root, 12, 0);
    lv_obj_clear_flag(_root, LV_OBJ_FLAG_SCROLLABLE);

    const int gap = 12;
    const int top_h = 62;
    const int content_w = _width - 24;
    const int content_y = 12 + top_h + gap;
    const int content_h = _height - content_y - 12;
    const int left_w = 240;
    const int right_w = 240;
    const int center_w = content_w - left_w - right_w - gap * 2;

    lv_obj_t *top = lv_obj_create(_root);
    lv_obj_set_size(top, content_w, top_h);
    lv_obj_align(top, LV_ALIGN_TOP_MID, 0, 0);
    styleHeader(top);

    lv_obj_t *brand_mark = lv_obj_create(top);
    lv_obj_set_size(brand_mark, 42, 42);
    lv_obj_align(brand_mark, LV_ALIGN_LEFT_MID, 0, -4);
    lv_obj_set_style_bg_color(brand_mark, HUB_PANEL_SOLID_COLOR, 0);
    lv_obj_set_style_border_width(brand_mark, 1, 0);
    lv_obj_set_style_border_color(brand_mark, HUB_LINE_STRONG_COLOR, 0);
    lv_obj_set_style_radius(brand_mark, 8, 0);
    lv_obj_set_style_pad_all(brand_mark, 0, 0);
    lv_obj_clear_flag(brand_mark, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *brand_icon = createIconLabel(brand_mark, LV_SYMBOL_HOME, HUB_CYAN_COLOR);
    lv_obj_center(brand_icon);

    lv_obj_t *brand = createLabel(top, "Astra Home", HUB_FONT_HEAD, HUB_TEXT_COLOR);
    lv_obj_align_to(brand, brand_mark, LV_ALIGN_OUT_RIGHT_TOP, 10, 1);
    lv_obj_t *brand_subtitle = createLabel(top, "晴湾公寓 · 智能中控", HUB_FONT_SMALL, HUB_MUTED_COLOR);
    lv_obj_align_to(brand_subtitle, brand, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 3);
    _status_label = brand_subtitle;

    _time_label = createLabel(top, "08:24", HUB_FONT_TITLE, HUB_TEXT_COLOR);
    lv_obj_align(_time_label, LV_ALIGN_TOP_MID, 0, 1);
    _date_label = createLabel(top, "5月26日 星期二", HUB_FONT_SMALL, HUB_MUTED_COLOR);
    lv_obj_align_to(_date_label, _time_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    lv_obj_t *top_actions = lv_obj_create(top);
    lv_obj_set_size(top_actions, 310, 40);
    lv_obj_align(top_actions, LV_ALIGN_RIGHT_MID, 0, -5);
    styleFlatContainer(top_actions);
    lv_obj_set_style_pad_column(top_actions, 8, 0);
    lv_obj_set_flex_flow(top_actions, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top_actions, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    _mode_label = createStatusPill(top_actions, LV_SYMBOL_OK, "全屋安全", HUB_GREEN_COLOR);
    _top_weather_label = createStatusPill(top_actions, LV_SYMBOL_SETTINGS, "26°C 室外", HUB_MUTED_COLOR);
    _privacy_label = createStatusPill(top_actions, LV_SYMBOL_EYE_OPEN, "在家模式", HUB_CYAN_COLOR);

    lv_obj_t *left = lv_obj_create(_root);
    lv_obj_set_size(left, left_w, content_h);
    lv_obj_align(left, LV_ALIGN_TOP_LEFT, 0, content_y - 12);
    styleFlatContainer(left);

    lv_obj_t *center = createPanel(_root, center_w, content_h, HUB_PANEL_COLOR);
    lv_obj_align_to(center, left, LV_ALIGN_OUT_RIGHT_TOP, gap, 0);

    lv_obj_t *right = lv_obj_create(_root);
    lv_obj_set_size(right, right_w, content_h);
    lv_obj_align_to(right, center, LV_ALIGN_OUT_RIGHT_TOP, gap, 0);
    styleFlatContainer(right);

    const int weather_h = 132;
    const int rooms_h = 206;
    const int auto_h = content_h - weather_h - rooms_h - gap * 2;

    lv_obj_t *weather = createPanel(left, left_w, weather_h, HUB_PANEL_COLOR);
    lv_obj_align(weather, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_grad_color(weather, lv_color_hex(0x172020), 0);
    lv_obj_set_style_bg_grad_dir(weather, LV_GRAD_DIR_HOR, 0);
    _weather_temp_label = createLabel(weather, "25°", HUB_FONT_TITLE, HUB_TEXT_COLOR);
    lv_obj_align(_weather_temp_label, LV_ALIGN_TOP_LEFT, 0, 4);
    _weather_summary_label = createLabel(weather, "客厅舒适 · 湿度 42%", HUB_FONT_SMALL, HUB_MUTED_COLOR);
    lv_obj_align(_weather_summary_label, LV_ALIGN_TOP_LEFT, 2, 58);
    lv_obj_t *weather_icon_box = lv_obj_create(weather);
    lv_obj_set_size(weather_icon_box, 48, 48);
    lv_obj_align(weather_icon_box, LV_ALIGN_TOP_RIGHT, 0, 6);
    lv_obj_set_style_bg_color(weather_icon_box, lv_color_hex(0x1C2828), 0);
    lv_obj_set_style_border_width(weather_icon_box, 1, 0);
    lv_obj_set_style_border_color(weather_icon_box, HUB_LINE_COLOR, 0);
    lv_obj_set_style_radius(weather_icon_box, 8, 0);
    lv_obj_set_style_pad_all(weather_icon_box, 0, 0);
    lv_obj_clear_flag(weather_icon_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *weather_icon = createIconLabel(weather_icon_box, LV_SYMBOL_SETTINGS, HUB_CYAN_COLOR);
    lv_obj_center(weather_icon);
    _weather_city_label = createLabel(weather, homecare_weather_city_get_selected_name(), HUB_FONT_SMALL, HUB_FAINT_COLOR);
    lv_obj_align(_weather_city_label, LV_ALIGN_TOP_RIGHT, 0, 58);
    _weather_label = createLabel(weather, "多云", HUB_FONT_SMALL, HUB_CYAN_COLOR);
    lv_obj_align(_weather_label, LV_ALIGN_TOP_RIGHT, 0, 76);

    lv_obj_t *mini_metrics = lv_obj_create(weather);
    lv_obj_set_size(mini_metrics, left_w - 24, 34);
    lv_obj_align(mini_metrics, LV_ALIGN_BOTTOM_MID, 0, 0);
    styleFlatContainer(mini_metrics);
    lv_obj_set_style_pad_column(mini_metrics, 7, 0);
    lv_obj_set_flex_flow(mini_metrics, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(mini_metrics, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    const char *metric_names[] = {"AQI", "CO2", "噪声"};
    lv_obj_t **metric_values[] = {&_aqi_label, &_co2_label, &_noise_label};
    for (int i = 0; i < 3; ++i) {
        lv_obj_t *metric = createPanel(mini_metrics, 66, 34, HUB_PANEL_SOLID_COLOR);
        lv_obj_set_style_shadow_width(metric, 0, 0);
        lv_obj_set_style_pad_all(metric, 5, 0);
        lv_obj_t *name = createLabel(metric, metric_names[i], HUB_FONT_SMALL, HUB_MUTED_COLOR);
        lv_obj_align(name, LV_ALIGN_TOP_LEFT, 0, -1);
        *metric_values[i] = createLabel(metric, i == 0 ? "32" : (i == 1 ? "620" : "28dB"), HUB_FONT_SMALL, HUB_TEXT_COLOR);
        lv_obj_align(*metric_values[i], LV_ALIGN_BOTTOM_RIGHT, 0, 1);
    }

    lv_obj_t *rooms = createPanel(left, left_w, rooms_h, HUB_PANEL_COLOR);
    lv_obj_align_to(rooms, weather, LV_ALIGN_OUT_BOTTOM_LEFT, 0, gap);
    createSectionHeader(rooms, "房间", "6 区在线", left_w - 24, 30);
    const char *room_names[] = {"客厅", "主卧", "厨房", "书房"};
    const char *room_desc[] = {"主灯 68% · 空调 25°", "窗帘关闭 · 静音", "排风运行 · 烟感正常", "台灯 42% · 空气优"};
    const char *room_tags[] = {"舒适", "睡眠", "运行", "安静"};
    const char *room_icons[] = {LV_SYMBOL_HOME, LV_SYMBOL_AUDIO, LV_SYMBOL_SETTINGS, LV_SYMBOL_EDIT};
    const int room_row_h = 37;
    for (int i = 0; i < 4; ++i) {
        lv_obj_t *card = createPanel(rooms, left_w - 24, room_row_h, i == 0 ? lv_color_hex(0x172324) : HUB_PANEL_SOLID_COLOR);
        lv_obj_align(card, LV_ALIGN_TOP_LEFT, 0, 34 + i * (room_row_h + 5));
        lv_obj_set_style_shadow_width(card, 0, 0);
        lv_obj_set_style_pad_left(card, 8, 0);
        lv_obj_set_style_pad_right(card, 8, 0);
        lv_obj_set_style_pad_top(card, 5, 0);
        lv_obj_set_style_pad_bottom(card, 5, 0);
        _room_cards[i] = card;
        _room_accent_bars[i] = lv_obj_create(card);
        lv_obj_clear_flag(_room_accent_bars[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_move_background(_room_accent_bars[i]);
        lv_obj_t *icon = createIconLabel(card, room_icons[i], i == 0 ? HUB_CYAN_COLOR : HUB_MUTED_COLOR);
        lv_obj_align(icon, LV_ALIGN_LEFT_MID, 0, 0);
        _room_name_labels[i] = createLabel(card, room_names[i], HUB_FONT_SMALL, HUB_TEXT_COLOR);
        lv_obj_align(_room_name_labels[i], LV_ALIGN_TOP_LEFT, 24, -1);
        _room_activity_labels[i] = createLabel(card, room_desc[i], HUB_FONT_SMALL, HUB_MUTED_COLOR);
        lv_obj_set_width(_room_activity_labels[i], left_w - 108);
        lv_obj_align(_room_activity_labels[i], LV_ALIGN_BOTTOM_LEFT, 24, 1);
        _room_risk_labels[i] = createLabel(card, room_tags[i], HUB_FONT_SMALL, i == 0 ? HUB_CYAN_COLOR : HUB_MUTED_COLOR);
        lv_obj_align(_room_risk_labels[i], LV_ALIGN_RIGHT_MID, 0, 0);
        _room_csi_labels[i] = nullptr;
    }

    lv_obj_t *automation = createPanel(left, left_w, auto_h, HUB_PANEL_COLOR);
    lv_obj_align_to(automation, rooms, LV_ALIGN_OUT_BOTTOM_LEFT, 0, gap);
    createSectionHeader(automation, "自动化", "今日", left_w - 24, 30);
    const char *schedule_times[] = {"18:30", "21:15", "23:40"};
    const char *schedule_titles[] = {"回家场景", "影音模式", "夜间布防"};
    const char *schedule_desc[] = {"玄关灯、热水、空调", "投影、遮光帘、氛围灯", "门锁复核、周界检测"};
    for (int i = 0; i < 3; ++i) {
        lv_obj_t *event = lv_obj_create(automation);
        lv_obj_set_size(event, left_w - 24, 28);
        lv_obj_align(event, LV_ALIGN_TOP_LEFT, 0, 35 + i * 30);
        styleFlatContainer(event);
        _event_cards[i] = event;
        _event_time_labels[i] = createLabel(event, schedule_times[i], HUB_FONT_SMALL, HUB_CYAN_COLOR);
        lv_obj_align(_event_time_labels[i], LV_ALIGN_LEFT_MID, 0, 0);
        _event_level_labels[i] = createLabel(event, schedule_titles[i], HUB_FONT_SMALL, HUB_TEXT_COLOR);
        lv_obj_align(_event_level_labels[i], LV_ALIGN_TOP_LEFT, 48, -1);
        _event_text_labels[i] = createLabel(event, schedule_desc[i], HUB_FONT_SMALL, HUB_MUTED_COLOR);
        lv_obj_set_width(_event_text_labels[i], left_w - 76);
        lv_obj_align(_event_text_labels[i], LV_ALIGN_BOTTOM_LEFT, 48, 1);
    }
    _event_cards[3] = nullptr;
    _event_dot_labels.fill(nullptr);

    lv_obj_t *hero_top = lv_obj_create(center);
    lv_obj_set_size(hero_top, center_w - 24, 34);
    lv_obj_align(hero_top, LV_ALIGN_TOP_LEFT, 0, 0);
    styleFlatContainer(hero_top);
    _hero_kicker_label = createLabel(hero_top, "LIVE · 客厅", HUB_FONT_SMALL, HUB_CYAN_COLOR);
    lv_obj_align(_hero_kicker_label, LV_ALIGN_LEFT_MID, 0, 0);
    _security_chip_label = createLabel(hero_top, "在家模式", HUB_FONT_SMALL, HUB_GREEN_COLOR);
    lv_obj_set_style_bg_color(_security_chip_label, HUB_GREEN_COLOR, 0);
    lv_obj_set_style_bg_opa(_security_chip_label, LV_OPA_20, 0);
    lv_obj_set_style_border_width(_security_chip_label, 1, 0);
    lv_obj_set_style_border_color(_security_chip_label, HUB_GREEN_COLOR, 0);
    lv_obj_set_style_radius(_security_chip_label, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_left(_security_chip_label, 10, 0);
    lv_obj_set_style_pad_right(_security_chip_label, 10, 0);
    lv_obj_set_style_pad_top(_security_chip_label, 3, 0);
    lv_obj_set_style_pad_bottom(_security_chip_label, 3, 0);
    lv_obj_align(_security_chip_label, LV_ALIGN_RIGHT_MID, 0, 0);

    lv_obj_t *hero_main = lv_obj_create(center);
    lv_obj_set_size(hero_main, center_w - 24, content_h - 174);
    lv_obj_align(hero_main, LV_ALIGN_TOP_LEFT, 0, 42);
    styleFlatContainer(hero_main);

    _hero_room_label = createLabel(hero_main, "客厅主控", HUB_FONT_TITLE, HUB_TEXT_COLOR);
    lv_obj_align(_hero_room_label, LV_ALIGN_TOP_LEFT, 0, 4);
    _hero_text_label = createLabel(hero_main, "灯光、温度、影音和安防已同步到当前场景。南向窗帘保持 72%，主灯为暖白。", HUB_FONT_BODY, HUB_MUTED_COLOR);
    lv_obj_set_width(_hero_text_label, center_w - 216);
    lv_obj_align(_hero_text_label, LV_ALIGN_TOP_LEFT, 0, 46);

    lv_obj_t *comfort = createPanel(hero_main, 188, lv_obj_get_height(hero_main) - 4, HUB_PANEL_SOLID_COLOR);
    lv_obj_align(comfort, LV_ALIGN_TOP_RIGHT, 0, 2);
    _comfort_temp_label = createLabel(comfort, "25°", HUB_FONT_TITLE, HUB_TEXT_COLOR);
    lv_obj_align(_comfort_temp_label, LV_ALIGN_TOP_LEFT, 0, 2);
    _comfort_status_label = createLabel(comfort, "自动恒温 · 新风低速", HUB_FONT_SMALL, HUB_MUTED_COLOR);
    lv_obj_align(_comfort_status_label, LV_ALIGN_TOP_LEFT, 0, 48);
    lv_obj_t *temp_arc = lv_arc_create(comfort);
    _temp_arc = temp_arc;
    lv_obj_set_size(temp_arc, 110, 110);
    lv_obj_align(temp_arc, LV_ALIGN_TOP_MID, 0, 74);
    lv_arc_set_range(temp_arc, 18, 30);
    lv_arc_set_value(temp_arc, 25);
    lv_obj_set_style_arc_color(temp_arc, lv_color_hex(0x283132), LV_PART_MAIN);
    lv_obj_set_style_arc_color(temp_arc, HUB_CYAN_COLOR, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(temp_arc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(temp_arc, 10, LV_PART_INDICATOR);

    lv_obj_t *metric_row = lv_obj_create(comfort);
    lv_obj_set_size(metric_row, 164, 54);
    lv_obj_align(metric_row, LV_ALIGN_BOTTOM_MID, 0, 0);
    styleFlatContainer(metric_row);
    lv_obj_set_style_pad_column(metric_row, 5, 0);
    lv_obj_set_flex_flow(metric_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(metric_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    const char *comfort_names[] = {"灯光", "空气", "功耗"};
    lv_obj_t **comfort_values[] = {&_light_label, &_air_label, &_power_label};
    for (int i = 0; i < 3; ++i) {
        lv_obj_t *metric = createPanel(metric_row, 50, 54, lv_color_hex(0x101515));
        lv_obj_set_style_shadow_width(metric, 0, 0);
        lv_obj_set_style_pad_all(metric, 5, 0);
        createLabel(metric, comfort_names[i], HUB_FONT_SMALL, HUB_MUTED_COLOR);
        *comfort_values[i] = createLabel(metric, i == 0 ? "68%" : (i == 1 ? "优" : "1.8kW"), HUB_FONT_SMALL, HUB_TEXT_COLOR);
        lv_obj_align(*comfort_values[i], LV_ALIGN_BOTTOM_MID, 0, 0);
    }

    lv_obj_t *scene_dock = lv_obj_create(center);
    lv_obj_set_size(scene_dock, center_w - 24, 96);
    lv_obj_align(scene_dock, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    styleFlatContainer(scene_dock);
    lv_obj_set_style_pad_column(scene_dock, 8, 0);
    lv_obj_set_flex_flow(scene_dock, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(scene_dock, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    const char *scene_names[] = {"晨起", "会客", "观影", "睡眠"};
    const char *scene_desc[] = {"窗帘 · 音乐", "主灯 · 空调", "投影 · 遮光", "静音 · 布防"};
    const char *scene_icons[] = {LV_SYMBOL_UP, LV_SYMBOL_HOME, LV_SYMBOL_PLAY, LV_SYMBOL_EYE_CLOSE};
    const int scene_w = (center_w - 24 - 8 * 3) / 4;
    for (int i = 0; i < MODE_MAX; ++i) {
        lv_obj_t *scene = lv_btn_create(scene_dock);
        lv_obj_set_size(scene, scene_w, 88);
        lv_obj_set_style_bg_color(scene, i == 0 ? HUB_CYAN_COLOR : HUB_PANEL_SOLID_COLOR, 0);
        lv_obj_set_style_border_width(scene, 1, 0);
        lv_obj_set_style_border_color(scene, i == 0 ? HUB_CYAN_COLOR : HUB_LINE_COLOR, 0);
        lv_obj_set_style_radius(scene, 8, 0);
        lv_obj_set_style_shadow_width(scene, 0, 0);
        lv_obj_add_event_cb(scene, scenarioEventCb, LV_EVENT_CLICKED, this);
        lv_obj_set_user_data(scene, reinterpret_cast<void *>(static_cast<intptr_t>(i)));
        _scene_cards[i] = scene;
        lv_obj_t *scene_icon = createIconLabel(scene, scene_icons[i], i == 0 ? HUB_BG_COLOR : HUB_CYAN_COLOR);
        lv_obj_align(scene_icon, LV_ALIGN_TOP_MID, 0, 8);
        _scene_name_labels[i] = createLabel(scene, scene_names[i], HUB_FONT_SMALL, i == 0 ? HUB_BG_COLOR : HUB_TEXT_COLOR);
        lv_obj_align(_scene_name_labels[i], LV_ALIGN_CENTER, 0, 5);
        _scene_desc_labels[i] = createLabel(scene, scene_desc[i], HUB_FONT_SMALL, i == 0 ? HUB_BG_COLOR : HUB_MUTED_COLOR);
        lv_obj_set_width(_scene_desc_labels[i], scene_w - 8);
        lv_obj_align(_scene_desc_labels[i], LV_ALIGN_BOTTOM_MID, 0, -6);
    }

    const int devices_h = 158;
    const int security_h = 108;
    const int energy_h = 96;
    const int assistant_h = content_h - devices_h - security_h - energy_h - gap * 3;

    lv_obj_t *devices = createPanel(right, right_w, devices_h, HUB_PANEL_COLOR);
    lv_obj_align(devices, LV_ALIGN_TOP_LEFT, 0, 0);
    createSectionHeader(devices, "常用设备", "18/21", right_w - 24, 28);
    const char *device_names[] = {"客厅主灯", "中央空调", "南向窗帘", "客厅音响"};
    const char *device_states[] = {"亮度 68% · 暖白", "制冷 · 低风速", "开启 72%", "爵士电台 · 音量 32"};
    const char *device_icons[] = {LV_SYMBOL_HOME, LV_SYMBOL_SETTINGS, LV_SYMBOL_UP, LV_SYMBOL_AUDIO};
    for (int i = 0; i < 4; ++i) {
        lv_obj_t *device = lv_obj_create(devices);
        lv_obj_set_size(device, right_w - 24, 28);
        lv_obj_align(device, LV_ALIGN_TOP_LEFT, 0, 32 + i * 30);
        styleFlatContainer(device);
        _device_cards[i] = device;
        lv_obj_t *icon_box = lv_obj_create(device);
        lv_obj_set_size(icon_box, 24, 24);
        lv_obj_align(icon_box, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_set_style_bg_color(icon_box, lv_color_hex(0x1B2525), 0);
        lv_obj_set_style_border_width(icon_box, 1, 0);
        lv_obj_set_style_border_color(icon_box, HUB_LINE_COLOR, 0);
        lv_obj_set_style_radius(icon_box, 8, 0);
        lv_obj_set_style_pad_all(icon_box, 0, 0);
        lv_obj_clear_flag(icon_box, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *device_icon = createIconLabel(icon_box, device_icons[i], HUB_CYAN_COLOR);
        lv_obj_center(device_icon);
        lv_obj_t *name = createLabel(device, device_names[i], HUB_FONT_SMALL, HUB_TEXT_COLOR);
        lv_obj_align(name, LV_ALIGN_TOP_LEFT, 34, -2);
        _device_state_labels[i] = createLabel(device, device_states[i], HUB_FONT_SMALL, HUB_MUTED_COLOR);
        lv_obj_set_width(_device_state_labels[i], right_w - 90);
        lv_obj_align(_device_state_labels[i], LV_ALIGN_BOTTOM_LEFT, 34, 2);
        lv_obj_t *sw = lv_btn_create(device);
        lv_obj_set_size(sw, 30, 16);
        lv_obj_align(sw, LV_ALIGN_RIGHT_MID, 0, 0);
        styleButton(sw, i == 3 ? HUB_FAINT_COLOR : HUB_CYAN_COLOR, i != 3);
        lv_obj_add_event_cb(sw, actionEventCb, LV_EVENT_CLICKED, this);
        lv_obj_set_user_data(sw, reinterpret_cast<void *>(static_cast<intptr_t>(10 + i)));
        _device_switches[i] = sw;
    }

    lv_obj_t *security = createPanel(right, right_w, security_h, HUB_PANEL_COLOR);
    lv_obj_align_to(security, devices, LV_ALIGN_OUT_BOTTOM_LEFT, 0, gap);
    createSectionHeader(security, "门厅安防", "实时", right_w - 24, 26);
    createCameraPreview(security, right_w - 24, 42);
    lv_obj_t *camera = lv_obj_get_child(security, lv_obj_get_child_cnt(security) - 1);
    lv_obj_align(camera, LV_ALIGN_TOP_LEFT, 0, 30);
    _camera_status_label = createLabel(security, "在家 · 周界正常", HUB_FONT_SMALL, HUB_GREEN_COLOR);
    lv_obj_align(_camera_status_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_t *security_modes = lv_obj_create(security);
    lv_obj_set_size(security_modes, 100, 22);
    lv_obj_align(security_modes, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    styleFlatContainer(security_modes);
    lv_obj_set_style_pad_column(security_modes, 4, 0);
    lv_obj_set_flex_flow(security_modes, LV_FLEX_FLOW_ROW);
    const char *mode_texts[] = {"在家", "离家", "夜间"};
    for (int i = 0; i < 3; ++i) {
        lv_obj_t *mode = lv_btn_create(security_modes);
        lv_obj_set_size(mode, 31, 20);
        styleButton(mode, i == 0 ? HUB_GREEN_COLOR : HUB_BLUE_COLOR, i == 0);
        lv_obj_t *label = createLabel(mode, mode_texts[i], HUB_FONT_SMALL, i == 0 ? HUB_BG_COLOR : HUB_TEXT_COLOR);
        lv_obj_center(label);
    }

    lv_obj_t *energy = createPanel(right, right_w, energy_h, HUB_PANEL_COLOR);
    lv_obj_align_to(energy, security, LV_ALIGN_OUT_BOTTOM_LEFT, 0, gap);
    createSectionHeader(energy, "能源", "12.8 kWh", right_w - 24, 26);
    const char *energy_names[] = {"空调", "厨电", "照明", "影音"};
    const int energy_values[] = {72, 46, 34, 28};
    const char *energy_text[] = {"4.8", "2.1", "0.8", "0.6"};
    const lv_color_t energy_colors[] = {HUB_CYAN_COLOR, HUB_AMBER_COLOR, HUB_CYAN_COLOR, HUB_CORAL_COLOR};
    for (int i = 0; i < 4; ++i) {
        lv_obj_t *name = createLabel(energy, energy_names[i], HUB_FONT_SMALL, HUB_MUTED_COLOR);
        lv_obj_align(name, LV_ALIGN_TOP_LEFT, 0, 30 + i * 15);
        lv_obj_t *bar = lv_bar_create(energy);
        lv_obj_set_size(bar, right_w - 92, 7);
        lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 42, 35 + i * 15);
        lv_bar_set_range(bar, 0, 100);
        lv_bar_set_value(bar, energy_values[i], LV_ANIM_OFF);
        styleBar(bar, energy_colors[i]);
        _energy_bars[i] = bar;
        _energy_value_labels[i] = createLabel(energy, energy_text[i], HUB_FONT_SMALL, HUB_TEXT_COLOR);
        lv_obj_align(_energy_value_labels[i], LV_ALIGN_TOP_RIGHT, 0, 28 + i * 15);
    }

    lv_obj_t *assistant = createPanel(right, right_w, assistant_h, HUB_PANEL_COLOR);
    lv_obj_align_to(assistant, energy, LV_ALIGN_OUT_BOTTOM_LEFT, 0, gap);
    createSectionHeader(assistant, "语音助手", "待命", right_w - 24, 24);
    _assistant_text_label = createLabel(assistant, "把客厅切到观影模式", HUB_FONT_SMALL, HUB_MUTED_COLOR);
    lv_obj_set_width(_assistant_text_label, right_w - 74);
    lv_obj_align(_assistant_text_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_t *mic = lv_btn_create(assistant);
    lv_obj_set_size(mic, 32, 32);
    lv_obj_align(mic, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    styleButton(mic, HUB_CYAN_COLOR, true);
    lv_obj_add_event_cb(mic, actionEventCb, LV_EVENT_CLICKED, this);
    lv_obj_set_user_data(mic, reinterpret_cast<void *>(static_cast<intptr_t>(3)));
    lv_obj_t *mic_icon = createIconLabel(mic, LV_SYMBOL_AUDIO, HUB_BG_COLOR);
    lv_obj_center(mic_icon);

    _outdoor_label = _top_weather_label;
    _humidity_label = _weather_summary_label;
    _indoor_label = _comfort_status_label;
    _night_label = _assistant_text_label;
    return true;
}

void HomeCareHub::setMode(DemoMode mode)
{
    _mode = mode;
    updateUi();
}

homecare_mqtt_mode_t HomeCareHub::toMqttMode(DemoMode mode) const
{
    switch (mode) {
    case MODE_NORMAL:
        return HOMECARE_MQTT_MODE_NORMAL;
    case MODE_FALL:
        return HOMECARE_MQTT_MODE_FALL;
    case MODE_BATHROOM:
        return HOMECARE_MQTT_MODE_BATHROOM;
    case MODE_NIGHT:
        return HOMECARE_MQTT_MODE_NIGHT;
    default:
        return HOMECARE_MQTT_MODE_UNKNOWN;
    }
}

HomeCareHub::DemoMode HomeCareHub::fromMqttMode(homecare_mqtt_mode_t mode) const
{
    switch (mode) {
    case HOMECARE_MQTT_MODE_NORMAL:
        return MODE_NORMAL;
    case HOMECARE_MQTT_MODE_FALL:
        return MODE_FALL;
    case HOMECARE_MQTT_MODE_BATHROOM:
        return MODE_BATHROOM;
    case HOMECARE_MQTT_MODE_NIGHT:
        return MODE_NIGHT;
    default:
        return _mode;
    }
}

void HomeCareHub::applyMqttMessage(const HomeCareMqttInboundMessage &message)
{
    if (message.has_mode) {
        _mode = fromMqttMode(message.mode);
    }

    if (message.has_smartcar_attitude && message.smartcar_attitude.valid) {
        _has_smartcar_attitude = true;
        _smartcar_attitude = message.smartcar_attitude;
    }

    if (message.type == HOMECARE_MQTT_INBOUND_EVENT) {
        _has_mqtt_event = true;
        _mqtt_event = message.event;
        if (_mqtt_event.level[0] == '\0') {
            std::snprintf(_mqtt_event.level, sizeof(_mqtt_event.level), "L1");
        }
        if (_mqtt_event.time[0] == '\0') {
            std::snprintf(_mqtt_event.time, sizeof(_mqtt_event.time), "MQTT");
        }
        if (_mqtt_event.text[0] == '\0') {
            std::snprintf(_mqtt_event.text, sizeof(_mqtt_event.text), "MQTT message received");
        }

        if (std::strcmp(_mqtt_event.level, "L3") == 0) {
            _mqtt_event_color = HUB_CORAL_COLOR;
        } else if (std::strcmp(_mqtt_event.level, "L2") == 0) {
            _mqtt_event_color = HUB_AMBER_COLOR;
        } else if (std::strcmp(_mqtt_event.level, "L1") == 0) {
            _mqtt_event_color = HUB_BLUE_COLOR;
        } else {
            _mqtt_event_color = HUB_GREEN_COLOR;
        }
    }

    updateUi();
}

void HomeCareHub::applyMqttMessages(void)
{
    HomeCareMqttInboundMessage message = {};
    while (homecare_mqtt_bridge_receive(&message)) {
        applyMqttMessage(message);
    }
}

void HomeCareHub::updateUi(void)
{
    const DashboardState states[MODE_MAX] = {
        {
            "全屋安全", "WiFi-CSI 在线 | 本地 AI 就绪", "在家模式",
            "多云", "室外 24°C", "湿度 58%", "空气 优", "客厅舒适 · 湿度 42%", "系统舒适运行",
            "待命中", "充电桩", "客厅巡检点", "CSI 看护", "障碍：通畅", "传感器：CSI 在线", "语音：待命",
            86, 12,
            {{
                {"客厅", "主灯 68% · 空调 25°", "舒适", 31, true, false, HUB_CYAN_COLOR},
                {"主卧", "窗帘关闭 · 静音", "睡眠", 18, false, true, HUB_GREEN_COLOR},
                {"厨房", "排风运行 · 烟感正常", "运行", 14, false, true, HUB_AMBER_COLOR},
                {"书房", "台灯 42% · 空气优", "安静", 20, false, false, HUB_BLUE_COLOR},
            }},
            {{
                {"回家场景", "18:30", "玄关灯、热水、空调", HUB_CYAN_COLOR},
                {"影音模式", "21:15", "投影、遮光帘、氛围灯", HUB_BLUE_COLOR},
                {"夜间布防", "23:40", "门锁复核、周界检测", HUB_GREEN_COLOR},
                {"L0", "07:20", "小车已回充", HUB_GREEN_COLOR},
            }},
        },
        {
            "异常复核", "CSI 高动态 | 小车出发", "离家模式",
            "多云", "室外 24°C", "湿度 58%", "空气 优", "客厅波动 · 湿度 42%", "优先确认客厅",
            "前往现场", "走廊", "客厅巡检点", "CSI 复核", "障碍：已绕行", "传感器：CSI 增强", "语音：询问用户",
            78, 74,
            {{
                {"客厅", "高动态后静止", "复核", 88, true, false, HUB_CORAL_COLOR},
                {"主卧", "无人 · 静音", "正常", 12, false, true, HUB_GREEN_COLOR},
                {"厨房", "排风低速", "运行", 10, false, true, HUB_AMBER_COLOR},
                {"书房", "无人 · 台灯关", "安静", 45, true, false, HUB_BLUE_COLOR},
            }},
            {{
                {"跌倒复核", "22:31", "CSI 触发高动态", HUB_CORAL_COLOR},
                {"小车出发", "22:30", "前往客厅巡检点", HUB_AMBER_COLOR},
                {"语音询问", "22:30", "等待用户回应", HUB_BLUE_COLOR},
                {"夜间待命", "22:15", "系统进入布防", HUB_GREEN_COLOR},
            }},
        },
        {
            "浴室看护", "CSI 久留计时 | 门区保护", "在家模式",
            "小雨", "室外 22°C", "湿度 76%", "空气 良好", "浴室偏湿 · 湿度 55%", "超时前先观察",
            "门外等待", "浴室门口", "浴室门口", "语音询问", "障碍：通畅", "传感器：门区 CSI", "语音：轻声询问",
            72, 100,
            {{
                {"客厅", "主灯 42% · 空调 24°", "舒适", 15, false, false, HUB_CYAN_COLOR},
                {"主卧", "无人 · 窗帘关", "正常", 12, false, true, HUB_GREEN_COLOR},
                {"厨房", "无人 · 烟感正常", "安全", 10, false, true, HUB_GREEN_COLOR},
                {"浴室", "久留超时", "看护", 79, true, true, HUB_AMBER_COLOR},
            }},
            {{
                {"浴室久留", "21:08", "超过设定观察时间", HUB_AMBER_COLOR},
                {"小车等待", "21:07", "停靠浴室门外", HUB_BLUE_COLOR},
                {"隐私保护", "21:02", "无图像采集", HUB_GREEN_COLOR},
                {"语音准备", "20:55", "轻声询问模板就绪", HUB_BLUE_COLOR},
            }},
        },
        {
            "夜间守护", "CSI 离床 | 低速跟随", "夜间模式",
            "晴夜", "室外 19°C", "湿度 62%", "空气 优秀", "卧室安静 · 湿度 49%", "引导灯并看护走廊",
            "低速跟随", "主卧门口", "走廊点位", "夜间陪护", "障碍：通畅", "传感器：CSI 在线", "语音：待命",
            81, 46,
            {{
                {"客厅", "无人 · 主灯关", "安静", 18, false, false, HUB_GREEN_COLOR},
                {"主卧", "离床活动", "看护", 64, true, true, HUB_BLUE_COLOR},
                {"厨房", "无人 · 电器关", "安全", 16, false, true, HUB_GREEN_COLOR},
                {"书房", "无人 · 夜灯关", "安静", 56, true, false, HUB_GREEN_COLOR},
            }},
            {{
                {"离床发现", "02:14", "主卧 CSI 变化", HUB_BLUE_COLOR},
                {"夜灯联动", "02:14", "走廊灯低亮开启", HUB_CYAN_COLOR},
                {"卧室稳定", "02:10", "呼吸节律正常", HUB_GREEN_COLOR},
                {"系统看护", "01:40", "夜间静默巡检", HUB_GREEN_COLOR},
            }},
        },
    };

    const DashboardState &state = states[_mode];
    const lv_color_t mode_color = _mode == MODE_FALL ? HUB_CORAL_COLOR :
                                  (_mode == MODE_BATHROOM ? HUB_AMBER_COLOR :
                                   (_mode == MODE_NIGHT ? HUB_BLUE_COLOR : HUB_GREEN_COLOR));

    if (_time_label != nullptr) {
        std::time_t now = std::time(nullptr);
        std::tm tm_now = {};
        localtime_r(&now, &tm_now);
        char time_text[8] = {};
        char date_text[32] = {};
        std::snprintf(time_text, sizeof(time_text), "%02d:%02d", tm_now.tm_hour, tm_now.tm_min);
        std::snprintf(date_text, sizeof(date_text), "%d月%d日", tm_now.tm_mon + 1, tm_now.tm_mday);
        lv_label_set_text(_time_label, time_text);
        lv_label_set_text(_date_label, date_text);
    }

    lv_label_set_text(_mode_label, state.mode_name);
    lv_label_set_text(_status_label, state.home_status);
    lv_label_set_text(_privacy_label, _privacy_enabled ? state.privacy : "手动巡检模式");
    lv_obj_set_style_text_color(_mode_label, mode_color, 0);
    lv_obj_set_style_text_color(_privacy_label, _privacy_enabled ? HUB_GREEN_COLOR : HUB_AMBER_COLOR, 0);
    lv_label_set_text(_security_chip_label, _privacy_enabled ? state.privacy : "手动模式");
    lv_obj_set_style_text_color(_security_chip_label, _privacy_enabled ? HUB_GREEN_COLOR : HUB_AMBER_COLOR, 0);
    lv_obj_set_style_bg_color(_security_chip_label, _privacy_enabled ? HUB_GREEN_COLOR : HUB_AMBER_COLOR, 0);
    lv_obj_set_style_border_color(_security_chip_label, _privacy_enabled ? HUB_GREEN_COLOR : HUB_AMBER_COLOR, 0);

    lv_label_set_text(_weather_label, state.weather);
    lv_label_set_text(_weather_summary_label, state.indoor_env);
    lv_label_set_text(_outdoor_label, state.outdoor_temp);
    lv_label_set_text(_humidity_label, state.humidity);
    lv_label_set_text(_air_label, "优");
    lv_label_set_text(_indoor_label, state.indoor_env);
    lv_label_set_text(_night_label, state.night_hint);
    lv_label_set_text(_top_weather_label, state.outdoor_temp);

    HomeCareWeatherSnapshot weather_snapshot = {};
    if (homecare_weather_service_get_snapshot(&weather_snapshot)) {
        lv_label_set_text(_weather_city_label, weather_snapshot.city);
        lv_label_set_text(_weather_label, weather_snapshot.weather);
        lv_label_set_text(_outdoor_label, weather_snapshot.outdoor_temp);
        lv_label_set_text(_humidity_label, weather_snapshot.humidity);
        lv_label_set_text(_top_weather_label, weather_snapshot.outdoor_temp);
        lv_label_set_text(_weather_summary_label, weather_snapshot.humidity);
        lv_obj_set_style_text_color(_weather_label,
                                    weather_snapshot.has_live_data ? HUB_CYAN_COLOR : HUB_MUTED_COLOR, 0);
        lv_color_t air_color = HUB_MUTED_COLOR;
        if (weather_snapshot.air_quality_level == 1) {
            air_color = HUB_GREEN_COLOR;
        } else if (weather_snapshot.air_quality_level == 2) {
            air_color = HUB_CYAN_COLOR;
        } else if (weather_snapshot.air_quality_level == 3) {
            air_color = HUB_AMBER_COLOR;
        } else if (weather_snapshot.air_quality_level >= 4) {
            air_color = HUB_CORAL_COLOR;
        }
        lv_label_set_text(_air_label, weather_snapshot.air_quality);
        lv_obj_set_style_text_color(_air_label, air_color, 0);
        _weather_revision = weather_snapshot.revision;
    }

    lv_label_set_text(_hero_kicker_label, _mode == MODE_NORMAL ? "LIVE · 客厅" : state.car_status);
    lv_label_set_text(_hero_room_label, _mode == MODE_NORMAL ? "客厅主控" : state.mode_name);
    lv_label_set_text(_hero_text_label, state.night_hint);
    lv_label_set_text(_comfort_temp_label, _mode == MODE_NIGHT ? "23°" : (_mode == MODE_BATHROOM ? "24°" : "25°"));
    lv_label_set_text(_comfort_status_label, state.indoor_env);
    lv_arc_set_value(_temp_arc, _mode == MODE_NIGHT ? 23 : (_mode == MODE_BATHROOM ? 24 : 25));
    lv_label_set_text(_light_label, _mode == MODE_NIGHT ? "12%" : "68%");
    lv_label_set_text(_power_label, _mode == MODE_FALL ? "2.4kW" : "1.8kW");

    if (_has_smartcar_attitude && _smartcar_attitude.valid) {
        lv_label_set_text(_hero_kicker_label, "LIVE · 姿态回传");
        lv_label_set_text_fmt(_hero_text_label, "smartcar/attitude 在线：roll %.1f，pitch %.1f，yaw %.1f。",
                              _smartcar_attitude.roll_deg,
                              _smartcar_attitude.pitch_deg,
                              _smartcar_attitude.yaw_deg);
    }

    for (int i = 0; i < 4; ++i) {
        const RoomState &room = state.rooms[i];
        lv_label_set_text(_room_name_labels[i], room.name);
        lv_label_set_text(_room_activity_labels[i], room.activity);
        lv_label_set_text(_room_risk_labels[i], room.risk);
        lv_obj_set_style_text_color(_room_risk_labels[i], room.accent, 0);
        lv_obj_set_style_bg_color(_room_cards[i], i == 0 ? lv_color_hex(0x172324) : HUB_PANEL_SOLID_COLOR, 0);
        setCardAccent(_room_cards[i], _room_accent_bars[i], room.accent);
    }

    for (int i = 0; i < 4; ++i) {
        if (_scene_cards[i] == nullptr) {
            continue;
        }
        const bool active = (i == static_cast<int>(_mode));
        lv_obj_set_style_bg_color(_scene_cards[i], active ? HUB_CYAN_COLOR : HUB_PANEL_SOLID_COLOR, 0);
        lv_obj_set_style_border_color(_scene_cards[i], active ? HUB_CYAN_COLOR : HUB_LINE_COLOR, 0);
        lv_obj_set_style_text_color(_scene_name_labels[i], active ? HUB_BG_COLOR : HUB_TEXT_COLOR, 0);
        lv_obj_set_style_text_color(_scene_desc_labels[i], active ? HUB_BG_COLOR : HUB_MUTED_COLOR, 0);
    }

    for (int i = 0; i < 3; ++i) {
        const EventState &event = state.events[i];
        lv_label_set_text(_event_level_labels[i], event.level);
        lv_label_set_text(_event_time_labels[i], event.time);
        lv_label_set_text(_event_text_labels[i], event.text);
        lv_obj_set_style_text_color(_event_time_labels[i], event.accent, 0);
    }

    if (_has_mqtt_event && _event_level_labels[0] != nullptr) {
        lv_label_set_text(_event_level_labels[0], _mqtt_event.level);
        lv_label_set_text(_event_time_labels[0], _mqtt_event.time);
        lv_label_set_text(_event_text_labels[0], _mqtt_event.text);
        lv_obj_set_style_text_color(_event_time_labels[0], _mqtt_event_color, 0);
    }
}

void HomeCareHub::scenarioEventCb(lv_event_t *e)
{
    HomeCareHub *app = static_cast<HomeCareHub *>(lv_event_get_user_data(e));
    lv_obj_t *target = static_cast<lv_obj_t *>(lv_event_get_target(e));
    int mode = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(target)));
    if (app != nullptr && mode >= 0 && mode < MODE_MAX) {
        app->setMode(static_cast<DemoMode>(mode));
        homecare_mqtt_bridge_publish_mode(app->toMqttMode(static_cast<DemoMode>(mode)));
    }
}

void HomeCareHub::actionEventCb(lv_event_t *e)
{
    HomeCareHub *app = static_cast<HomeCareHub *>(lv_event_get_user_data(e));
    if (app == nullptr) {
        return;
    }
    lv_obj_t *btn = static_cast<lv_obj_t *>(lv_event_get_target(e));
    int action = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(btn)));

    if (action >= 10 && action < 14) {
        const int index = action - 10;
        if (app->_device_switches[index] != nullptr) {
            const bool now_off = lv_obj_has_state(app->_device_switches[index], LV_STATE_CHECKED);
            if (now_off) {
                lv_obj_clear_state(app->_device_switches[index], LV_STATE_CHECKED);
                lv_obj_set_style_bg_color(app->_device_switches[index], HUB_CYAN_COLOR, 0);
            } else {
                lv_obj_add_state(app->_device_switches[index], LV_STATE_CHECKED);
                lv_obj_set_style_bg_color(app->_device_switches[index], HUB_FAINT_COLOR, 0);
            }
        }
        return;
    }

    if (action == 0) {
        homecare_mqtt_bridge_publish_action(HOMECARE_MQTT_ACTION_PATROL);
        app->setMode(MODE_NORMAL);
    } else if (action == 1) {
        homecare_mqtt_bridge_publish_action(HOMECARE_MQTT_ACTION_RECHARGE);
        app->setMode(MODE_NORMAL);
    } else if (action == 3) {
        homecare_mqtt_bridge_publish_action(HOMECARE_MQTT_ACTION_CALL_FAMILY);
        if (app->_assistant_text_label != nullptr) {
            lv_label_set_text(app->_assistant_text_label, "正在呼叫家人");
        }
    } else {
        homecare_mqtt_bridge_publish_action(HOMECARE_MQTT_ACTION_PRIVACY_TOGGLE);
        app->_privacy_enabled = !app->_privacy_enabled;
        app->updateUi();
    }
}

void HomeCareHub::scrollEventCb(lv_event_t *e)
{
    HomeCareHub *app = static_cast<HomeCareHub *>(lv_event_get_user_data(e));
    if (app == nullptr || app->_pages == nullptr) {
        return;
    }

    const int page_w = app->_width - 24;
    const int stride = page_w + HUB_PAGE_GAP;
    if (stride <= 0) {
        return;
    }

    int page = (lv_obj_get_scroll_x(app->_pages) + stride / 2) / stride;
    app->updatePageIndicator(page);
}

void HomeCareHub::timerCb(lv_timer_t *timer)
{
    HomeCareHub *app = static_cast<HomeCareHub *>(timer->user_data);
    if (app == nullptr) {
        return;
    }
    if (timer == app->_mqtt_timer) {
        app->applyMqttMessages();
        HomeCareWeatherSnapshot weather_snapshot = {};
        if (homecare_weather_service_get_snapshot(&weather_snapshot) &&
            weather_snapshot.revision != app->_weather_revision) {
            app->updateUi();
        }
        return;
    }

    int next = (static_cast<int>(app->_mode) + 1) % MODE_MAX;
    app->setMode(static_cast<DemoMode>(next));
}
