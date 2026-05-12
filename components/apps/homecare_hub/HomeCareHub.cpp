#include <cstdio>
#include <cstdint>
#include "HomeCareHub.hpp"

LV_IMG_DECLARE(img_app_setting);
LV_FONT_DECLARE(homecare_font_simsun_14);
LV_FONT_DECLARE(homecare_font_simsun_16);
LV_FONT_DECLARE(homecare_font_simsun_20);
LV_FONT_DECLARE(homecare_font_simsun_28);

#define HUB_BG_COLOR           lv_color_hex(0x101722)
#define HUB_PANEL_COLOR        lv_color_hex(0x182231)
#define HUB_PANEL_2_COLOR      lv_color_hex(0x202C3B)
#define HUB_TEXT_COLOR         lv_color_hex(0xF5F7FB)
#define HUB_MUTED_COLOR        lv_color_hex(0x9AA8BA)
#define HUB_BLUE_COLOR         lv_color_hex(0x4CA3FF)
#define HUB_GREEN_COLOR        lv_color_hex(0x34D399)
#define HUB_YELLOW_COLOR       lv_color_hex(0xFBBF24)
#define HUB_ORANGE_COLOR       lv_color_hex(0xF97316)
#define HUB_RED_COLOR          lv_color_hex(0xEF4444)
#define HUB_PURPLE_COLOR       lv_color_hex(0xA78BFA)

#define HUB_FONT_TITLE         (&homecare_font_simsun_28)
#define HUB_FONT_HEAD          (&homecare_font_simsun_20)
#define HUB_FONT_BODY          (&homecare_font_simsun_16)
#define HUB_FONT_SMALL         (&homecare_font_simsun_14)

HomeCareHub::HomeCareHub():
    ESP_Brookesia_PhoneApp("家庭终端", &img_app_setting, true),
    _mode(MODE_NORMAL),
    _timer(nullptr),
    _width(1024),
    _height(600),
    _privacy_enabled(true),
    _root(nullptr),
    _mode_label(nullptr),
    _status_label(nullptr),
    _privacy_label(nullptr),
    _weather_label(nullptr),
    _outdoor_label(nullptr),
    _humidity_label(nullptr),
    _air_label(nullptr),
    _indoor_label(nullptr),
    _night_label(nullptr),
    _car_status_label(nullptr),
    _car_position_label(nullptr),
    _car_target_label(nullptr),
    _car_phase_label(nullptr),
    _obstacle_label(nullptr),
    _camera_label(nullptr),
    _voice_label(nullptr),
    _battery_bar(nullptr),
    _battery_label(nullptr),
    _route_bar(nullptr),
    _route_label(nullptr),
    _room_cards({}),
    _room_name_labels({}),
    _room_activity_labels({}),
    _room_risk_labels({}),
    _room_csi_labels({}),
    _event_cards({}),
    _event_level_labels({}),
    _event_time_labels({}),
    _event_text_labels({})
{
}

HomeCareHub::~HomeCareHub()
{
    deleteTimer();
}

bool HomeCareHub::init(void)
{
    return true;
}

bool HomeCareHub::run(void)
{
    lv_area_t area = getVisualArea();
    _width = area.x2 > area.x1 ? area.x2 - area.x1 : 1024;
    _height = area.y2 > area.y1 ? area.y2 - area.y1 : 600;
    if (_width < 800) {
        _width = 1024;
    }
    if (_height < 480) {
        _height = 600;
    }

    createUi();
    setMode(MODE_NORMAL);
    _timer = lv_timer_create(timerCb, 6000, this);
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
    return true;
}

void HomeCareHub::deleteTimer(void)
{
    if (_timer != nullptr) {
        lv_timer_del(_timer);
        _timer = nullptr;
    }
}

lv_obj_t *HomeCareHub::createPanel(lv_obj_t *parent, int32_t width, int32_t height, lv_color_t bg)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, width, height);
    lv_obj_set_style_bg_color(panel, bg, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0x2D3A4C), 0);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_set_style_pad_all(panel, 12, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    return panel;
}

