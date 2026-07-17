/**
 * @file HomeCareHub.cpp
 * @brief 家庭智能中控主界面应用实现
 *
 * 实现完整的三栏式智能家居仪表盘 UI，包括：
 * - LVGL 控件创建与样式设置
 * - 四种场景模式（日常/跌倒/浴室/夜间）的数据与配色切换
 * - MQTT 远程消息处理（模式切换、事件注入、小车姿态）
 * - 天气服务数据集成与实时刷新
 * - 定时器驱动的场景轮播与 MQTT 轮询
 */
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <algorithm>
#include "esp_err.h"
#include "esp_log.h"
#include "HomeCareHub.hpp"
#include "HomeCareWeather.hpp"
#include "HomeCareWeatherCity.hpp"

/* 外部资源声明：应用图标和中文字体（不同字号） */
LV_IMG_DECLARE(img_app_setting);
LV_FONT_DECLARE(homecare_font_simsun_14);
LV_FONT_DECLARE(homecare_font_simsun_16);
LV_FONT_DECLARE(homecare_font_simsun_20);
LV_FONT_DECLARE(homecare_font_simsun_28);

static const char *TAG = "HomeCareHub";

/* ========== 主题颜色定义（Vercel 风格：浅底、黑白、细边框） ========== */
#define HUB_BG_COLOR           lv_color_hex(0xFAFAFA)   /**< 页面背景 */
#define HUB_BG_GRAD_COLOR      lv_color_hex(0xFAFAFA)   /**< 页面背景渐变结束色 */
#define HUB_PANEL_COLOR        lv_color_hex(0xFFFFFF)   /**< 卡片/面板白色表面 */
#define HUB_PANEL_SOLID_COLOR  lv_color_hex(0xF5F5F5)   /**< 次级浅灰表面 */
#define HUB_LINE_COLOR         lv_color_hex(0xEBEBEB)   /**< Vercel hairline */
#define HUB_LINE_STRONG_COLOR  lv_color_hex(0xA1A1A1)   /**< 强分隔线 */
#define HUB_SHADOW_COLOR       lv_color_hex(0x000000)   /**< 轻阴影 */
#define HUB_TEXT_COLOR         lv_color_hex(0x171717)   /**< Ink */
#define HUB_MUTED_COLOR        lv_color_hex(0x4D4D4D)   /**< Body */
#define HUB_FAINT_COLOR        lv_color_hex(0x888888)   /**< Muted */
#define HUB_CYAN_COLOR         lv_color_hex(0x171717)   /**< 主操作黑 */
#define HUB_GREEN_COLOR        lv_color_hex(0x0070F3)   /**< 正常/链接蓝 */
#define HUB_AMBER_COLOR        lv_color_hex(0xF5A623)   /**< 警告 */
#define HUB_CORAL_COLOR        lv_color_hex(0xEE0000)   /**< 异常 */
#define HUB_BLUE_COLOR         lv_color_hex(0x0070F3)   /**< 信息蓝 */

/* ========== 字体与布局常量 ========== */
#define HUB_FONT_TITLE         (&homecare_font_simsun_28)  /**< 标题字号 28px */
#define HUB_FONT_HEAD          (&homecare_font_simsun_20)  /**< 小标题字号 20px */
#define HUB_FONT_BODY          (&homecare_font_simsun_16)  /**< 正文字号 16px */
#define HUB_FONT_SMALL         (&homecare_font_simsun_14)  /**< 辅助文字字号 14px */
#define HUB_PAGE_COUNT         (3)                         /**< 分页总数 */
#define HUB_PAGE_GAP           (12)                        /**< 分页间隔像素 */

/* ========== 静态辅助函数 ========== */

/**
 * @brief 为标签控件应用统一的文本样式
 *
 * 设置字体、颜色、字间距，并启用超长文本省略号截断。
 */