lv_obj_t *HomeCareHub::createLabel(lv_obj_t *parent, const char *text, const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    return label;
}

void HomeCareHub::setCardAccent(lv_obj_t *obj, lv_color_t color)
{
    lv_obj_set_style_border_width(obj, 2, 0);
    lv_obj_set_style_border_color(obj, color, 0);
}

bool HomeCareHub::createUi(void)
{
    _root = lv_obj_create(lv_scr_act());
    lv_obj_set_size(_root, _width, _height);
    lv_obj_align(_root, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(_root, HUB_BG_COLOR, 0);
    lv_obj_set_style_border_width(_root, 0, 0);
    lv_obj_set_style_radius(_root, 0, 0);
    lv_obj_set_style_pad_all(_root, 14, 0);
    lv_obj_clear_flag(_root, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *top = lv_obj_create(_root);
    lv_obj_set_size(top, _width - 28, 58);
    lv_obj_align(top, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(top, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(top, 0, 0);
    lv_obj_set_style_pad_all(top, 0, 0);
    lv_obj_clear_flag(top, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = createLabel(top, "家庭终端", HUB_FONT_TITLE, HUB_TEXT_COLOR);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, -8);
    lv_obj_t *subtitle = createLabel(top, "家庭安全看板", HUB_FONT_SMALL, HUB_MUTED_COLOR);
    lv_obj_align_to(subtitle, title, LV_ALIGN_OUT_BOTTOM_LEFT, 2, 2);

    _mode_label = createLabel(top, "正常巡检", HUB_FONT_HEAD, HUB_GREEN_COLOR);
    lv_obj_align(_mode_label, LV_ALIGN_RIGHT_MID, -360, -8);
    _status_label = createLabel(top, "WiFi-CSI 在线 | 本地 AI 就绪", HUB_FONT_SMALL, HUB_MUTED_COLOR);
    lv_obj_align_to(_status_label, _mode_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 4);
    _privacy_label = createLabel(top, "隐私：摄像头关闭", HUB_FONT_BODY, HUB_GREEN_COLOR);
    lv_obj_align(_privacy_label, LV_ALIGN_RIGHT_MID, 0, 0);

    const int content_y = 72;
    const int content_h = _height - 190;
    const int left_w = 300;
    const int center_w = 390;
    const int right_w = _width - left_w - center_w - 70;

    lv_obj_t *left = createPanel(_root, left_w, content_h, HUB_PANEL_COLOR);
    lv_obj_align(left, LV_ALIGN_TOP_LEFT, 0, content_y);
    createLabel(left, "居家区域", HUB_FONT_HEAD, HUB_TEXT_COLOR);

    const char *room_names[] = {"客厅", "卧室", "浴室", "走廊"};
    for (int i = 0; i < 4; ++i) {
        lv_obj_t *card = createPanel(left, left_w - 24, 72, HUB_PANEL_2_COLOR);
        lv_obj_align(card, LV_ALIGN_TOP_LEFT, 0, 38 + i * 82);
        _room_cards[i] = card;
        _room_name_labels[i] = createLabel(card, room_names[i], HUB_FONT_BODY, HUB_TEXT_COLOR);
        lv_obj_align(_room_name_labels[i], LV_ALIGN_TOP_LEFT, 0, 0);
        _room_activity_labels[i] = createLabel(card, "空闲", HUB_FONT_SMALL, HUB_MUTED_COLOR);
        lv_obj_align(_room_activity_labels[i], LV_ALIGN_TOP_LEFT, 0, 26);
        _room_risk_labels[i] = createLabel(card, "风险 L0", HUB_FONT_SMALL, HUB_GREEN_COLOR);
        lv_obj_align(_room_risk_labels[i], LV_ALIGN_TOP_RIGHT, 0, 0);
        _room_csi_labels[i] = createLabel(card, "CSI 20%", HUB_FONT_SMALL, HUB_MUTED_COLOR);
        lv_obj_align(_room_csi_labels[i], LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    }

    lv_obj_t *center = createPanel(_root, center_w, content_h, HUB_PANEL_COLOR);
    lv_obj_align_to(center, left, LV_ALIGN_OUT_RIGHT_TOP, 14, 0);
    createLabel(center, "巡检小车", HUB_FONT_HEAD, HUB_TEXT_COLOR);

    _car_status_label = createLabel(center, "待命中", HUB_FONT_TITLE, HUB_GREEN_COLOR);
    lv_obj_align(_car_status_label, LV_ALIGN_TOP_LEFT, 0, 42);
    _car_position_label = createLabel(center, "位置：充电桩", HUB_FONT_BODY, HUB_MUTED_COLOR);
    lv_obj_align(_car_position_label, LV_ALIGN_TOP_LEFT, 0, 86);
    _car_target_label = createLabel(center, "目标：客厅巡检点", HUB_FONT_BODY, HUB_MUTED_COLOR);
    lv_obj_align(_car_target_label, LV_ALIGN_TOP_LEFT, 0, 114);
    _car_phase_label = createLabel(center, "阶段：CSI 看护", HUB_FONT_BODY, HUB_MUTED_COLOR);
    lv_obj_align(_car_phase_label, LV_ALIGN_TOP_LEFT, 0, 142);

    lv_obj_t *battery_title = createLabel(center, "电量", HUB_FONT_SMALL, HUB_MUTED_COLOR);
    lv_obj_align(battery_title, LV_ALIGN_TOP_LEFT, 0, 176);
    _battery_bar = lv_bar_create(center);
    lv_obj_set_size(_battery_bar, center_w - 110, 14);
    lv_obj_align(_battery_bar, LV_ALIGN_TOP_LEFT, 0, 198);
    lv_bar_set_range(_battery_bar, 0, 100);
    lv_obj_set_style_bg_color(_battery_bar, lv_color_hex(0x334155), LV_PART_MAIN);
    lv_obj_set_style_bg_color(_battery_bar, HUB_GREEN_COLOR, LV_PART_INDICATOR);
    _battery_label = createLabel(center, "86%", HUB_FONT_SMALL, HUB_TEXT_COLOR);
    lv_obj_align(_battery_label, LV_ALIGN_TOP_RIGHT, 0, 190);

    lv_obj_t *route_title = createLabel(center, "路线", HUB_FONT_SMALL, HUB_MUTED_COLOR);
    lv_obj_align(route_title, LV_ALIGN_TOP_LEFT, 0, 228);
    _route_bar = lv_bar_create(center);
    lv_obj_set_size(_route_bar, center_w - 110, 14);
    lv_obj_align(_route_bar, LV_ALIGN_TOP_LEFT, 0, 250);
    lv_bar_set_range(_route_bar, 0, 100);
    lv_obj_set_style_bg_color(_route_bar, lv_color_hex(0x334155), LV_PART_MAIN);
    lv_obj_set_style_bg_color(_route_bar, HUB_BLUE_COLOR, LV_PART_INDICATOR);
    _route_label = createLabel(center, "充电桩 > 走廊 > 点位", HUB_FONT_SMALL, HUB_MUTED_COLOR);
    lv_obj_align(_route_label, LV_ALIGN_TOP_LEFT, 0, 272);

    _obstacle_label = createLabel(center, "障碍：通畅", HUB_FONT_BODY, HUB_TEXT_COLOR);
    lv_obj_align(_obstacle_label, LV_ALIGN_TOP_LEFT, 0, 314);
    _camera_label = createLabel(center, "摄像头：关闭", HUB_FONT_BODY, HUB_TEXT_COLOR);
    lv_obj_align(_camera_label, LV_ALIGN_TOP_LEFT, 0, 344);
    _voice_label = createLabel(center, "语音：待命", HUB_FONT_BODY, HUB_TEXT_COLOR);
    lv_obj_align(_voice_label, LV_ALIGN_TOP_LEFT, 0, 374);

    lv_obj_t *right = createPanel(_root, right_w, content_h, HUB_PANEL_COLOR);
    lv_obj_align_to(right, center, LV_ALIGN_OUT_RIGHT_TOP, 14, 0);
    createLabel(right, "天气", HUB_FONT_HEAD, HUB_TEXT_COLOR);
    _weather_label = createLabel(right, "多云", HUB_FONT_TITLE, HUB_BLUE_COLOR);
    lv_obj_align(_weather_label, LV_ALIGN_TOP_LEFT, 0, 42);
    _outdoor_label = createLabel(right, "室外 24C", HUB_FONT_BODY, HUB_TEXT_COLOR);
    lv_obj_align(_outdoor_label, LV_ALIGN_TOP_LEFT, 0, 92);
    _humidity_label = createLabel(right, "湿度 58%", HUB_FONT_BODY, HUB_TEXT_COLOR);
    lv_obj_align(_humidity_label, LV_ALIGN_TOP_LEFT, 0, 126);
    _air_label = createLabel(right, "空气 良好", HUB_FONT_BODY, HUB_GREEN_COLOR);
    lv_obj_align(_air_label, LV_ALIGN_TOP_LEFT, 0, 160);

    lv_obj_t *divider = lv_obj_create(right);
    lv_obj_set_size(divider, right_w - 26, 1);
    lv_obj_align(divider, LV_ALIGN_TOP_LEFT, 0, 214);
    lv_obj_set_style_bg_color(divider, lv_color_hex(0x334155), 0);
    lv_obj_set_style_border_width(divider, 0, 0);
    lv_obj_clear_flag(divider, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *env_title = createLabel(right, "室内环境", HUB_FONT_HEAD, HUB_TEXT_COLOR);
    lv_obj_align(env_title, LV_ALIGN_TOP_LEFT, 0, 226);
    _indoor_label = createLabel(right, "室内 25C / 51%", HUB_FONT_BODY, HUB_TEXT_COLOR);
    lv_obj_align(_indoor_label, LV_ALIGN_TOP_LEFT, 0, 254);
    _night_label = createLabel(right, "夜间风险低", HUB_FONT_BODY, HUB_MUTED_COLOR);
    lv_obj_set_width(_night_label, right_w - 28);
    lv_obj_align(_night_label, LV_ALIGN_TOP_LEFT, 0, 294);

    lv_obj_t *bottom = createPanel(_root, _width - 28, 92, HUB_PANEL_COLOR);
    lv_obj_align(bottom, LV_ALIGN_BOTTOM_MID, 0, 0);

    for (int i = 0; i < 4; ++i) {
        lv_obj_t *event = createPanel(bottom, (_width - 94) / 4, 66, HUB_PANEL_2_COLOR);
        lv_obj_align(event, LV_ALIGN_LEFT_MID, i * ((_width - 94) / 4 + 12), 0);
        _event_cards[i] = event;
        _event_level_labels[i] = createLabel(event, "L0", HUB_FONT_SMALL, HUB_GREEN_COLOR);
        lv_obj_align(_event_level_labels[i], LV_ALIGN_TOP_LEFT, 0, 0);
        _event_time_labels[i] = createLabel(event, "08:30", HUB_FONT_SMALL, HUB_MUTED_COLOR);
        lv_obj_align(_event_time_labels[i], LV_ALIGN_TOP_RIGHT, 0, 0);
        _event_text_labels[i] = createLabel(event, "正常巡检", HUB_FONT_SMALL, HUB_TEXT_COLOR);
        lv_obj_set_width(_event_text_labels[i], ((_width - 94) / 4) - 30);
        lv_obj_align(_event_text_labels[i], LV_ALIGN_BOTTOM_LEFT, 0, 0);
    }

    lv_obj_t *actions = lv_obj_create(center);
    lv_obj_set_size(actions, center_w - 24, 42);
    lv_obj_align(actions, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(actions, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(actions, 0, 0);
    lv_obj_set_style_pad_all(actions, 0, 0);
    lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(actions, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(actions, LV_OBJ_FLAG_SCROLLABLE);

    const char *button_texts[] = {"巡检", "回充", "隐私", "呼叫"};
    for (int i = 0; i < 4; ++i) {
        lv_obj_t *btn = lv_btn_create(actions);
        lv_obj_set_size(btn, 82, 38);
        lv_obj_set_style_radius(btn, 6, 0);
        lv_obj_set_style_bg_color(btn, i == 2 ? HUB_PURPLE_COLOR : HUB_BLUE_COLOR, 0);
        lv_obj_add_event_cb(btn, actionEventCb, LV_EVENT_CLICKED, this);
        lv_obj_set_user_data(btn, reinterpret_cast<void *>(static_cast<intptr_t>(i)));
        lv_obj_t *label = createLabel(btn, button_texts[i], HUB_FONT_SMALL, lv_color_white());
        lv_obj_center(label);
    }

    lv_obj_t *modes = lv_obj_create(right);
    lv_obj_set_size(modes, right_w - 24, 86);
    lv_obj_align(modes, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(modes, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(modes, 0, 0);
    lv_obj_set_style_pad_all(modes, 0, 0);
    lv_obj_set_flex_flow(modes, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(modes, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(modes, LV_OBJ_FLAG_SCROLLABLE);

    const char *mode_texts[] = {"正常", "跌倒", "浴室", "夜间"};
    for (int i = 0; i < MODE_MAX; ++i) {
        lv_obj_t *btn = lv_btn_create(modes);
        lv_obj_set_size(btn, (right_w - 40) / 2, 36);
        lv_obj_set_style_radius(btn, 6, 0);
        lv_obj_set_style_bg_color(btn, HUB_PANEL_2_COLOR, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_color(btn, HUB_BLUE_COLOR, 0);
        lv_obj_add_event_cb(btn, scenarioEventCb, LV_EVENT_CLICKED, this);
        lv_obj_set_user_data(btn, reinterpret_cast<void *>(static_cast<intptr_t>(i)));
        lv_obj_t *label = createLabel(btn, mode_texts[i], HUB_FONT_SMALL, HUB_TEXT_COLOR);
        lv_obj_center(label);
    }

    return true;
}

void HomeCareHub::setMode(DemoMode mode)
{
    _mode = mode;
    updateUi();
}

void HomeCareHub::updateUi(void)
{
    const DashboardState states[MODE_MAX] = {
        {
            "正常巡检", "WiFi-CSI 在线 | 本地 AI 就绪", "隐私：摄像头关闭",
            "多云", "室外 24C", "湿度 58%", "空气 良好", "室内 25C / 51%", "夜间风险低",
            "待命中", "充电桩", "客厅巡检点", "CSI 看护", "障碍：通畅", "摄像头：关闭", "语音：待命",
            86, 12,
            {{
                {"客厅", "有人且稳定", "风险 L0", 31, true, false, HUB_GREEN_COLOR},
                {"卧室", "休息区安静", "风险 L0", 18, false, true, HUB_GREEN_COLOR},
                {"浴室", "未见久留", "风险 L0", 14, false, true, HUB_GREEN_COLOR},
                {"走廊", "通道清", "风险 L0", 20, false, false, HUB_GREEN_COLOR},
            }},
            {{
                {"L0", "08:30", "日常巡检完成", HUB_GREEN_COLOR},
                {"L0", "08:18", "摄像头保持关闭", HUB_GREEN_COLOR},
                {"L1", "07:42", "卧室轻微活动", HUB_BLUE_COLOR},
                {"L0", "07:20", "小车已回充", HUB_GREEN_COLOR},
            }},
        },
        {
            "疑似跌倒", "CSI 高动态 | 小车出发", "隐私：事件确认",
            "多云", "室外 24C", "湿度 58%", "空气 良好", "室内 25C / 51%", "优先确认客厅",
            "前往现场", "走廊", "客厅巡检点", "视觉确认", "障碍：已绕行", "摄像头：本地 AI 开启", "语音：询问用户",
            78, 74,
            {{
                {"客厅", "高动态后静止", "风险 L3", 88, true, false, HUB_RED_COLOR},
                {"卧室", "无活动", "风险 L0", 12, false, true, HUB_GREEN_COLOR},
                {"浴室", "清", "风险 L0", 10, false, true, HUB_GREEN_COLOR},
                {"走廊", "小车经过", "风险 L1", 45, true, false, HUB_BLUE_COLOR},
            }},
            {{
                {"L3", "22:31", "视觉确认疑似跌倒", HUB_RED_COLOR},
                {"L2", "22:30", "小车前往客厅", HUB_ORANGE_COLOR},
                {"L2", "22:30", "CSI 疑似跌倒", HUB_ORANGE_COLOR},
                {"L0", "22:15", "夜间巡检待命", HUB_GREEN_COLOR},
            }},
        },
        {
            "浴室久留", "CSI 久留计时 | 门区保护", "隐私：浴室保护",
            "小雨", "室外 22C", "湿度 76%", "空气 良好", "室内 24C / 55%", "超时前先观察",
            "门外等待", "浴室门口", "浴室门口", "语音询问", "障碍：通畅", "摄像头：仅门区", "语音：轻声询问",
            72, 100,
            {{
                {"客厅", "安静", "风险 L0", 15, false, false, HUB_GREEN_COLOR},
                {"卧室", "无人", "风险 L0", 12, false, true, HUB_GREEN_COLOR},
                {"浴室", "久留超时", "风险 L2", 79, true, true, HUB_ORANGE_COLOR},
                {"走廊", "小车停靠", "风险 L1", 40, true, false, HUB_BLUE_COLOR},
            }},
            {{
                {"L2", "21:08", "浴室久留超时", HUB_ORANGE_COLOR},
                {"L2", "21:07", "小车门外等待", HUB_ORANGE_COLOR},
                {"L1", "21:02", "进入浴室区", HUB_BLUE_COLOR},
                {"L0", "20:55", "隐私区摄像头关闭", HUB_GREEN_COLOR},
            }},
        },
        {
            "夜间离床", "CSI 离床 | 低速跟随", "隐私：摄像头关闭",
            "晴夜", "室外 19C", "湿度 62%", "空气 优秀", "室内 23C / 49%", "引导灯并看护走廊",
            "低速跟随", "卧室门口", "走廊点位", "夜间陪护", "障碍：通畅", "摄像头：关闭", "语音：待命",
            81, 46,
            {{
                {"客厅", "无活动", "风险 L0", 18, false, false, HUB_GREEN_COLOR},
                {"卧室", "离床", "风险 L1", 64, true, true, HUB_BLUE_COLOR},
                {"浴室", "等待", "风险 L0", 16, false, true, HUB_GREEN_COLOR},
                {"走廊", "发现活动", "风险 L1", 56, true, false, HUB_BLUE_COLOR},
            }},
            {{
                {"L1", "02:14", "夜间离床发现", HUB_BLUE_COLOR},
                {"L1", "02:14", "小车移动至走廊", HUB_BLUE_COLOR},
                {"L0", "02:10", "卧室休息稳定", HUB_GREEN_COLOR},
                {"L0", "01:40", "系统安静看护", HUB_GREEN_COLOR},
            }},
        },
    };

    const DashboardState &state = states[_mode];
    lv_label_set_text(_mode_label, state.mode_name);
    lv_obj_set_style_text_color(_mode_label, _mode == MODE_FALL ? HUB_RED_COLOR : (_mode == MODE_BATHROOM ? HUB_ORANGE_COLOR : HUB_GREEN_COLOR), 0);
    lv_label_set_text(_status_label, state.home_status);
    lv_label_set_text(_privacy_label, _privacy_enabled ? state.privacy : "隐私：手动允许摄像头");
    lv_obj_set_style_text_color(_privacy_label, _privacy_enabled ? HUB_GREEN_COLOR : HUB_YELLOW_COLOR, 0);

    lv_label_set_text(_weather_label, state.weather);
    lv_label_set_text(_outdoor_label, state.outdoor_temp);
    lv_label_set_text(_humidity_label, state.humidity);
    lv_label_set_text(_air_label, state.air_quality);
    lv_label_set_text(_indoor_label, state.indoor_env);
    lv_label_set_text(_night_label, state.night_hint);

    lv_label_set_text(_car_status_label, state.car_status);
    lv_obj_set_style_text_color(_car_status_label, _mode == MODE_FALL ? HUB_RED_COLOR : (_mode == MODE_BATHROOM ? HUB_ORANGE_COLOR : HUB_GREEN_COLOR), 0);
    lv_label_set_text_fmt(_car_position_label, "位置：%s", state.car_position);
    lv_label_set_text_fmt(_car_target_label, "目标：%s", state.car_target);
    lv_label_set_text_fmt(_car_phase_label, "阶段：%s", state.car_phase);
    lv_label_set_text(_obstacle_label, state.obstacle);
    lv_label_set_text(_camera_label, state.camera);
    lv_label_set_text(_voice_label, state.voice);
    lv_bar_set_value(_battery_bar, state.battery, LV_ANIM_ON);
    lv_label_set_text_fmt(_battery_label, "%d%%", state.battery);
    lv_bar_set_value(_route_bar, state.route_progress, LV_ANIM_ON);
    lv_label_set_text_fmt(_route_label, "%s > %s", state.car_position, state.car_target);

    for (int i = 0; i < 4; ++i) {
        const RoomState &room = state.rooms[i];
        lv_label_set_text(_room_name_labels[i], room.name);
        lv_label_set_text_fmt(_room_activity_labels[i], "%s%s", room.activity, room.privacy_zone ? " | 私密" : "");
        lv_label_set_text(_room_risk_labels[i], room.risk);
        lv_obj_set_style_text_color(_room_risk_labels[i], room.accent, 0);
        lv_label_set_text_fmt(_room_csi_labels[i], "CSI %d%%", room.csi);
        setCardAccent(_room_cards[i], room.accent);
    }

    for (int i = 0; i < 4; ++i) {
        const EventState &event = state.events[i];
        lv_label_set_text(_event_level_labels[i], event.level);
        lv_label_set_text(_event_time_labels[i], event.time);
        lv_label_set_text(_event_text_labels[i], event.text);
        lv_obj_set_style_text_color(_event_level_labels[i], event.accent, 0);
        setCardAccent(_event_cards[i], event.accent);
    }
}

void HomeCareHub::scenarioEventCb(lv_event_t *e)
{
    HomeCareHub *app = static_cast<HomeCareHub *>(lv_event_get_user_data(e));
    lv_obj_t *target = static_cast<lv_obj_t *>(lv_event_get_target(e));
    int mode = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(target)));
    if (app != nullptr && mode >= 0 && mode < MODE_MAX) {
        app->setMode(static_cast<DemoMode>(mode));
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

    if (action == 0) {
        app->setMode(MODE_NORMAL);
    } else if (action == 1) {
        app->setMode(MODE_NORMAL);
        lv_label_set_text(app->_car_status_label, "回充中");
        lv_label_set_text(app->_car_phase_label, "阶段：回充指令");
        lv_bar_set_value(app->_route_bar, 30, LV_ANIM_ON);
    } else if (action == 3) {
        lv_label_set_text(app->_voice_label, "语音：呼叫家人");
    } else {
        app->_privacy_enabled = !app->_privacy_enabled;
        app->updateUi();
    }
}

void HomeCareHub::timerCb(lv_timer_t *timer)
{
    HomeCareHub *app = static_cast<HomeCareHub *>(timer->user_data);
    if (app == nullptr) {
        return;
    }
    int next = (static_cast<int>(app->_mode) + 1) % MODE_MAX;
    app->setMode(static_cast<DemoMode>(next));
}