static void applyLabelStyle(lv_obj_t *label, const lv_font_t *font, lv_color_t color)
{
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_letter_space(label, 0, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
}

/**
 * @brief 将控件设为平坦透明容器样式
 *
 * 无背景、无边框、无圆角、无内边距、不可滚动。
 * 用作 Flex 布局的包裹容器。
 */
static void styleFlatContainer(lv_obj_t *obj)
{
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

/** @brief 创建一个图标符号标签（使用 LVGL 默认字体中的符号） */
static lv_obj_t *createIconLabel(lv_obj_t *parent, const char *symbol, lv_color_t color)
{
    const char *text = symbol;
    if (symbol == nullptr) {
        text = "";
    } else if (std::strcmp(symbol, LV_SYMBOL_HOME) == 0) {
        text = "H";
    } else if (std::strcmp(symbol, LV_SYMBOL_OK) == 0) {
        text = "OK";
    } else if (std::strcmp(symbol, LV_SYMBOL_SETTINGS) == 0) {
        text = "*";
    } else if (std::strcmp(symbol, LV_SYMBOL_EYE_OPEN) == 0) {
        text = "O";
    } else if (std::strcmp(symbol, LV_SYMBOL_AUDIO) == 0) {
        text = "A";
    } else if (std::strcmp(symbol, LV_SYMBOL_EDIT) == 0) {
        text = "E";
    } else if (std::strcmp(symbol, LV_SYMBOL_UP) == 0) {
        text = "^";
    } else if (std::strcmp(symbol, LV_SYMBOL_PLAY) == 0) {
        text = ">";
    } else if (std::strcmp(symbol, LV_SYMBOL_EYE_CLOSE) == 0) {
        text = "-";
    }

    lv_obj_t *icon = lv_label_create(parent);
    lv_label_set_text(icon, text);
    lv_obj_set_style_text_font(icon, HUB_FONT_SMALL, 0);
    lv_obj_set_style_text_color(icon, color, 0);
    return icon;
}

/**
 * @brief 创建模拟摄像头预览面板
 *
 * 带渐变深色背景、十字准线和"LIVE"标识，用于安防区域占位展示。
 * 实际摄像头画面需通过硬件接口接入。
 */
#if 0
static lv_obj_t *createCameraPreview(lv_obj_t *parent, int32_t width, int32_t height,
                                     lv_obj_t **image_out, lv_obj_t **empty_label_out)
{
    lv_obj_t *camera = lv_obj_create(parent);
    lv_obj_set_size(camera, width, height);
    lv_obj_set_style_bg_color(camera, HUB_PANEL_SOLID_COLOR, 0);
    lv_obj_set_style_bg_grad_color(camera, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_grad_dir(camera, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_border_width(camera, 1, 0);
    lv_obj_set_style_border_color(camera, HUB_LINE_COLOR, 0);
    lv_obj_set_style_radius(camera, 8, 0);
    lv_obj_set_style_clip_corner(camera, true, 0);
    lv_obj_set_style_pad_all(camera, 0, 0);
    lv_obj_clear_flag(camera, LV_OBJ_FLAG_SCROLLABLE);

    /* 十字准线 - 水平线 */
    lv_obj_t *cross_1 = lv_obj_create(camera);
    lv_obj_set_size(cross_1, width - 28, 1);
    lv_obj_align(cross_1, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(cross_1, HUB_LINE_COLOR, 0);
    lv_obj_set_style_border_width(cross_1, 0, 0);
    lv_obj_set_style_pad_all(cross_1, 0, 0);
    lv_obj_clear_flag(cross_1, LV_OBJ_FLAG_SCROLLABLE);

    /* 十字准线 - 垂直线 */
    lv_obj_t *cross_2 = lv_obj_create(camera);
    lv_obj_set_size(cross_2, 1, height - 16);
    lv_obj_align(cross_2, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(cross_2, HUB_LINE_COLOR, 0);
    lv_obj_set_style_border_width(cross_2, 0, 0);
    lv_obj_set_style_pad_all(cross_2, 0, 0);
    lv_obj_clear_flag(cross_2, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *image = lv_img_create(camera);
    lv_obj_center(image);
    if (image_out != nullptr) {
        *image_out = image;
    }

    lv_obj_t *empty = lv_label_create(camera);
    lv_label_set_text(empty, "等待摄像头画面");
    applyLabelStyle(empty, HUB_FONT_SMALL, HUB_MUTED_COLOR);
    lv_obj_center(empty);
    if (empty_label_out != nullptr) {
        *empty_label_out = empty;
    }

    /* "LIVE" 实时标识标签 */
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

/* ========== HomeCareHub 类实现 ========== */

/**
 * @brief 构造函数：初始化所有成员变量为安全默认值
 *
 * 所有 LVGL 控件指针初始化为 nullptr，所有数组填充 nullptr，
 * 防止 close() 中的重复释放和悬空指针问题。
 */
#endif

HomeCareHub::HomeCareHub():
    ESP_Brookesia_PhoneApp("家庭终端", &img_app_setting, true),
    _mode(MODE_NORMAL),
    _mqtt_timer(nullptr),
    _width(1024),
    _height(600),
    _privacy_enabled(true),
    _has_mqtt_event(false),
    _mqtt_event({}),
    _mqtt_event_color(HUB_BLUE_COLOR),
    _has_smartcar_attitude(false),
    _smartcar_attitude({}),
    _smartcar_system_status(HOMECARE_MQTT_SYSTEM_STATUS_UNKNOWN),
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
    _assistant_text_label(nullptr),
    _car_status_label(nullptr),
    _car_position_label(nullptr),
    _car_target_label(nullptr),
    _car_phase_label(nullptr),
    _obstacle_label(nullptr),
    _sensor_label(nullptr),
    _voice_label(nullptr),
    _return_button(nullptr),
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
    _device_switches({})
{
}

HomeCareHub::~HomeCareHub()
{
    deleteTimer();
}

/** @brief 应用初始化：初始化 MQTT 桥接模块和天气服务 */
bool HomeCareHub::init(void)
{
    esp_err_t err = homecare_weather_city_init();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Init weather city failed: %s", esp_err_to_name(err));
    }
    return true;
}

/**
 * @brief 启动应用：计算可视区域、创建 UI、启动两个定时器
 *
 * _mqtt_timer: 1 秒定时器，轮询 MQTT 消息和天气更新。
 * 场景模式仅由 MQTT 消息和用户点击驱动，不再自动轮播。
 */
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
    _mqtt_timer = lv_timer_create(timerCb, 1000, this);
    if (_mqtt_timer == nullptr) {
        ESP_LOGE(TAG, "Create HomeCare timers failed");
        close();
        return false;
    }
    return true;
}

/** @brief 返回键处理：通知框架核心关闭本应用 */
bool HomeCareHub::back(void)
{
    notifyCoreClosed();
    return true;
}

/**
 * @brief 关闭应用：销毁定时器和根控件，将所有指针置空
 *
 * 调用 lv_obj_del(_root) 会递归销毁所有子控件，
 * 此处仅需将缓存的指针清零以防止悬空引用。
 */
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
    _assistant_text_label = nullptr;
    _car_status_label = nullptr;
    _car_position_label = nullptr;
    _car_target_label = nullptr;
    _car_phase_label = nullptr;
    _obstacle_label = nullptr;
    _sensor_label = nullptr;
    _voice_label = nullptr;
    _return_button = nullptr;
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
    return true;
}

/** @brief 安全销毁 MQTT 与天气刷新定时器 */
void HomeCareHub::deleteTimer(void)
{
    if (_mqtt_timer != nullptr) {
        lv_timer_del(_mqtt_timer);
        _mqtt_timer = nullptr;
    }
}

/* ========== UI 工厂方法实现 ========== */

/** @brief 创建带边框、圆角、内边距和阴影的标准面板 */
lv_obj_t *HomeCareHub::createPanel(lv_obj_t *parent, int32_t width, int32_t height, lv_color_t bg)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, width, height);
    lv_obj_set_style_bg_color(panel, bg, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, HUB_LINE_COLOR, 0);
    lv_obj_set_style_border_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_set_style_pad_top(panel, 12, 0);
    lv_obj_set_style_pad_bottom(panel, 12, 0);
    lv_obj_set_style_pad_left(panel, 12, 0);
    lv_obj_set_style_pad_right(panel, 12, 0);
    lv_obj_set_style_shadow_width(panel, 8, 0);
    lv_obj_set_style_shadow_ofs_y(panel, 2, 0);
    lv_obj_set_style_shadow_color(panel, HUB_SHADOW_COLOR, 0);
    lv_obj_set_style_shadow_opa(panel, LV_OPA_10, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    return panel;
}

/** @brief 创建文本标签并应用统一的字体和颜色样式 */
lv_obj_t *HomeCareHub::createLabel(lv_obj_t *parent, const char *text, const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    applyLabelStyle(label, font, color);
    return label;
}

/**
 * @brief 创建胶囊形状态徽标（图标 + 文本）
 *
 * 用于顶部状态栏的模式、温度、隐私等快速状态指示。
 * 文本为空时自动隐藏标签（仅显示图标）。
 */
lv_obj_t *HomeCareHub::createStatusPill(lv_obj_t *parent, const char *icon, const char *text, lv_color_t color)
{
    lv_obj_t *pill = lv_obj_create(parent);
    lv_obj_set_size(pill, LV_SIZE_CONTENT, 34);
    lv_obj_set_style_bg_color(pill, HUB_PANEL_SOLID_COLOR, 0);
    lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pill, 1, 0);
    lv_obj_set_style_border_color(pill, HUB_LINE_COLOR, 0);
    lv_obj_set_style_radius(pill, 8, 0);
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

/**
 * @brief 创建区域标题栏：左侧显示标题，右侧显示备注文本
 *
 * 用于各面板（房间、自动化、设备等）的顶部标题行。
 */
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

/** @brief 为卡片设置左侧强调色条和对应的阴影颜色 */
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
    lv_obj_set_style_shadow_color(obj, HUB_SHADOW_COLOR, 0);
}

/** @brief 设置透明头部区域样式：无背景，仅保留底部边框线分隔 */
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

/**
 * @brief 设置按钮样式
 *
 * @param bg 按钮主色
 * @param filled true=填充风格（高亮），false=描边风格（低调）
 *
 * 两种风格均有按下态的颜色变暗和高度缩小动画反馈。
 */
void HomeCareHub::styleButton(lv_obj_t *obj, lv_color_t bg, bool filled)
{
    lv_obj_set_style_radius(obj, 6, 0);
    lv_obj_set_style_bg_color(obj, filled ? bg : HUB_PANEL_SOLID_COLOR, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, filled ? bg : HUB_LINE_COLOR, 0);
    lv_obj_set_style_shadow_width(obj, filled ? 4 : 0, 0);
    lv_obj_set_style_shadow_ofs_y(obj, 1, 0);
    lv_obj_set_style_shadow_color(obj, HUB_SHADOW_COLOR, 0);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_10, 0);
    lv_obj_set_style_bg_color(obj, filled ? lv_color_hex(0x000000) : HUB_PANEL_COLOR, LV_STATE_PRESSED);
    lv_obj_set_style_transform_height(obj, -1, LV_STATE_PRESSED);
}

/** @brief 设置进度条样式：圆形端点，深色轨道，渐变色指示器 */
void HomeCareHub::styleBar(lv_obj_t *obj, lv_color_t color)
{
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(obj, HUB_PANEL_SOLID_COLOR, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(obj, color, LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_color(obj, color, LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
}

/**
 * @brief 更新分页指示器圆点样式
 *
 * 当前页圆点拉长（宽 22px）并变为青色，其余页圆点收缩（宽 8px）并变为灰色。
 */
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

/**
 * @brief 构建完整的三栏式仪表盘 UI
 *
 * 布局结构:
 * ┌─────────────────── 顶部状态栏 ───────────────────┐
 * │ 品牌标识 │ 时间/日期 │ 模式/温度/隐私胶囊       │
 * ├────────┬────────────────┬─────────────────────────┤
 * │ 左侧面板 │   中央面板     │       右侧面板          │
 * │ 天气卡片 │ 场景按钮行     │ 常用设备控制            │
 * │ 房间列表 │ 舒适度+监控    │ 门厅安防摄像头          │
 * │ 自动化   │               │ 语音助手                │
 * └────────┴────────────────┴─────────────────────────┘
 */
bool HomeCareHub::createUi(void)
{
    /* 创建根容器，Vercel 式浅色工作台背景 */
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
    const int dot_h = 18;
    const int content_w = _width - 24;
    const int content_y = 12 + top_h + gap;
    const int content_h = _height - content_y - dot_h - 14;
    const int page_w = content_w;
    const int col_w = (page_w - gap) / 2;

    /* ==================== 顶部状态栏 ==================== */
    lv_obj_t *top = lv_obj_create(_root);
    lv_obj_set_size(top, content_w, top_h);
    lv_obj_align(top, LV_ALIGN_TOP_MID, 0, 0);
    styleHeader(top);

    /* 品牌标识方块 + 图标 */
    lv_obj_t *brand_mark = lv_obj_create(top);
    lv_obj_set_size(brand_mark, 42, 42);
    lv_obj_align(brand_mark, LV_ALIGN_LEFT_MID, 0, -4);
    lv_obj_set_style_bg_color(brand_mark, HUB_TEXT_COLOR, 0);
    lv_obj_set_style_border_width(brand_mark, 1, 0);
    lv_obj_set_style_border_color(brand_mark, HUB_TEXT_COLOR, 0);
    lv_obj_set_style_radius(brand_mark, 8, 0);
    lv_obj_set_style_pad_all(brand_mark, 0, 0);
    lv_obj_clear_flag(brand_mark, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *brand_icon = createIconLabel(brand_mark, LV_SYMBOL_HOME, HUB_BG_COLOR);
    lv_obj_center(brand_icon);

    /* 品牌名称与副标题 */
    lv_obj_t *brand = createLabel(top, "Astra Home", HUB_FONT_HEAD, HUB_TEXT_COLOR);
    lv_obj_align_to(brand, brand_mark, LV_ALIGN_OUT_RIGHT_TOP, 10, 1);
    lv_obj_t *brand_subtitle = createLabel(top, "晴湾公寓 · 智能中控", HUB_FONT_SMALL, HUB_MUTED_COLOR);
    lv_obj_align_to(brand_subtitle, brand, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 3);
    _status_label = brand_subtitle;

    /* 时间与日期显示 */
    _time_label = createLabel(top, "08:24", HUB_FONT_TITLE, HUB_TEXT_COLOR);
    lv_obj_align(_time_label, LV_ALIGN_TOP_MID, 0, 1);
    _date_label = createLabel(top, "5月26日 星期二", HUB_FONT_SMALL, HUB_MUTED_COLOR);
    lv_obj_align_to(_date_label, _time_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);

    /* 右上角状态胶囊组：模式、温度、隐私 */
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

    /* ==================== 横向分页容器 ==================== */
    _pages = lv_obj_create(_root);
    lv_obj_set_size(_pages, page_w, content_h);
    lv_obj_align(_pages, LV_ALIGN_TOP_MID, 0, content_y - 12);
    styleFlatContainer(_pages);
    lv_obj_add_flag(_pages, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ONE);
    lv_obj_set_scroll_dir(_pages, LV_DIR_HOR);
    lv_obj_set_scroll_snap_x(_pages, LV_SCROLL_SNAP_CENTER);
    lv_obj_set_scrollbar_mode(_pages, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_pad_column(_pages, HUB_PAGE_GAP, 0);
    lv_obj_set_flex_flow(_pages, LV_FLEX_FLOW_ROW);
    lv_obj_add_event_cb(_pages, scrollEventCb, LV_EVENT_SCROLL, this);

    lv_obj_t *page_overview = lv_obj_create(_pages);
    lv_obj_set_size(page_overview, page_w, content_h);
    styleFlatContainer(page_overview);
    lv_obj_set_flex_grow(page_overview, 0);

    lv_obj_t *page_patrol = lv_obj_create(_pages);
    lv_obj_set_size(page_patrol, page_w, content_h);
    styleFlatContainer(page_patrol);
    lv_obj_set_flex_grow(page_patrol, 0);

    lv_obj_t *page_control = lv_obj_create(_pages);
    lv_obj_set_size(page_control, page_w, content_h);
    styleFlatContainer(page_control);
    lv_obj_set_flex_grow(page_control, 0);

    /* ==================== Page 1: Overview ==================== */
    const int hero_h = content_h - 170;
    const int lower_h = content_h - hero_h - gap;

    lv_obj_t *hero = createPanel(page_overview, col_w, hero_h, HUB_PANEL_COLOR);
    lv_obj_align(hero, LV_ALIGN_TOP_LEFT, 0, 0);

    _hero_kicker_label = createLabel(hero, "LIVE · 客厅", HUB_FONT_SMALL, HUB_GREEN_COLOR);
    lv_obj_align(_hero_kicker_label, LV_ALIGN_TOP_LEFT, 0, 0);
    _security_chip_label = createLabel(hero, "在家模式", HUB_FONT_SMALL, HUB_TEXT_COLOR);
    lv_obj_set_style_bg_color(_security_chip_label, HUB_PANEL_SOLID_COLOR, 0);
    lv_obj_set_style_bg_opa(_security_chip_label, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_security_chip_label, 1, 0);
    lv_obj_set_style_border_color(_security_chip_label, HUB_LINE_COLOR, 0);
    lv_obj_set_style_radius(_security_chip_label, 8, 0);
    lv_obj_set_style_pad_left(_security_chip_label, 10, 0);
    lv_obj_set_style_pad_right(_security_chip_label, 10, 0);
    lv_obj_set_style_pad_top(_security_chip_label, 3, 0);
    lv_obj_set_style_pad_bottom(_security_chip_label, 3, 0);
    lv_obj_align(_security_chip_label, LV_ALIGN_TOP_RIGHT, 0, -2);

    _hero_room_label = createLabel(hero, "客厅主控", HUB_FONT_TITLE, HUB_TEXT_COLOR);
    lv_obj_align(_hero_room_label, LV_ALIGN_TOP_LEFT, 0, 42);
    _hero_text_label = createLabel(hero, "灯光、温度、影音和安防已同步到当前场景。", HUB_FONT_BODY, HUB_MUTED_COLOR);
    lv_obj_set_width(_hero_text_label, col_w - 32);
    lv_obj_align(_hero_text_label, LV_ALIGN_TOP_LEFT, 0, 82);

    lv_obj_t *comfort = createPanel(hero, col_w - 24, hero_h - 132, HUB_PANEL_SOLID_COLOR);
    lv_obj_align(comfort, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_shadow_width(comfort, 0, 0);
    _comfort_temp_label = createLabel(comfort, "25°", HUB_FONT_TITLE, HUB_TEXT_COLOR);
    lv_obj_align(_comfort_temp_label, LV_ALIGN_TOP_LEFT, 0, 0);
    _comfort_status_label = createLabel(comfort, "自动恒温 · 新风低速", HUB_FONT_SMALL, HUB_MUTED_COLOR);
    lv_obj_align(_comfort_status_label, LV_ALIGN_TOP_LEFT, 0, 44);
    _temp_arc = lv_arc_create(comfort);
    lv_obj_set_size(_temp_arc, 112, 112);
    lv_obj_align(_temp_arc, LV_ALIGN_RIGHT_MID, -8, 0);
    lv_arc_set_range(_temp_arc, 18, 30);
    lv_arc_set_value(_temp_arc, 25);
    lv_obj_set_style_arc_color(_temp_arc, HUB_LINE_COLOR, LV_PART_MAIN);
    lv_obj_set_style_arc_color(_temp_arc, HUB_TEXT_COLOR, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(_temp_arc, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_width(_temp_arc, 8, LV_PART_INDICATOR);

    lv_obj_t *comfort_metrics = lv_obj_create(comfort);
    lv_obj_set_size(comfort_metrics, col_w - 170, 58);
    lv_obj_align(comfort_metrics, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    styleFlatContainer(comfort_metrics);
    lv_obj_set_style_pad_column(comfort_metrics, 8, 0);
    lv_obj_set_flex_flow(comfort_metrics, LV_FLEX_FLOW_ROW);
    const char *comfort_names[] = {"灯光", "空气", "功耗"};
    lv_obj_t **comfort_values[] = {&_light_label, &_air_label, &_power_label};
    for (int i = 0; i < 3; ++i) {
        lv_obj_t *metric = createPanel(comfort_metrics, 64, 54, HUB_PANEL_COLOR);
        lv_obj_set_style_shadow_width(metric, 0, 0);
        lv_obj_set_style_pad_all(metric, 6, 0);
        createLabel(metric, comfort_names[i], HUB_FONT_SMALL, HUB_FAINT_COLOR);
        *comfort_values[i] = createLabel(metric, i == 0 ? "68%" : (i == 1 ? "优" : "1.8kW"), HUB_FONT_SMALL, HUB_TEXT_COLOR);
        lv_obj_align(*comfort_values[i], LV_ALIGN_BOTTOM_LEFT, 0, 0);
    }

    lv_obj_t *weather = createPanel(page_overview, col_w, 132, HUB_PANEL_COLOR);
    lv_obj_align_to(weather, hero, LV_ALIGN_OUT_RIGHT_TOP, gap, 0);

    /* 天气主温度（大字号） */
    _weather_temp_label = createLabel(weather, "25°", HUB_FONT_TITLE, HUB_TEXT_COLOR);
    lv_obj_align(_weather_temp_label, LV_ALIGN_TOP_LEFT, 0, 4);
    _weather_summary_label = createLabel(weather, "客厅舒适 · 湿度 42%", HUB_FONT_SMALL, HUB_MUTED_COLOR);
    lv_obj_align(_weather_summary_label, LV_ALIGN_TOP_LEFT, 2, 58);

    /* 天气图标方块 */
    lv_obj_t *weather_icon_box = lv_obj_create(weather);
    lv_obj_set_size(weather_icon_box, 48, 48);
    lv_obj_align(weather_icon_box, LV_ALIGN_TOP_RIGHT, 0, 6);
    lv_obj_set_style_bg_color(weather_icon_box, HUB_PANEL_SOLID_COLOR, 0);
    lv_obj_set_style_border_width(weather_icon_box, 1, 0);
    lv_obj_set_style_border_color(weather_icon_box, HUB_LINE_COLOR, 0);
    lv_obj_set_style_radius(weather_icon_box, 8, 0);
    lv_obj_set_style_pad_all(weather_icon_box, 0, 0);
    lv_obj_clear_flag(weather_icon_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *weather_icon = createIconLabel(weather_icon_box, LV_SYMBOL_SETTINGS, HUB_TEXT_COLOR);
    lv_obj_center(weather_icon);

    /* 城市名和天气描述 */
    _weather_city_label = createLabel(weather, homecare_weather_city_get_selected_name(), HUB_FONT_SMALL, HUB_FAINT_COLOR);
    lv_obj_align(_weather_city_label, LV_ALIGN_TOP_RIGHT, 0, 58);
    _weather_label = createLabel(weather, "多云", HUB_FONT_SMALL, HUB_CYAN_COLOR);
    lv_obj_align(_weather_label, LV_ALIGN_TOP_RIGHT, 0, 76);

    /* 底部微指标行：AQI / CO2 / 噪声 */
    lv_obj_t *mini_metrics = lv_obj_create(weather);
    lv_obj_set_size(mini_metrics, col_w - 24, 34);
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

    /* ---------- 房间状态列表 ---------- */
    lv_obj_t *rooms = createPanel(page_overview, col_w, hero_h - 132 - gap, HUB_PANEL_COLOR);
    lv_obj_align_to(rooms, weather, LV_ALIGN_OUT_BOTTOM_LEFT, 0, gap);
    createSectionHeader(rooms, "房间", "6 区在线", col_w - 24, 30);

    const char *room_names[] = {"客厅", "主卧", "厨房", "书房"};
    const char *room_desc[] = {"主灯 68% · 空调 25°", "窗帘关闭 · 静音", "排风运行 · 烟感正常", "台灯 42% · 空气优"};
    const char *room_tags[] = {"舒适", "睡眠", "运行", "安静"};
    const char *room_icons[] = {LV_SYMBOL_HOME, LV_SYMBOL_AUDIO, LV_SYMBOL_SETTINGS, LV_SYMBOL_EDIT};
    const int room_row_h = 30;

    for (int i = 0; i < 4; ++i) {
        lv_obj_t *card = createPanel(rooms, col_w - 24, room_row_h, HUB_PANEL_SOLID_COLOR);
        lv_obj_align(card, LV_ALIGN_TOP_LEFT, 0, 34 + i * (room_row_h + 4));
        lv_obj_set_style_shadow_width(card, 0, 0);
        lv_obj_set_style_pad_left(card, 8, 0);
        lv_obj_set_style_pad_right(card, 8, 0);
        lv_obj_set_style_pad_top(card, 5, 0);
        lv_obj_set_style_pad_bottom(card, 5, 0);
        _room_cards[i] = card;
        _room_accent_bars[i] = lv_obj_create(card);
        lv_obj_clear_flag(_room_accent_bars[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_move_background(_room_accent_bars[i]);
        lv_obj_t *icon = createIconLabel(card, room_icons[i], HUB_MUTED_COLOR);
        lv_obj_align(icon, LV_ALIGN_LEFT_MID, 0, 0);
        _room_name_labels[i] = createLabel(card, room_names[i], HUB_FONT_SMALL, HUB_TEXT_COLOR);
        lv_obj_align(_room_name_labels[i], LV_ALIGN_TOP_LEFT, 24, -1);
        _room_activity_labels[i] = createLabel(card, room_desc[i], HUB_FONT_SMALL, HUB_MUTED_COLOR);
        lv_obj_set_width(_room_activity_labels[i], col_w - 108);
        lv_obj_align(_room_activity_labels[i], LV_ALIGN_BOTTOM_LEFT, 24, 1);
        _room_risk_labels[i] = createLabel(card, room_tags[i], HUB_FONT_SMALL, i == 0 ? HUB_CYAN_COLOR : HUB_MUTED_COLOR);
        lv_obj_align(_room_risk_labels[i], LV_ALIGN_RIGHT_MID, 0, 0);
        _room_csi_labels[i] = nullptr;
    }

    /* ---------- 场景按钮栏 ---------- */
    lv_obj_t *scene_dock = lv_obj_create(page_overview);
    lv_obj_set_size(scene_dock, page_w, lower_h);
    lv_obj_align(scene_dock, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    styleFlatContainer(scene_dock);
    lv_obj_set_style_pad_column(scene_dock, gap, 0);
    lv_obj_set_flex_flow(scene_dock, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(scene_dock, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    const char *scene_names[] = {"晨起", "会客", "观影", "睡眠"};
    const char *scene_desc[] = {"窗帘 · 音乐", "主灯 · 空调", "投影 · 遮光", "静音 · 布防"};
    const char *scene_icons[] = {LV_SYMBOL_UP, LV_SYMBOL_HOME, LV_SYMBOL_PLAY, LV_SYMBOL_EYE_CLOSE};
    const int scene_w = (page_w - gap * 3) / 4;

    for (int i = 0; i < MODE_MAX; ++i) {
        lv_obj_t *scene = lv_btn_create(scene_dock);
        lv_obj_set_size(scene, scene_w, lower_h);
        lv_obj_set_style_bg_color(scene, i == 0 ? HUB_CYAN_COLOR : HUB_PANEL_SOLID_COLOR, 0);
        lv_obj_set_style_border_width(scene, 1, 0);
        lv_obj_set_style_border_color(scene, i == 0 ? HUB_CYAN_COLOR : HUB_LINE_COLOR, 0);
        lv_obj_set_style_radius(scene, 8, 0);
        lv_obj_set_style_shadow_width(scene, 0, 0);
        lv_obj_add_event_cb(scene, scenarioEventCb, LV_EVENT_CLICKED, this);
        lv_obj_set_user_data(scene, reinterpret_cast<void *>(static_cast<intptr_t>(i)));
        _scene_cards[i] = scene;
        lv_obj_t *scene_icon = createIconLabel(scene, scene_icons[i], i == 0 ? HUB_BG_COLOR : HUB_TEXT_COLOR);
        lv_obj_align(scene_icon, LV_ALIGN_TOP_MID, 0, 8);
        _scene_name_labels[i] = createLabel(scene, scene_names[i], HUB_FONT_SMALL, i == 0 ? HUB_BG_COLOR : HUB_TEXT_COLOR);
        lv_obj_align(_scene_name_labels[i], LV_ALIGN_CENTER, 0, 5);
        _scene_desc_labels[i] = createLabel(scene, scene_desc[i], HUB_FONT_SMALL, i == 0 ? HUB_BG_COLOR : HUB_MUTED_COLOR);
        lv_obj_set_width(_scene_desc_labels[i], scene_w - 8);
        lv_obj_align(_scene_desc_labels[i], LV_ALIGN_BOTTOM_MID, 0, -6);
    }

    /* ==================== Page 2: Patrol ==================== */
    lv_obj_t *car = createPanel(page_patrol, col_w, content_h, HUB_PANEL_COLOR);
    lv_obj_align(car, LV_ALIGN_TOP_LEFT, 0, 0);
    createSectionHeader(car, "巡检车", "MQTT 在线", col_w - 24, 30);

    _car_status_label = createLabel(car, "待命中", HUB_FONT_TITLE, HUB_TEXT_COLOR);
    lv_obj_align(_car_status_label, LV_ALIGN_TOP_LEFT, 0, 48);
    _car_phase_label = createLabel(car, "CSI 看护", HUB_FONT_BODY, HUB_MUTED_COLOR);
    lv_obj_align(_car_phase_label, LV_ALIGN_TOP_LEFT, 0, 88);

    const char *car_meta_names[] = {"位置", "目标", "障碍", "传感器", "语音"};
    lv_obj_t **car_meta_values[] = {&_car_position_label, &_car_target_label, &_obstacle_label, &_sensor_label, &_voice_label};
    for (int i = 0; i < 5; ++i) {
        const int row_y = 134 + i * 40;
        lv_obj_t *row = createPanel(car, col_w - 24, 34, HUB_PANEL_SOLID_COLOR);
        lv_obj_align(row, LV_ALIGN_TOP_LEFT, 0, row_y);
        lv_obj_set_style_shadow_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 7, 0);
        lv_obj_t *name = createLabel(row, car_meta_names[i], HUB_FONT_SMALL, HUB_FAINT_COLOR);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 0, 0);
        *car_meta_values[i] = createLabel(row, "-", HUB_FONT_SMALL, HUB_TEXT_COLOR);
        lv_obj_align(*car_meta_values[i], LV_ALIGN_RIGHT_MID, 0, 0);
    }

    lv_obj_t *place_actions = lv_obj_create(car);
    lv_obj_set_size(place_actions, col_w - 24, 38);
    lv_obj_align(place_actions, LV_ALIGN_TOP_LEFT, 0, 346);
    styleFlatContainer(place_actions);
    lv_obj_set_style_pad_column(place_actions, 8, 0);
    lv_obj_set_flex_flow(place_actions, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(place_actions, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    const char *place_texts[] = {"卫生间", "卧室", "厨房"};
    const int place_action_ids[] = {5, 6, 7};
    const int place_action_w = (col_w - 24 - 16) / 3;
    for (int i = 0; i < 3; ++i) {
        lv_obj_t *btn = lv_btn_create(place_actions);
        lv_obj_set_size(btn, place_action_w, 36);
        styleButton(btn, HUB_AMBER_COLOR, false);
        lv_obj_add_event_cb(btn, actionEventCb, LV_EVENT_CLICKED, this);
        lv_obj_set_user_data(btn, reinterpret_cast<void *>(static_cast<intptr_t>(place_action_ids[i])));
        lv_obj_t *label = createLabel(btn, place_texts[i], HUB_FONT_SMALL, HUB_AMBER_COLOR);
        lv_obj_center(label);
    }

    lv_obj_t *car_actions = lv_obj_create(car);
    lv_obj_set_size(car_actions, col_w - 24, 38);
    lv_obj_align(car_actions, LV_ALIGN_TOP_LEFT, 0, 390);
    styleFlatContainer(car_actions);
    lv_obj_set_style_pad_column(car_actions, 8, 0);
    lv_obj_set_flex_flow(car_actions, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(car_actions, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    const char *car_action_texts[] = {"巡逻", "回充", "停止"};
    const int car_action_ids[] = {0, 1, 2};
    const lv_color_t car_action_colors[] = {HUB_TEXT_COLOR, HUB_BLUE_COLOR, HUB_CORAL_COLOR};
    const int car_action_w = (col_w - 24 - 16) / 3;
    for (int i = 0; i < 3; ++i) {
        lv_obj_t *btn = lv_btn_create(car_actions);
        lv_obj_set_size(btn, car_action_w, 36);
        styleButton(btn, car_action_colors[i], i == 0);
        lv_obj_add_event_cb(btn, actionEventCb, LV_EVENT_CLICKED, this);
        lv_obj_set_user_data(btn, reinterpret_cast<void *>(static_cast<intptr_t>(car_action_ids[i])));
        if (car_action_ids[i] == 1) {
            _return_button = btn;
            lv_obj_add_state(btn, LV_STATE_DISABLED);
        }
        lv_obj_t *label = createLabel(btn, car_action_texts[i], HUB_FONT_SMALL,
                                      i == 0 ? HUB_BG_COLOR : car_action_colors[i]);
        lv_obj_center(label);
    }

    lv_obj_t *security = createPanel(page_patrol, col_w, content_h, HUB_PANEL_COLOR);
    lv_obj_align_to(security, car, LV_ALIGN_OUT_RIGHT_TOP, gap, 0);
    createSectionHeader(security, "门厅安防", "实时", col_w - 24, 30);

    lv_obj_t *security_body = createLabel(security, "摄像头画面请打开 Camera 应用查看",
                                          HUB_FONT_SMALL, HUB_MUTED_COLOR);
    lv_obj_set_width(security_body, col_w - 24);
    lv_label_set_long_mode(security_body, LV_LABEL_LONG_WRAP);
    lv_obj_align(security_body, LV_ALIGN_TOP_LEFT, 0, 48);

    lv_obj_t *security_status = createLabel(security, "在家 · 周界正常", HUB_FONT_SMALL, HUB_GREEN_COLOR);
    lv_obj_align(security_status, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lv_obj_t *security_modes = lv_obj_create(security);
    lv_obj_set_size(security_modes, 150, 26);
    lv_obj_align(security_modes, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    styleFlatContainer(security_modes);
    lv_obj_set_style_pad_column(security_modes, 6, 0);
    lv_obj_set_flex_flow(security_modes, LV_FLEX_FLOW_ROW);
    const char *mode_texts[] = {"在家", "离家", "夜间"};
    for (int i = 0; i < 3; ++i) {
        lv_obj_t *mode = lv_btn_create(security_modes);
        lv_obj_set_size(mode, 46, 24);
        styleButton(mode, i == 0 ? HUB_TEXT_COLOR : HUB_BLUE_COLOR, i == 0);
        lv_obj_t *label = createLabel(mode, mode_texts[i], HUB_FONT_SMALL, i == 0 ? HUB_BG_COLOR : HUB_TEXT_COLOR);
        lv_obj_center(label);
    }

    /* ==================== Page 3: Control ==================== */
    lv_obj_t *devices = createPanel(page_control, col_w, content_h, HUB_PANEL_COLOR);
    lv_obj_align(devices, LV_ALIGN_TOP_LEFT, 0, 0);
    createSectionHeader(devices, "常用设备", "18/21", col_w - 24, 30);

    const char *device_names[] = {"客厅主灯", "中央空调", "南向窗帘", "客厅音响"};
    const char *device_states[] = {"亮度 68% · 暖白", "制冷 · 低风速", "开启 72%", "爵士电台 · 音量 32"};
    const char *device_icons[] = {LV_SYMBOL_HOME, LV_SYMBOL_SETTINGS, LV_SYMBOL_UP, LV_SYMBOL_AUDIO};

    for (int i = 0; i < 4; ++i) {
        lv_obj_t *device = lv_obj_create(devices);
        lv_obj_set_size(device, col_w - 24, 56);
        lv_obj_align(device, LV_ALIGN_TOP_LEFT, 0, 48 + i * 66);
        styleFlatContainer(device);
        _device_cards[i] = device;

        /* 设备图标方块 */
        lv_obj_t *icon_box = lv_obj_create(device);
        lv_obj_set_size(icon_box, 30, 30);
        lv_obj_align(icon_box, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_set_style_bg_color(icon_box, HUB_PANEL_SOLID_COLOR, 0);
        lv_obj_set_style_border_width(icon_box, 1, 0);
        lv_obj_set_style_border_color(icon_box, HUB_LINE_COLOR, 0);
        lv_obj_set_style_radius(icon_box, 8, 0);
        lv_obj_set_style_pad_all(icon_box, 0, 0);
        lv_obj_clear_flag(icon_box, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *device_icon = createIconLabel(icon_box, device_icons[i], HUB_TEXT_COLOR);
        lv_obj_center(device_icon);

        /* 设备名称与状态文本 */
        lv_obj_t *name = createLabel(device, device_names[i], HUB_FONT_SMALL, HUB_TEXT_COLOR);
        lv_obj_align(name, LV_ALIGN_TOP_LEFT, 40, 0);
        _device_state_labels[i] = createLabel(device, device_states[i], HUB_FONT_SMALL, HUB_MUTED_COLOR);
        lv_obj_set_width(_device_state_labels[i], col_w - 108);
        lv_obj_align(_device_state_labels[i], LV_ALIGN_BOTTOM_LEFT, 40, 0);

        /* 设备开关按钮 */
        lv_obj_t *sw = lv_btn_create(device);
        lv_obj_set_size(sw, 34, 18);
        lv_obj_align(sw, LV_ALIGN_RIGHT_MID, 0, 0);
        styleButton(sw, i == 3 ? HUB_FAINT_COLOR : HUB_CYAN_COLOR, i != 3);
        if (i == 3) {
            lv_obj_add_state(sw, LV_STATE_CHECKED);
        }
        lv_obj_add_event_cb(sw, actionEventCb, LV_EVENT_CLICKED, this);
        lv_obj_set_user_data(sw, reinterpret_cast<void *>(static_cast<intptr_t>(10 + i)));
        _device_switches[i] = sw;
    }

    lv_obj_t *right_stack = lv_obj_create(page_control);
    lv_obj_set_size(right_stack, col_w, content_h);
    lv_obj_align_to(right_stack, devices, LV_ALIGN_OUT_RIGHT_TOP, gap, 0);
    styleFlatContainer(right_stack);

    lv_obj_t *automation = createPanel(right_stack, col_w, 188, HUB_PANEL_COLOR);
    lv_obj_align(automation, LV_ALIGN_TOP_LEFT, 0, 0);
    createSectionHeader(automation, "自动化", "今日", col_w - 24, 30);

    const char *schedule_times[] = {"18:30", "21:15", "23:40"};
    const char *schedule_titles[] = {"回家场景", "影音模式", "夜间布防"};
    const char *schedule_desc[] = {"玄关灯、热水、空调", "投影、遮光帘、氛围灯", "门锁复核、周界检测"};

    for (int i = 0; i < 3; ++i) {
        lv_obj_t *event = lv_obj_create(automation);
        lv_obj_set_size(event, col_w - 24, 34);
        lv_obj_align(event, LV_ALIGN_TOP_LEFT, 0, 42 + i * 42);
        styleFlatContainer(event);
        _event_cards[i] = event;
        _event_time_labels[i] = createLabel(event, schedule_times[i], HUB_FONT_SMALL, HUB_GREEN_COLOR);
        lv_obj_align(_event_time_labels[i], LV_ALIGN_LEFT_MID, 0, 0);
        _event_level_labels[i] = createLabel(event, schedule_titles[i], HUB_FONT_SMALL, HUB_TEXT_COLOR);
        lv_obj_align(_event_level_labels[i], LV_ALIGN_TOP_LEFT, 56, -1);
        _event_text_labels[i] = createLabel(event, schedule_desc[i], HUB_FONT_SMALL, HUB_MUTED_COLOR);
        lv_obj_set_width(_event_text_labels[i], col_w - 92);
        lv_obj_align(_event_text_labels[i], LV_ALIGN_BOTTOM_LEFT, 56, 1);
    }
    _event_cards[3] = nullptr;
    _event_dot_labels.fill(nullptr);

    lv_obj_t *assistant = createPanel(right_stack, col_w, content_h - 188 - gap, HUB_PANEL_COLOR);
    lv_obj_align_to(assistant, automation, LV_ALIGN_OUT_BOTTOM_LEFT, 0, gap);
    createSectionHeader(assistant, "语音助手", "待命", col_w - 24, 30);

    _assistant_text_label = createLabel(assistant, "把客厅切到观影模式", HUB_FONT_SMALL, HUB_MUTED_COLOR);
    lv_obj_set_width(_assistant_text_label, col_w - 82);
    lv_obj_align(_assistant_text_label, LV_ALIGN_TOP_LEFT, 0, 48);

    /* 麦克风按钮 */
    lv_obj_t *mic = lv_btn_create(assistant);
    lv_obj_set_size(mic, 38, 38);
    lv_obj_align(mic, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    styleButton(mic, HUB_CYAN_COLOR, true);
    lv_obj_add_event_cb(mic, actionEventCb, LV_EVENT_CLICKED, this);
    lv_obj_set_user_data(mic, reinterpret_cast<void *>(static_cast<intptr_t>(3)));
    lv_obj_t *mic_icon = createIconLabel(mic, LV_SYMBOL_AUDIO, HUB_BG_COLOR);
    lv_obj_center(mic_icon);

    lv_obj_t *privacy = lv_btn_create(assistant);
    lv_obj_set_size(privacy, 112, 36);
    lv_obj_align(privacy, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    styleButton(privacy, HUB_TEXT_COLOR, false);
    lv_obj_add_event_cb(privacy, actionEventCb, LV_EVENT_CLICKED, this);
    lv_obj_set_user_data(privacy, reinterpret_cast<void *>(static_cast<intptr_t>(4)));
    lv_obj_t *privacy_label = createLabel(privacy, "隐私切换", HUB_FONT_SMALL, HUB_TEXT_COLOR);
    lv_obj_center(privacy_label);

    lv_obj_t *dots = lv_obj_create(_root);
    lv_obj_set_size(dots, 76, 12);
    lv_obj_align(dots, LV_ALIGN_BOTTOM_MID, 0, -1);
    styleFlatContainer(dots);
    lv_obj_set_style_pad_column(dots, 8, 0);
    lv_obj_set_flex_flow(dots, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dots, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    for (int i = 0; i < HUB_PAGE_COUNT; ++i) {
        _page_dots[i] = lv_obj_create(dots);
        lv_obj_set_size(_page_dots[i], 8, 8);
        lv_obj_set_style_radius(_page_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(_page_dots[i], HUB_FAINT_COLOR, 0);
        lv_obj_set_style_border_width(_page_dots[i], 0, 0);
        lv_obj_set_style_pad_all(_page_dots[i], 0, 0);
        lv_obj_clear_flag(_page_dots[i], LV_OBJ_FLAG_SCROLLABLE);
    }
    updatePageIndicator(0);

    /* 设置快捷引用别名（部分控件在 updateUi 中通过多个路径更新） */
    _outdoor_label = _top_weather_label;
    _humidity_label = _weather_summary_label;
    _indoor_label = _comfort_status_label;
    _night_label = _assistant_text_label;
    return true;
}

/* ========== 场景模式与数据更新 ========== */

/** @brief 切换场景模式并触发 UI 刷新 */
void HomeCareHub::setMode(DemoMode mode)
{
    _mode = mode;
    updateUi();
}

/** @brief 将内部 DemoMode 枚举转换为 MQTT 协议的模式枚举 */
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

/** @brief 将 MQTT 协议的模式枚举转换为内部 DemoMode 枚举，未知模式保持当前不变 */
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

/**
 * @brief 处理单条 MQTT 入站消息
 *
 * 处理三种消息内容:
 * 1. 模式切换指令 → 更新 _mode
 * 2. 智能小车姿态数据 → 更新 _smartcar_attitude
 * 3. 事件消息 → 注入到事件列表首位，按等级着色（L3=珊瑚/L2=琥珀/L1=蓝/其他=绿）
 */
void HomeCareHub::applyMqttMessage(const HomeCareMqttInboundMessage &message)
{
    if (message.has_mode) {
        _mode = fromMqttMode(message.mode);
    }
    if (message.has_system_status) {
        _smartcar_system_status = message.system_status;
    }

    if (message.has_smartcar_attitude && message.smartcar_attitude.valid) {
        _has_smartcar_attitude = true;
        _smartcar_attitude = message.smartcar_attitude;
        ESP_LOGI(TAG, "smartcar attitude r=%.2f p=%.2f y=%.2f mag=%d ts=%lld",
                 _smartcar_attitude.roll_deg,
                 _smartcar_attitude.pitch_deg,
                 _smartcar_attitude.yaw_deg,
                 _smartcar_attitude.has_mag ? 1 : 0,
                 _smartcar_attitude.timestamp_ms);
    }

    if (message.type == HOMECARE_MQTT_INBOUND_EVENT) {
        _has_mqtt_event = true;
        _mqtt_event = message.event;
        /* 填充缺失字段的默认值 */
        if (_mqtt_event.level[0] == '\0') {
            std::snprintf(_mqtt_event.level, sizeof(_mqtt_event.level), "L1");
        }
        if (_mqtt_event.time[0] == '\0') {
            std::snprintf(_mqtt_event.time, sizeof(_mqtt_event.time), "MQTT");
        }
        if (_mqtt_event.text[0] == '\0') {
            std::snprintf(_mqtt_event.text, sizeof(_mqtt_event.text), "MQTT message received");
        }

        /* 根据事件等级设置颜色 */
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

/** @brief 从 MQTT 桥接队列中读取并处理所有待处理消息 */
void HomeCareHub::applyMqttMessages(void)
{
    HomeCareMqttInboundMessage message = {};
    while (homecare_mqtt_bridge_receive(&message)) {
        applyMqttMessage(message);
    }
}

/**
 * @brief 根据当前场景模式刷新整个仪表盘 UI
 *
 * 这是 UI 更新的核心函数，由以下时机调用:
 * - setMode() 切换场景时
 * - applyMqttMessage() 收到 MQTT 消息时
 * - timerCb() 检测到天气数据版本变更时
 *
 * 通过预设的 states[] 数组将每种模式的完整数据一次性渲染到所有控件。
 */
void HomeCareHub::updateUi(void)
{
    if (_return_button != nullptr) {
        if (_smartcar_system_status == HOMECARE_MQTT_SYSTEM_STATUS_ABNORMAL_READY) {
            lv_obj_clear_state(_return_button, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(_return_button, LV_STATE_DISABLED);
        }
    }

    /* 预设的四种场景模式完整仪表盘数据 */
    const DashboardState states[MODE_MAX] = {
        /* MODE_NORMAL - 日常模式 */
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
        /* MODE_FALL - 异常复核模式 */
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
        /* MODE_BATHROOM - 浴室看护模式 */
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
        /* MODE_NIGHT - 夜间守护模式 */
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

    /* 获取当前模式对应的状态数据和主色调 */
    const DashboardState &state = states[_mode];
    const lv_color_t mode_color = _mode == MODE_FALL ? HUB_CORAL_COLOR :
                                  (_mode == MODE_BATHROOM ? HUB_AMBER_COLOR :
                                   (_mode == MODE_NIGHT ? HUB_BLUE_COLOR : HUB_GREEN_COLOR));

    /* 更新顶部时间显示 */
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

    /* 更新顶部状态栏胶囊 */
    lv_label_set_text(_mode_label, state.mode_name);
    lv_label_set_text(_status_label, state.home_status);
    lv_label_set_text(_privacy_label, _privacy_enabled ? state.privacy : "手动巡检模式");
    lv_obj_set_style_text_color(_mode_label, mode_color, 0);
    lv_obj_set_style_text_color(_privacy_label, _privacy_enabled ? HUB_GREEN_COLOR : HUB_AMBER_COLOR, 0);

    /* 更新中央安全模式标签 */
    lv_label_set_text(_security_chip_label, _privacy_enabled ? state.privacy : "手动模式");
    lv_obj_set_style_text_color(_security_chip_label, _privacy_enabled ? HUB_GREEN_COLOR : HUB_AMBER_COLOR, 0);
    lv_obj_set_style_bg_color(_security_chip_label, HUB_PANEL_SOLID_COLOR, 0);
    lv_obj_set_style_border_color(_security_chip_label, _privacy_enabled ? HUB_GREEN_COLOR : HUB_AMBER_COLOR, 0);

    /* 更新天气相关标签（预设值） */
    lv_label_set_text(_weather_label, state.weather);
    lv_label_set_text(_weather_summary_label, state.indoor_env);
    lv_label_set_text(_outdoor_label, state.outdoor_temp);
    lv_label_set_text(_humidity_label, state.humidity);
    lv_label_set_text(_air_label, "优");
    lv_label_set_text(_indoor_label, state.indoor_env);
    lv_label_set_text(_night_label, state.night_hint);
    lv_label_set_text(_top_weather_label, state.outdoor_temp);

    /* 用实时天气数据覆盖预设值（如果天气服务已返回数据） */
    HomeCareWeatherSnapshot weather_snapshot = {};
    if (homecare_weather_service_get_snapshot(&weather_snapshot)) {
        lv_label_set_text(_weather_city_label, weather_snapshot.city);
        lv_label_set_text(_weather_label, weather_snapshot.weather);
        lv_label_set_text(_outdoor_label, weather_snapshot.outdoor_temp);
        lv_label_set_text(_humidity_label, weather_snapshot.humidity);
        lv_label_set_text(_top_weather_label, weather_snapshot.outdoor_temp);
        lv_label_set_text(_weather_summary_label, weather_snapshot.humidity);
        /* 有实时数据时天气文本为青色，否则为灰色 */
        lv_obj_set_style_text_color(_weather_label,
                                    weather_snapshot.has_live_data ? HUB_CYAN_COLOR : HUB_MUTED_COLOR, 0);
        /* 空气质量按等级着色：优=绿, 良=青, 轻度=琥珀, 中度及以上=珊瑚 */
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

    /* 更新中央主内容区 */
    lv_label_set_text(_hero_kicker_label, _mode == MODE_NORMAL ? "LIVE · 客厅" : state.car_status);
    lv_label_set_text(_hero_room_label, _mode == MODE_NORMAL ? "客厅主控" : state.mode_name);
    lv_label_set_text(_hero_text_label, state.night_hint);
    if (_car_status_label != nullptr) {
        lv_label_set_text(_car_status_label, state.car_status);
        lv_label_set_text(_car_position_label, state.car_position);
        lv_label_set_text(_car_target_label, state.car_target);
        lv_label_set_text(_car_phase_label, state.car_phase);
        lv_label_set_text(_obstacle_label, state.obstacle);
        lv_label_set_text(_sensor_label, state.sensor);
        lv_label_set_text(_voice_label, state.voice);
    }

    /* 更新舒适度面板（不同模式对应不同温度设定） */
    lv_label_set_text(_comfort_temp_label, _mode == MODE_NIGHT ? "23°" : (_mode == MODE_BATHROOM ? "24°" : "25°"));
    lv_label_set_text(_comfort_status_label, state.indoor_env);
    lv_arc_set_value(_temp_arc, _mode == MODE_NIGHT ? 23 : (_mode == MODE_BATHROOM ? 24 : 25));
    lv_label_set_text(_light_label, _mode == MODE_NIGHT ? "12%" : "68%");
    lv_label_set_text(_power_label, _mode == MODE_FALL ? "2.4kW" : "1.8kW");

    /* 如果有智能小车姿态数据，覆盖中央区域显示 */
    if (_has_smartcar_attitude && _smartcar_attitude.valid) {
        char attitude_text[128] = {};
        char roll_text[32] = {};
        char pitch_text[32] = {};
        char yaw_text[32] = {};
        std::snprintf(attitude_text, sizeof(attitude_text),
                      "smartcar/attitude 在线：roll %.1f，pitch %.1f，yaw %.1f。",
                      _smartcar_attitude.roll_deg,
                      _smartcar_attitude.pitch_deg,
                      _smartcar_attitude.yaw_deg);
        std::snprintf(roll_text, sizeof(roll_text), "roll %.1f", _smartcar_attitude.roll_deg);
        std::snprintf(pitch_text, sizeof(pitch_text), "pitch %.1f", _smartcar_attitude.pitch_deg);
        std::snprintf(yaw_text, sizeof(yaw_text), "yaw %.1f", _smartcar_attitude.yaw_deg);

        lv_label_set_text(_hero_kicker_label, "LIVE · 姿态回传");
        lv_label_set_text(_hero_text_label, attitude_text);
        if (_car_status_label != nullptr) {
            lv_label_set_text(_car_status_label, "SmartCar 在线");
            lv_label_set_text(_car_phase_label, "MQTT attitude");
            lv_label_set_text(_car_position_label, roll_text);
            lv_label_set_text(_car_target_label, pitch_text);
            lv_label_set_text(_obstacle_label, yaw_text);
            lv_label_set_text_fmt(_sensor_label, "mag %s", _smartcar_attitude.has_mag ? "on" : "off");
            if (_smartcar_attitude.timestamp_ms > 0) {
                lv_label_set_text_fmt(_voice_label, "ts %lld", _smartcar_attitude.timestamp_ms);
            } else {
                lv_label_set_text(_voice_label, "ts -");
            }
            lv_obj_set_style_text_color(_car_status_label, HUB_GREEN_COLOR, 0);
            lv_obj_set_style_text_color(_car_phase_label, HUB_BLUE_COLOR, 0);
        }
    }

    /* 更新四个房间卡片 */
    for (int i = 0; i < 4; ++i) {
        const RoomState &room = state.rooms[i];
        lv_label_set_text(_room_name_labels[i], room.name);
        lv_label_set_text(_room_activity_labels[i], room.activity);
        lv_label_set_text(_room_risk_labels[i], room.risk);
        lv_obj_set_style_text_color(_room_risk_labels[i], room.accent, 0);
        lv_obj_set_style_bg_color(_room_cards[i], HUB_PANEL_SOLID_COLOR, 0);
        setCardAccent(_room_cards[i], _room_accent_bars[i], room.accent);
    }

    /* 更新场景按钮高亮状态（当前模式对应按钮变为青色填充） */
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

    /* 更新事件/日程列表 */
    for (int i = 0; i < 3; ++i) {
        const EventState &event = state.events[i];
        lv_label_set_text(_event_level_labels[i], event.level);
        lv_label_set_text(_event_time_labels[i], event.time);
        lv_label_set_text(_event_text_labels[i], event.text);
        lv_obj_set_style_text_color(_event_time_labels[i], event.accent, 0);
    }

    /* 如果有 MQTT 事件，将其覆盖到事件列表的第一条 */
    if (_has_mqtt_event && _event_level_labels[0] != nullptr) {
        lv_label_set_text(_event_level_labels[0], _mqtt_event.level);
        lv_label_set_text(_event_time_labels[0], _mqtt_event.time);
        lv_label_set_text(_event_text_labels[0], _mqtt_event.text);
        lv_obj_set_style_text_color(_event_time_labels[0], _mqtt_event_color, 0);
    }
}

#if 0
void HomeCareHub::refreshCameraPreview(bool force)
{
    if (_camera_panel == nullptr || _camera_image == nullptr || _camera_status_label == nullptr) {
        return;
    }

    CameraMqttSnapshot snapshot = {};
    bool has_new_frame = camera_mqtt_receiver_get_snapshot(&snapshot, _camera_frame_version);
    if (!has_new_frame) {
        camera_mqtt_receiver_get_status(&snapshot);
    }

    const uint32_t now_ms = static_cast<uint32_t>(esp_timer_get_time() / 1000);
    static uint32_t s_last_camera_request_ms = 0;
    if (snapshot.receiver_enabled && snapshot.mqtt_connected && snapshot.jpeg_frames == 0 &&
        (force || s_last_camera_request_ms == 0 || now_ms - s_last_camera_request_ms > 5000)) {
        esp_err_t err = camera_mqtt_receiver_publish_mode(true);
        s_last_camera_request_ms = now_ms;
        ESP_LOGI(TAG, "home hub camera remote request: %s", esp_err_to_name(err));
        camera_mqtt_receiver_get_status(&snapshot);
    }

    if (!snapshot.receiver_enabled) {
        lv_label_set_text(_camera_status_label, "摄像头未启用");
        lv_obj_set_style_text_color(_camera_status_label, HUB_AMBER_COLOR, 0);
    } else if (snapshot.mqtt_connected && snapshot.decoded_frames > 0 && snapshot.width > 0 && snapshot.height > 0) {
        lv_label_set_text_fmt(_camera_status_label, "摄像头在线 · %ux%u",
                              static_cast<unsigned>(snapshot.width),
                              static_cast<unsigned>(snapshot.height));
        lv_obj_set_style_text_color(_camera_status_label, HUB_GREEN_COLOR, 0);
    } else if (snapshot.mqtt_connected) {
        lv_label_set_text(_camera_status_label, "摄像头在线 · 等待画面");
        lv_obj_set_style_text_color(_camera_status_label, HUB_GREEN_COLOR, 0);
    } else {
        lv_label_set_text(_camera_status_label, "摄像头 MQTT 未连接");
        lv_obj_set_style_text_color(_camera_status_label, HUB_AMBER_COLOR, 0);
    }

    if (!snapshot.receiver_enabled) {
        lv_label_set_text(_camera_status_label, "Camera disabled");
        lv_obj_set_style_text_color(_camera_status_label, HUB_AMBER_COLOR, 0);
    } else if (snapshot.mqtt_connected && snapshot.decoded_frames > 0 && snapshot.width > 0 && snapshot.height > 0) {
        lv_label_set_text_fmt(_camera_status_label, "JPEG OK | %ux%u",
                              static_cast<unsigned>(snapshot.width),
                              static_cast<unsigned>(snapshot.height));
        lv_obj_set_style_text_color(_camera_status_label, HUB_GREEN_COLOR, 0);
    } else if (snapshot.mqtt_connected && snapshot.status_messages > 0) {
        lv_label_set_text_fmt(_camera_status_label, "Status OK | wait JPEG | %lu B",
                              static_cast<unsigned long>(snapshot.last_jpeg_bytes));
        lv_obj_set_style_text_color(_camera_status_label, HUB_AMBER_COLOR, 0);
    } else if (snapshot.mqtt_connected) {
        lv_label_set_text_fmt(_camera_status_label, "MQTT OK | Ctrl %s | no packet",
                              snapshot.control_requests == 0 ? "pending" :
                              (snapshot.last_control_result == ESP_OK ? "OK" : "FAIL"));
        lv_obj_set_style_text_color(_camera_status_label,
                                    snapshot.last_control_result == ESP_OK ? HUB_AMBER_COLOR : HUB_GREEN_COLOR, 0);
    } else {
        lv_label_set_text(_camera_status_label, "Camera MQTT disconnected");
        lv_obj_set_style_text_color(_camera_status_label, HUB_AMBER_COLOR, 0);
    }

    if (!has_new_frame || snapshot.image == nullptr) {
        return;
    }

    _camera_frame_version = snapshot.frame_version;
    lv_img_set_src(_camera_image, snapshot.image);

    const int32_t panel_w = lv_obj_get_width(_camera_panel);
    const int32_t panel_h = lv_obj_get_height(_camera_panel);
    const int32_t usable_w = std::max<int32_t>(1, panel_w - 2);
    const int32_t usable_h = std::max<int32_t>(1, panel_h - 2);
    int32_t zoom_w = static_cast<int32_t>(usable_w * 256 / std::max<uint16_t>(1, snapshot.width));
    int32_t zoom_h = static_cast<int32_t>(usable_h * 256 / std::max<uint16_t>(1, snapshot.height));
    int32_t zoom = std::min<int32_t>(zoom_w, zoom_h);
    zoom = std::max<int32_t>(64, std::min<int32_t>(zoom, 768));
    lv_img_set_zoom(_camera_image, static_cast<uint16_t>(zoom));
    lv_obj_center(_camera_image);

    if (_camera_empty_label != nullptr) {
        lv_obj_add_flag(_camera_empty_label, LV_OBJ_FLAG_HIDDEN);
    }
    if (force) {
        lv_obj_invalidate(_camera_image);
    }
}

/* ========== LVGL 回调实现 ========== */

/**
 * @brief 场景按钮点击回调
 *
 * 从按钮的 user_data 中提取模式索引，切换本地场景并通过 MQTT 发布模式变更通知。
 */
#endif

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

/**
 * @brief 操作按钮点击回调
 *
 * 根据 user_data 中的 action ID 执行不同操作:
 * - 10~13: 设备开关切换（客厅主灯/空调/窗帘/音响）
 * - 0: 发送巡逻指令
 * - 1: 发送回充指令
 * - 2: 发送停止指令
 * - 3: 发送呼叫家人指令
 * - 4: 切换隐私模式
 */
void HomeCareHub::actionEventCb(lv_event_t *e)
{
    HomeCareHub *app = static_cast<HomeCareHub *>(lv_event_get_user_data(e));
    if (app == nullptr) {
        return;
    }
    lv_obj_t *btn = static_cast<lv_obj_t *>(lv_event_get_target(e));
    int action = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(btn)));

    /* 设备开关切换（action 10~13 对应 4 个设备） */
    if (action >= 10 && action < 14) {
        const int index = action - 10;
        if (app->_device_switches[index] != nullptr) {
            const bool now_off = lv_obj_has_state(app->_device_switches[index], LV_STATE_CHECKED);
            const bool now_enabled = now_off;
            if (now_off) {
                lv_obj_clear_state(app->_device_switches[index], LV_STATE_CHECKED);
                lv_obj_set_style_bg_color(app->_device_switches[index], HUB_CYAN_COLOR, 0);
            } else {
                lv_obj_add_state(app->_device_switches[index], LV_STATE_CHECKED);
                lv_obj_set_style_bg_color(app->_device_switches[index], HUB_FAINT_COLOR, 0);
            }
            static const char *const device_topics[] = {
                "homecare/device/light",
                "homecare/device/air_conditioner",
                "homecare/device/curtain",
                "homecare/device/speaker",
            };
            const char *payload = now_enabled ? "{\"state\":\"on\"}" : "{\"state\":\"off\"}";
            esp_err_t err = homecare_mqtt_bridge_publish_raw(device_topics[index], payload, 1, 0);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Publish device state failed: topic=%s err=%s",
                         device_topics[index], esp_err_to_name(err));
            }
        }
        return;
    }

    /* 指令按钮 */
    if (action == 0) {
        homecare_mqtt_bridge_publish_action(HOMECARE_MQTT_ACTION_PATROL);
    } else if (action == 1) {
        if (app->_smartcar_system_status != HOMECARE_MQTT_SYSTEM_STATUS_ABNORMAL_READY) {
            return;
        }
        homecare_mqtt_bridge_publish_action(HOMECARE_MQTT_ACTION_RECHARGE);
    } else if (action == 2) {
        homecare_mqtt_bridge_publish_action(HOMECARE_MQTT_ACTION_STOP);
    } else if (action >= 5 && action <= 7) {
        const homecare_mqtt_action_t place_actions[] = {
            HOMECARE_MQTT_ACTION_ABNORMAL_BATHROOM,
            HOMECARE_MQTT_ACTION_ABNORMAL_BEDROOM,
            HOMECARE_MQTT_ACTION_ABNORMAL_KITCHEN,
        };
        homecare_mqtt_bridge_publish_action(place_actions[action - 5]);
    } else if (action == 3) {
        homecare_mqtt_bridge_publish_action(HOMECARE_MQTT_ACTION_CALL_FAMILY);
        if (app->_assistant_text_label != nullptr) {
            lv_label_set_text(app->_assistant_text_label, "正在呼叫家人");
        }
    } else if (action == 4) {
        homecare_mqtt_bridge_publish_action(HOMECARE_MQTT_ACTION_PRIVACY_TOGGLE);
        app->_privacy_enabled = !app->_privacy_enabled;
        app->updateUi();
    }
}

/** @brief 分页滚动回调：根据水平滚动偏移计算当前页码 */
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

/**
 * @brief 定时器回调
 *
 * _mqtt_timer 每秒轮询 MQTT 消息队列并检测天气数据版本变更。
 */
void HomeCareHub::timerCb(lv_timer_t *timer)
{
    HomeCareHub *app = static_cast<HomeCareHub *>(timer->user_data);
    if (app == nullptr) {
        return;
    }

    if (timer != app->_mqtt_timer) {
        return;
    }
    app->applyMqttMessages();
    HomeCareWeatherSnapshot weather_snapshot = {};
    if (homecare_weather_service_get_snapshot(&weather_snapshot) &&
        weather_snapshot.revision != app->_weather_revision) {
        app->updateUi();
    }
}
