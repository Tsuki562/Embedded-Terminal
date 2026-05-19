#include <cstdio>
#include <cstdint>
#include <cstring>
#include "HomeCareHub.hpp"
#include "HomeCareWeather.hpp"
#include "HomeCareWeatherCity.hpp"

// 应用图标和中文字体资源由 LVGL 的资源编译流程生成，这里只声明外部符号。
LV_IMG_DECLARE(img_app_setting);
LV_FONT_DECLARE(homecare_font_simsun_14);
LV_FONT_DECLARE(homecare_font_simsun_16);
LV_FONT_DECLARE(homecare_font_simsun_20);
LV_FONT_DECLARE(homecare_font_simsun_28);

// HomeCareHub 的主色板：深色背景、面板层级、边框、阴影和状态提示色。
#define HUB_BG_COLOR           lv_color_hex(0x101722)
#define HUB_BG_GRAD_COLOR      lv_color_hex(0x142433)
#define HUB_PANEL_COLOR        lv_color_hex(0x112236)
#define HUB_PANEL_GRAD_COLOR   lv_color_hex(0x172C43)
#define HUB_PANEL_2_COLOR      lv_color_hex(0x0D1B2A)
#define HUB_CARD_GRAD_COLOR    lv_color_hex(0x102236)
#define HUB_LINE_COLOR         lv_color_hex(0x334155)
#define HUB_SHADOW_COLOR       lv_color_hex(0x050A12)
#define HUB_TEXT_COLOR         lv_color_hex(0xF5F7FB)
#define HUB_MUTED_COLOR        lv_color_hex(0x9AA8BA)
#define HUB_BLUE_COLOR         lv_color_hex(0x4CA3FF)
#define HUB_GREEN_COLOR        lv_color_hex(0x34D399)
#define HUB_YELLOW_COLOR       lv_color_hex(0xFBBF24)
#define HUB_ORANGE_COLOR       lv_color_hex(0xF97316)
#define HUB_RED_COLOR          lv_color_hex(0xEF4444)
#define HUB_PURPLE_COLOR       lv_color_hex(0xA78BFA)

// 统一使用本应用内置的宋体字号，避免不同控件各自硬编码字体资源。
#define HUB_FONT_TITLE         (&homecare_font_simsun_28)
#define HUB_FONT_HEAD          (&homecare_font_simsun_20)
#define HUB_FONT_BODY          (&homecare_font_simsun_16)
#define HUB_FONT_SMALL         (&homecare_font_simsun_14)
// 横向仪表盘固定为三页：区域、巡检小车、环境/事件。
#define HUB_PAGE_COUNT         (3)
// 页面之间的间距，同时用于滚动结束后计算当前页。
#define HUB_PAGE_GAP           (14)

// 构造函数只初始化成员和注册应用元信息，真正的 LVGL 对象在 run() 中创建。
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
    _mode_label(nullptr),
    _status_label(nullptr),
    _privacy_label(nullptr),
    _weather_city_label(nullptr),
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
    _sensor_label(nullptr),
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

// 析构时确保定时器释放，避免 LVGL 在应用销毁后继续回调悬空 this 指针。
HomeCareHub::~HomeCareHub()
{
    deleteTimer();
}

// 初始化 MQTT 桥接层，后续 UI 按钮和入站消息都通过该桥接层交互。
bool HomeCareHub::init(void)
{
    homecare_mqtt_bridge_init();
    homecare_weather_service_init();
    return true;
}

// 应用进入前台时创建界面、设置初始模式，并启动自动演示与 MQTT 轮询定时器。
bool HomeCareHub::run(void)
{
    // Brookesia 会把 App 屏幕裁到可视区域，布局也必须跟随这个区域，不能再强行回到 1024x600。
    lv_area_t area = getVisualArea();
    const int32_t visual_w = area.x2 >= area.x1 ? (area.x2 - area.x1 + 1) : 0;
    const int32_t visual_h = area.y2 >= area.y1 ? (area.y2 - area.y1 + 1) : 0;
    _width = visual_w > 0 ? static_cast<uint16_t>(visual_w) : static_cast<uint16_t>(lv_disp_get_hor_res(nullptr));
    _height = visual_h > 0 ? static_cast<uint16_t>(visual_h) : static_cast<uint16_t>(lv_disp_get_ver_res(nullptr));

    createUi();
    setMode(MODE_NORMAL);
    // _timer 负责周期切换演示场景，_mqtt_timer 高频轮询 MQTT 入站队列。
    _timer = lv_timer_create(timerCb, 6000, this);
    _mqtt_timer = lv_timer_create(timerCb, 1000, this);
    return true;
}

// 返回键交给系统核心处理，通知 Brookesia 当前应用请求关闭。
bool HomeCareHub::back(void)
{
    notifyCoreClosed();
    return true;
}

// 应用关闭时释放定时器和根容器，并清空缓存的 LVGL 对象指针。
bool HomeCareHub::close(void)
{
    deleteTimer();
    if (_root != nullptr) {
        lv_obj_del(_root);
        _root = nullptr;
    }
    _pages = nullptr;
    _page_dots.fill(nullptr);
    return true;
}

// 统一删除本应用创建的 LVGL 定时器，重复调用也保持安全。
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

// 创建带圆角、边框、渐变和阴影的通用面板，是三页大面板和内部卡片的基础样式。
lv_obj_t *HomeCareHub::createPanel(lv_obj_t *parent, int32_t width, int32_t height, lv_color_t bg)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_size(panel, width, height);
    lv_obj_set_style_bg_color(panel, bg, 0);
    // 外层页面面板和内层信息卡使用不同渐变色，形成视觉层级。
    const bool is_page_panel = lv_color_to32(bg) == lv_color_to32(HUB_PANEL_COLOR);
    lv_obj_set_style_bg_grad_color(panel, is_page_panel ? HUB_PANEL_GRAD_COLOR : HUB_CARD_GRAD_COLOR, 0);
    lv_obj_set_style_bg_grad_dir(panel, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_main_stop(panel, 28, 0);
    lv_obj_set_style_bg_grad_stop(panel, 230, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, HUB_LINE_COLOR, 0);
    lv_obj_set_style_border_opa(panel, LV_OPA_80, 0);
    lv_obj_set_style_border_side(panel, LV_BORDER_SIDE_FULL, 0);
    lv_obj_set_style_outline_width(panel, 1, 0);
    lv_obj_set_style_outline_color(panel, lv_color_hex(0x0B1220), 0);
    lv_obj_set_style_outline_opa(panel, LV_OPA_70, 0);
    lv_obj_set_style_shadow_width(panel, 16, 0);
    lv_obj_set_style_shadow_ofs_y(panel, 6, 0);
    lv_obj_set_style_shadow_spread(panel, 0, 0);
    lv_obj_set_style_shadow_color(panel, HUB_SHADOW_COLOR, 0);
    lv_obj_set_style_shadow_opa(panel, LV_OPA_40, 0);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_set_style_pad_top(panel, 14, 0);
    lv_obj_set_style_pad_bottom(panel, 14, 0);
    lv_obj_set_style_pad_left(panel, 14, 0);
    lv_obj_set_style_pad_right(panel, 14, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    return panel;
}

// 创建统一字体、颜色和省略模式的文本标签，长文本会以省略号收尾。
lv_obj_t *HomeCareHub::createLabel(lv_obj_t *parent, const char *text, const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_obj_set_style_text_letter_space(label, 0, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    return label;
}

// 根据风险/状态颜色高亮卡片边框、外描边和阴影，便于快速识别异常区域。
void HomeCareHub::setCardAccent(lv_obj_t *obj, lv_color_t color)
{
    lv_obj_t *accent = static_cast<lv_obj_t *>(lv_obj_get_user_data(obj));
    if (accent == nullptr) {
        accent = lv_obj_create(obj);
        lv_obj_set_user_data(obj, accent);
        lv_obj_set_style_border_width(accent, 0, 0);
        lv_obj_set_style_radius(accent, 0, 0);
        lv_obj_set_style_pad_all(accent, 0, 0);
        lv_obj_clear_flag(accent, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_move_background(accent);
    }

    lv_obj_set_size(accent, 3, lv_obj_get_height(obj));
    lv_obj_align(accent, LV_ALIGN_LEFT_MID, -14, 0);
    lv_obj_set_style_bg_color(accent, color, 0);
    lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, HUB_LINE_COLOR, 0);
    lv_obj_set_style_outline_color(obj, lv_color_hex(0x0B1220), 0);
    lv_obj_set_style_shadow_color(obj, lv_color_mix(color, HUB_SHADOW_COLOR, 18), 0);
}

// 顶部状态栏样式：横向渐变、轻边框和浅阴影，用于承载标题与全局状态。
void HomeCareHub::styleHeader(lv_obj_t *obj)
{
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x132032), 0);
    lv_obj_set_style_bg_grad_color(obj, lv_color_hex(0x1C3048), 0);
    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, HUB_LINE_COLOR, 0);
    lv_obj_set_style_border_opa(obj, LV_OPA_70, 0);
    lv_obj_set_style_radius(obj, 8, 0);
    lv_obj_set_style_pad_left(obj, 16, 0);
    lv_obj_set_style_pad_right(obj, 16, 0);
    lv_obj_set_style_pad_top(obj, 6, 0);
    lv_obj_set_style_pad_bottom(obj, 6, 0);
    lv_obj_set_style_shadow_width(obj, 14, 0);
    lv_obj_set_style_shadow_ofs_y(obj, 5, 0);
    lv_obj_set_style_shadow_color(obj, HUB_SHADOW_COLOR, 0);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_30, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

// 按钮统一样式；filled=true 表示主操作按钮，false 表示次级模式按钮。
void HomeCareHub::styleButton(lv_obj_t *obj, lv_color_t bg, bool filled)
{
    lv_obj_set_style_radius(obj, 7, 0);
    lv_obj_set_style_bg_color(obj, filled ? bg : lv_color_hex(0x17283A), 0);
    lv_obj_set_style_bg_grad_color(obj, filled ? lv_color_mix(lv_color_white(), bg, 36) : lv_color_hex(0x20364B), 0);
    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, filled ? lv_color_mix(lv_color_white(), bg, 56) : bg, 0);
    lv_obj_set_style_shadow_width(obj, filled ? 10 : 6, 0);
    lv_obj_set_style_shadow_ofs_y(obj, 4, 0);
    lv_obj_set_style_shadow_color(obj, HUB_SHADOW_COLOR, 0);
    lv_obj_set_style_shadow_opa(obj, filled ? LV_OPA_40 : LV_OPA_20, 0);
    lv_obj_set_style_bg_color(obj, lv_color_mix(lv_color_black(), bg, 64), LV_STATE_PRESSED);
    lv_obj_set_style_bg_grad_color(obj, bg, LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(obj, 2, LV_STATE_PRESSED);
    lv_obj_set_style_transform_height(obj, -1, LV_STATE_PRESSED);
}

// 进度条统一样式；主背景保持暗色，指示器使用传入颜色和横向高光渐变。
void HomeCareHub::styleBar(lv_obj_t *obj, lv_color_t color)
{
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0x0F1A28), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(obj, HUB_LINE_COLOR, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(obj, color, LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_color(obj, lv_color_mix(lv_color_white(), color, 54), LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
}

// 根据当前页码刷新底部页码指示器，传入越界页码时自动夹紧。
void HomeCareHub::updatePageIndicator(int page)
{
    if (page < 0) {
        page = 0;
    } else if (page >= HUB_PAGE_COUNT) {
        page = HUB_PAGE_COUNT - 1;
    }

    for (int i = 0; i < HUB_PAGE_COUNT; ++i) {
        if (_page_dots[i] == nullptr) {
            continue;
        }
        // 当前页拉长为胶囊形，其余页保持小圆点。
        const bool active = (i == page);
        lv_obj_set_size(_page_dots[i], active ? 22 : 8, 8);
        lv_obj_set_style_bg_color(_page_dots[i], active ? HUB_BLUE_COLOR : lv_color_hex(0x4B5563), 0);
        lv_obj_set_style_bg_opa(_page_dots[i], active ? LV_OPA_COVER : LV_OPA_70, 0);
    }
}

// 创建完整 HomeCareHub 界面：顶栏、三页横向面板、操作按钮和页码点。
bool HomeCareHub::createUi(void)
{
    // 根容器铺满可视区域，使用暗色竖向渐变作为整个应用背景。
    _root = lv_obj_create(lv_scr_act());
    lv_obj_set_size(_root, _width, _height);
    lv_obj_align(_root, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(_root, HUB_BG_COLOR, 0);
    lv_obj_set_style_bg_grad_color(_root, HUB_BG_GRAD_COLOR, 0);
    lv_obj_set_style_bg_grad_dir(_root, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_width(_root, 0, 0);
    lv_obj_set_style_radius(_root, 0, 0);
    lv_obj_set_style_pad_all(_root, 14, 0);
    lv_obj_clear_flag(_root, LV_OBJ_FLAG_SCROLLABLE);

    // 顶部栏显示应用名、当前模式、通信/AI 状态和隐私模式提示。
    lv_obj_t *top = lv_obj_create(_root);
    lv_obj_set_size(top, _width - 28, 58);
    lv_obj_align(top, LV_ALIGN_TOP_MID, 0, 0);
    styleHeader(top);

    lv_obj_t *title = createLabel(top, "家庭终端", HUB_FONT_TITLE, HUB_TEXT_COLOR);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, -8);
    lv_obj_t *subtitle = createLabel(top, "家庭安全看板", HUB_FONT_SMALL, HUB_MUTED_COLOR);
    lv_obj_align_to(subtitle, title, LV_ALIGN_OUT_BOTTOM_LEFT, 2, 2);

    _mode_label = createLabel(top, "正常巡检", HUB_FONT_HEAD, HUB_GREEN_COLOR);
    lv_obj_set_style_radius(_mode_label, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(_mode_label, HUB_GREEN_COLOR, 0);
    lv_obj_set_style_bg_opa(_mode_label, LV_OPA_20, 0);
    lv_obj_set_style_border_width(_mode_label, 1, 0);
    lv_obj_set_style_border_color(_mode_label, HUB_GREEN_COLOR, 0);
    lv_obj_set_style_pad_left(_mode_label, 10, 0);
    lv_obj_set_style_pad_right(_mode_label, 10, 0);
    lv_obj_set_style_pad_top(_mode_label, 3, 0);
    lv_obj_set_style_pad_bottom(_mode_label, 3, 0);
    lv_obj_align(_mode_label, LV_ALIGN_RIGHT_MID, -360, -8);
    _status_label = createLabel(top, "WiFi-CSI 在线 | 本地 AI 就绪", HUB_FONT_SMALL, HUB_MUTED_COLOR);
    lv_obj_align_to(_status_label, _mode_label, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 6);
    _privacy_label = createLabel(top, "隐私：仅 CSI 感知", HUB_FONT_BODY, HUB_GREEN_COLOR);
    lv_obj_set_style_radius(_privacy_label, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(_privacy_label, HUB_GREEN_COLOR, 0);
    lv_obj_set_style_bg_opa(_privacy_label, LV_OPA_20, 0);
    lv_obj_set_style_border_width(_privacy_label, 1, 0);
    lv_obj_set_style_border_color(_privacy_label, HUB_GREEN_COLOR, 0);
    lv_obj_set_style_pad_left(_privacy_label, 10, 0);
    lv_obj_set_style_pad_right(_privacy_label, 10, 0);
    lv_obj_set_style_pad_top(_privacy_label, 4, 0);
    lv_obj_set_style_pad_bottom(_privacy_label, 4, 0);
    lv_obj_align(_privacy_label, LV_ALIGN_RIGHT_MID, 0, 0);

    // 内容区扣除顶部栏和底部页码点高度，三页共享同一尺寸。
    const int content_y = 72;
    const int page_w = _width - 28;
    const int page_h = _height - content_y - 44;

    // 横向滚动容器负责三页之间的滑动和吸附。
    _pages = lv_obj_create(_root);
    lv_obj_set_size(_pages, page_w, page_h);
    lv_obj_align(_pages, LV_ALIGN_TOP_MID, 0, content_y);
    lv_obj_set_style_bg_opa(_pages, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_pages, 0, 0);
    lv_obj_set_style_radius(_pages, 0, 0);
    lv_obj_set_style_pad_all(_pages, 0, 0);
    lv_obj_set_style_pad_column(_pages, HUB_PAGE_GAP, 0);
    lv_obj_set_scrollbar_mode(_pages, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(_pages, LV_DIR_HOR);
    lv_obj_set_scroll_snap_x(_pages, LV_SCROLL_SNAP_CENTER);
    lv_obj_add_flag(_pages, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ONE);
    lv_obj_clear_flag(_pages, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_flex_flow(_pages, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(_pages, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_add_event_cb(_pages, scrollEventCb, LV_EVENT_SCROLL_END, this);

    // 预先创建三个透明页面容器，后续每页内部再放置真实内容面板。
    std::array<lv_obj_t *, HUB_PAGE_COUNT> page_objs = {};
    for (int i = 0; i < HUB_PAGE_COUNT; ++i) {
        lv_obj_t *page = lv_obj_create(_pages);
        lv_obj_set_size(page, page_w, page_h);
        lv_obj_set_style_bg_opa(page, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(page, 0, 0);
        lv_obj_set_style_radius(page, 0, 0);
        lv_obj_set_style_pad_all(page, 0, 0);
        lv_obj_add_flag(page, LV_OBJ_FLAG_SNAPPABLE);
        lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
        page_objs[i] = page;
    }

    // 第 1 页：居家区域感知，四张卡分别展示房间活动、风险等级和 CSI 强度。
    lv_obj_t *left = createPanel(page_objs[0], page_w, page_h, HUB_PANEL_COLOR);
    lv_obj_align(left, LV_ALIGN_TOP_LEFT, 0, 0);
    createLabel(left, "居家区域", HUB_FONT_HEAD, HUB_TEXT_COLOR);

    const char *room_names[] = {"客厅", "卧室", "浴室", "走廊"};
    const int room_gap = 16;
    const int room_card_w = (page_w - 24 - room_gap) / 2;
    const int room_card_h = (page_h - 78 - room_gap) / 2;
    for (int i = 0; i < 4; ++i) {
        // 2x2 房间网格，索引 0/1 在第一行，2/3 在第二行。
        lv_obj_t *card = createPanel(left, room_card_w, room_card_h, HUB_PANEL_2_COLOR);
        lv_obj_align(card, LV_ALIGN_TOP_LEFT,
                     (i % 2) * (room_card_w + room_gap),
                     44 + (i / 2) * (room_card_h + room_gap));
        _room_cards[i] = card;
        _room_name_labels[i] = createLabel(card, room_names[i], HUB_FONT_BODY, HUB_TEXT_COLOR);
        lv_obj_align(_room_name_labels[i], LV_ALIGN_TOP_LEFT, 0, 0);
        _room_activity_labels[i] = createLabel(card, "空闲", HUB_FONT_SMALL, HUB_MUTED_COLOR);
        lv_obj_set_width(_room_activity_labels[i], room_card_w - 28);
        lv_obj_align(_room_activity_labels[i], LV_ALIGN_TOP_LEFT, 0, 42);
        _room_risk_labels[i] = createLabel(card, "风险 L0", HUB_FONT_SMALL, HUB_GREEN_COLOR);
        lv_obj_align(_room_risk_labels[i], LV_ALIGN_TOP_RIGHT, 0, 0);
        _room_csi_labels[i] = createLabel(card, "CSI 20%", HUB_FONT_SMALL, HUB_MUTED_COLOR);
        lv_obj_align(_room_csi_labels[i], LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    }

    // 第 2 页：巡检小车状态，包含位置、目标、阶段、电量、路线和四个操作按钮。
    lv_obj_t *center = createPanel(page_objs[1], page_w, page_h, HUB_PANEL_COLOR);
    lv_obj_align(center, LV_ALIGN_TOP_LEFT, 0, 0);
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
    lv_obj_set_size(_battery_bar, page_w - 150, 14);
    lv_obj_align(_battery_bar, LV_ALIGN_TOP_LEFT, 0, 198);
    lv_bar_set_range(_battery_bar, 0, 100);
    styleBar(_battery_bar, HUB_GREEN_COLOR);
    _battery_label = createLabel(center, "86%", HUB_FONT_SMALL, HUB_TEXT_COLOR);
    lv_obj_align(_battery_label, LV_ALIGN_TOP_RIGHT, 0, 190);

    lv_obj_t *route_title = createLabel(center, "路线", HUB_FONT_SMALL, HUB_MUTED_COLOR);
    lv_obj_align(route_title, LV_ALIGN_TOP_LEFT, 0, 228);
    _route_bar = lv_bar_create(center);
    lv_obj_set_size(_route_bar, page_w - 150, 14);
    lv_obj_align(_route_bar, LV_ALIGN_TOP_LEFT, 0, 250);
    lv_bar_set_range(_route_bar, 0, 100);
    styleBar(_route_bar, HUB_BLUE_COLOR);
    _route_label = createLabel(center, "充电桩 > 走廊 > 点位", HUB_FONT_SMALL, HUB_MUTED_COLOR);
    lv_obj_align(_route_label, LV_ALIGN_TOP_LEFT, 0, 272);

    _obstacle_label = createLabel(center, "障碍：通畅", HUB_FONT_BODY, HUB_TEXT_COLOR);
    lv_obj_align(_obstacle_label, LV_ALIGN_TOP_LEFT, 0, 314);
    _sensor_label = createLabel(center, "传感器：CSI 在线", HUB_FONT_BODY, HUB_TEXT_COLOR);
    lv_obj_align(_sensor_label, LV_ALIGN_TOP_LEFT, 0, 344);
    _voice_label = createLabel(center, "语音：待命", HUB_FONT_BODY, HUB_TEXT_COLOR);
    lv_obj_align(_voice_label, LV_ALIGN_TOP_LEFT, 0, 374);

    lv_obj_t *actions = lv_obj_create(center);
    lv_obj_set_size(actions, page_w - 24, 42);
    lv_obj_align(actions, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(actions, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(actions, 0, 0);
    lv_obj_set_style_pad_all(actions, 0, 0);
    lv_obj_set_flex_flow(actions, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(actions, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(actions, LV_OBJ_FLAG_SCROLLABLE);

    // 操作按钮的 user_data 保存动作编号，由 actionEventCb 统一解析并发布 MQTT 动作。
    const char *button_texts[] = {"巡检", "回充", "隐私", "呼叫"};
    const char *button_icons[] = {LV_SYMBOL_REFRESH, LV_SYMBOL_HOME, LV_SYMBOL_EYE_OPEN, LV_SYMBOL_CALL};
    for (int i = 0; i < 4; ++i) {
        lv_obj_t *btn = lv_btn_create(actions);
        lv_obj_set_size(btn, 120, 38);
        styleButton(btn, i == 2 ? HUB_PURPLE_COLOR : HUB_BLUE_COLOR, true);
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(btn, 6, 0);
        lv_obj_add_event_cb(btn, actionEventCb, LV_EVENT_CLICKED, this);
        lv_obj_set_user_data(btn, reinterpret_cast<void *>(static_cast<intptr_t>(i)));
        lv_obj_t *icon = lv_label_create(btn);
        lv_label_set_text(icon, button_icons[i]);
        lv_obj_set_style_text_font(icon, LV_FONT_DEFAULT, 0);
        lv_obj_set_style_text_color(icon, lv_color_white(), 0);
        lv_obj_t *label = createLabel(btn, button_texts[i], HUB_FONT_SMALL, lv_color_white());
    }

    // 第 3 页：天气、室内环境和事件记录，同时提供模式切换按钮。
    lv_obj_t *right = createPanel(page_objs[2], page_w, page_h, HUB_PANEL_COLOR);
    lv_obj_align(right, LV_ALIGN_TOP_LEFT, 0, 0);
    // 左侧 42% 放天气/环境卡，右侧放事件列表，底部保留模式按钮区。
    const int third_gap = 16;
    const int third_action_h = 42;
    const int third_content_h = page_h - third_action_h - third_gap;
    const int third_left_w = (page_w - 24 - third_gap) * 42 / 100;
    const int third_right_w = page_w - 24 - third_gap - third_left_w;
    const int info_h = (third_content_h - third_gap) / 2;

    // 天气卡展示户外天气、温度、湿度和空气质量。
    lv_obj_t *weather = createPanel(right, third_left_w, info_h, HUB_PANEL_2_COLOR);
    lv_obj_align(weather, LV_ALIGN_TOP_LEFT, 0, 0);
    createLabel(weather, "天气", HUB_FONT_HEAD, HUB_TEXT_COLOR);
    _weather_city_label = createLabel(weather, homecare_weather_city_get_selected_name(), HUB_FONT_SMALL, HUB_MUTED_COLOR);
    lv_obj_align(_weather_city_label, LV_ALIGN_TOP_RIGHT, 0, 4);
    _weather_label = createLabel(weather, "多云", HUB_FONT_TITLE, HUB_BLUE_COLOR);
    lv_obj_align(_weather_label, LV_ALIGN_TOP_LEFT, 0, 42);
    _outdoor_label = createLabel(weather, "室外 24C", HUB_FONT_BODY, HUB_TEXT_COLOR);
    lv_obj_t *weather_icon = lv_label_create(weather);
    lv_label_set_text(weather_icon, LV_SYMBOL_HOME);
    lv_obj_set_style_text_font(weather_icon, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_color(weather_icon, HUB_BLUE_COLOR, 0);
    lv_obj_align(weather_icon, LV_ALIGN_TOP_LEFT, 0, 103);
    lv_obj_align(_outdoor_label, LV_ALIGN_TOP_LEFT, 26, 100);
    _humidity_label = createLabel(weather, "湿度 58%", HUB_FONT_BODY, HUB_TEXT_COLOR);
    lv_obj_t *humidity_icon = lv_label_create(weather);
    lv_label_set_text(humidity_icon, LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_font(humidity_icon, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_color(humidity_icon, HUB_PURPLE_COLOR, 0);
    lv_obj_align(humidity_icon, LV_ALIGN_TOP_LEFT, 0, 133);
    lv_obj_align(_humidity_label, LV_ALIGN_TOP_LEFT, 26, 130);
    _air_label = createLabel(weather, "空气 良好", HUB_FONT_BODY, HUB_GREEN_COLOR);
    lv_obj_t *air_icon = lv_label_create(weather);
    lv_label_set_text(air_icon, LV_SYMBOL_OK);
    lv_obj_set_style_text_font(air_icon, LV_FONT_DEFAULT, 0);
    lv_obj_set_style_text_color(air_icon, HUB_GREEN_COLOR, 0);
    lv_obj_align(air_icon, LV_ALIGN_BOTTOM_LEFT, 0, -2);
    lv_obj_align(_air_label, LV_ALIGN_BOTTOM_LEFT, 26, 0);

    // 室内环境卡展示当前室内温湿度和夜间风险提示。
    lv_obj_t *env = createPanel(right, third_left_w, info_h, HUB_PANEL_2_COLOR);
    lv_obj_align_to(env, weather, LV_ALIGN_OUT_BOTTOM_LEFT, 0, third_gap);
    createLabel(env, "室内环境", HUB_FONT_HEAD, HUB_TEXT_COLOR);
    _indoor_label = createLabel(env, "室内 25C / 51%", HUB_FONT_BODY, HUB_TEXT_COLOR);
    lv_obj_align(_indoor_label, LV_ALIGN_TOP_LEFT, 0, 52);
    _night_label = createLabel(env, "夜间风险低", HUB_FONT_BODY, HUB_MUTED_COLOR);
    lv_obj_set_width(_night_label, third_left_w - 28);
    lv_obj_align(_night_label, LV_ALIGN_TOP_LEFT, 0, 92);

    // 事件区域固定显示四条最近事件；MQTT 事件会覆盖第一条。
    lv_obj_t *event_area = createPanel(right, third_right_w, third_content_h, HUB_PANEL_2_COLOR);
    lv_obj_align_to(event_area, weather, LV_ALIGN_OUT_RIGHT_TOP, third_gap, 0);
    lv_obj_t *event_title = createLabel(event_area, "事件记录", HUB_FONT_HEAD, HUB_TEXT_COLOR);
    lv_obj_align(event_title, LV_ALIGN_TOP_LEFT, 0, 0);

    const int event_gap = 10;
    const int event_w = third_right_w - 28;
    const int event_h = (third_content_h - 56 - event_gap * 3) / 4;
    for (int i = 0; i < 4; ++i) {
        // 每条事件拆成等级、时间和描述三段，便于按等级单独着色。
        lv_obj_t *event_row = lv_obj_create(event_area);
        lv_obj_t *event = event_row;
        lv_obj_set_size(event_row, event_w, event_h);
        lv_obj_align(event_row, LV_ALIGN_TOP_LEFT, 0, 42 + i * (event_h + event_gap));
        lv_obj_set_style_bg_opa(event_row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(event_row, 0, 0);
        lv_obj_set_style_radius(event_row, 0, 0);
        lv_obj_set_style_pad_all(event_row, 0, 0);
        lv_obj_clear_flag(event_row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t *event_dot = lv_obj_create(event_row);
        lv_obj_set_size(event_dot, 6, 6);
        lv_obj_set_style_radius(event_dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(event_dot, 0, 0);
        lv_obj_set_style_bg_color(event_dot, HUB_GREEN_COLOR, 0);
        lv_obj_set_style_bg_opa(event_dot, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_all(event_dot, 0, 0);
        lv_obj_clear_flag(event_dot, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_align(event_dot, LV_ALIGN_LEFT_MID, 0, 0);
        _event_cards[i] = event_dot;
        _event_level_labels[i] = createLabel(event, "L0", HUB_FONT_SMALL, HUB_GREEN_COLOR);
        lv_obj_set_style_radius(_event_level_labels[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(_event_level_labels[i], HUB_GREEN_COLOR, 0);
        lv_obj_set_style_bg_opa(_event_level_labels[i], LV_OPA_20, 0);
        lv_obj_set_style_border_width(_event_level_labels[i], 1, 0);
        lv_obj_set_style_border_color(_event_level_labels[i], HUB_GREEN_COLOR, 0);
        lv_obj_set_style_pad_left(_event_level_labels[i], 8, 0);
        lv_obj_set_style_pad_right(_event_level_labels[i], 8, 0);
        lv_obj_set_style_pad_top(_event_level_labels[i], 2, 0);
        lv_obj_set_style_pad_bottom(_event_level_labels[i], 2, 0);
        lv_obj_align(_event_level_labels[i], LV_ALIGN_RIGHT_MID, 0, 0);
        _event_time_labels[i] = createLabel(event, "08:30", HUB_FONT_SMALL, HUB_MUTED_COLOR);
        lv_obj_align(_event_time_labels[i], LV_ALIGN_BOTTOM_LEFT, 20, -2);
        _event_text_labels[i] = createLabel(event, "正常巡检", HUB_FONT_SMALL, HUB_TEXT_COLOR);
        lv_obj_set_width(_event_text_labels[i], event_w - 96);
        lv_obj_align(_event_text_labels[i], LV_ALIGN_TOP_LEFT, 20, 2);
    }

    lv_obj_t *modes = lv_obj_create(right);
    lv_obj_set_size(modes, page_w - 24, 42);
    lv_obj_align(modes, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(modes, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(modes, 0, 0);
    lv_obj_set_style_pad_all(modes, 0, 0);
    lv_obj_set_flex_flow(modes, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(modes, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(modes, LV_OBJ_FLAG_SCROLLABLE);

    // 模式按钮的 user_data 保存 DemoMode 枚举值，点击后同步 UI 并发布 MQTT 模式。
    const char *mode_texts[] = {"正常", "跌倒", "浴室", "夜间"};
    for (int i = 0; i < MODE_MAX; ++i) {
        lv_obj_t *btn = lv_btn_create(modes);
        lv_obj_set_size(btn, 120, 36);
        styleButton(btn, HUB_BLUE_COLOR, false);
        lv_obj_add_event_cb(btn, scenarioEventCb, LV_EVENT_CLICKED, this);
        lv_obj_set_user_data(btn, reinterpret_cast<void *>(static_cast<intptr_t>(i)));
        lv_obj_t *label = createLabel(btn, mode_texts[i], HUB_FONT_SMALL, HUB_TEXT_COLOR);
        lv_obj_center(label);
    }

    // 底部页码点用于提示横向分页位置。
    lv_obj_t *dots = lv_obj_create(_root);
    lv_obj_set_size(dots, 86, 14);
    lv_obj_align(dots, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_obj_set_style_bg_opa(dots, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(dots, 0, 0);
    lv_obj_set_style_pad_all(dots, 0, 0);
    lv_obj_set_flex_flow(dots, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(dots, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(dots, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < HUB_PAGE_COUNT; ++i) {
        // 页码点本身不滚动，只由 scrollEventCb/updatePageIndicator 改变尺寸和颜色。
        lv_obj_t *dot = lv_obj_create(dots);
        lv_obj_set_size(dot, 8, 8);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_set_style_pad_all(dot, 0, 0);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
        _page_dots[i] = dot;
    }
    updatePageIndicator(0);

    return true;
}

// 切换本地演示模式，并立即用对应 DashboardState 刷新所有绑定控件。
void HomeCareHub::setMode(DemoMode mode)
{
    _mode = mode;
    updateUi();
}

// 将本地 DemoMode 转成 MQTT 协议使用的模式枚举。
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

// 将 MQTT 协议模式转回本地 DemoMode；未知值保持当前模式不变。
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

// 应用单条 MQTT 入站消息：可远程切换模式，也可插入最新事件。
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
        // 保存最新事件；空字段用默认值补齐，避免 UI 出现空白。
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

        // 事件等级映射到卡片强调色，L3 最高风险，L0/未知按绿色展示。
        if (std::strcmp(_mqtt_event.level, "L3") == 0) {
            _mqtt_event_color = HUB_RED_COLOR;
        } else if (std::strcmp(_mqtt_event.level, "L2") == 0) {
            _mqtt_event_color = HUB_ORANGE_COLOR;
        } else if (std::strcmp(_mqtt_event.level, "L1") == 0) {
            _mqtt_event_color = HUB_BLUE_COLOR;
        } else {
            _mqtt_event_color = HUB_GREEN_COLOR;
        }
    }

    updateUi();
}

// 从 MQTT 桥接队列中取尽当前所有入站消息，保证 UI 使用最新状态。
void HomeCareHub::applyMqttMessages(void)
{
    HomeCareMqttInboundMessage message = {};
    while (homecare_mqtt_bridge_receive(&message)) {
        applyMqttMessage(message);
    }
}

// 根据当前模式的静态状态表和可选 MQTT 事件刷新全部界面控件。
void HomeCareHub::updateUi(void)
{
    // 四个演示场景的完整仪表盘快照：全局状态、环境、小车、房间和事件。
    const DashboardState states[MODE_MAX] = {
        {
            "正常巡检", "WiFi-CSI 在线 | 本地 AI 就绪", "隐私：仅 CSI 感知",
            "多云", "室外 24C", "湿度 58%", "空气 良好", "室内 25C / 51%", "夜间风险低",
            "待命中", "充电桩", "客厅巡检点", "CSI 看护", "障碍：通畅", "传感器：CSI 在线", "语音：待命",
            86, 12,
            {{
                {"客厅", "有人且稳定", "风险 L0", 31, true, false, HUB_GREEN_COLOR},
                {"卧室", "休息区安静", "风险 L0", 18, false, true, HUB_GREEN_COLOR},
                {"浴室", "未见久留", "风险 L0", 14, false, true, HUB_GREEN_COLOR},
                {"走廊", "通道清", "风险 L0", 20, false, false, HUB_GREEN_COLOR},
            }},
            {{
                {"L0", "08:30", "日常巡检完成", HUB_GREEN_COLOR},
                {"L0", "08:18", "仅 CSI 感知", HUB_GREEN_COLOR},
                {"L1", "07:42", "卧室轻微活动", HUB_BLUE_COLOR},
                {"L0", "07:20", "小车已回充", HUB_GREEN_COLOR},
            }},
        },
        {
            "疑似跌倒", "CSI 高动态 | 小车出发", "隐私：仅用 CSI 确认",
            "多云", "室外 24C", "湿度 58%", "空气 良好", "室内 25C / 51%", "优先确认客厅",
            "前往现场", "走廊", "客厅巡检点", "CSI 复核", "障碍：已绕行", "传感器：CSI 增强", "语音：询问用户",
            78, 74,
            {{
                {"客厅", "高动态后静止", "风险 L3", 88, true, false, HUB_RED_COLOR},
                {"卧室", "无活动", "风险 L0", 12, false, true, HUB_GREEN_COLOR},
                {"浴室", "清", "风险 L0", 10, false, true, HUB_GREEN_COLOR},
                {"走廊", "小车经过", "风险 L1", 45, true, false, HUB_BLUE_COLOR},
            }},
            {{
                {"L3", "22:31", "CSI 复核疑似跌倒", HUB_RED_COLOR},
                {"L2", "22:30", "小车前往客厅", HUB_ORANGE_COLOR},
                {"L2", "22:30", "CSI 疑似跌倒", HUB_ORANGE_COLOR},
                {"L0", "22:15", "夜间巡检待命", HUB_GREEN_COLOR},
            }},
        },
        {
            "浴室久留", "CSI 久留计时 | 门区保护", "隐私：浴室无图像",
            "小雨", "室外 22C", "湿度 76%", "空气 良好", "室内 24C / 55%", "超时前先观察",
            "门外等待", "浴室门口", "浴室门口", "语音询问", "障碍：通畅", "传感器：门区 CSI", "语音：轻声询问",
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
                {"L0", "20:55", "隐私区无图像采集", HUB_GREEN_COLOR},
            }},
        },
        {
            "夜间离床", "CSI 离床 | 低速跟随", "隐私：仅 CSI 感知",
            "晴夜", "室外 19C", "湿度 62%", "空气 优秀", "室内 23C / 49%", "引导灯并看护走廊",
            "低速跟随", "卧室门口", "走廊点位", "夜间陪护", "障碍：通畅", "传感器：CSI 在线", "语音：待命",
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

    // 先选择当前模式对应的快照，再逐个写入 LVGL 标签、进度条和卡片样式。
    const DashboardState &state = states[_mode];
    lv_label_set_text(_mode_label, state.mode_name);
    lv_color_t mode_color = _mode == MODE_FALL ? HUB_RED_COLOR : (_mode == MODE_BATHROOM ? HUB_ORANGE_COLOR : HUB_GREEN_COLOR);
    lv_obj_set_style_text_color(_mode_label, mode_color, 0);
    lv_obj_set_style_bg_color(_mode_label, mode_color, 0);
    lv_obj_set_style_border_color(_mode_label, mode_color, 0);
    lv_label_set_text(_status_label, state.home_status);
    lv_label_set_text(_privacy_label, _privacy_enabled ? state.privacy : "隐私：手动巡检模式");
    lv_color_t privacy_color = _privacy_enabled ? HUB_GREEN_COLOR : HUB_YELLOW_COLOR;
    lv_obj_set_style_text_color(_privacy_label, privacy_color, 0);
    lv_obj_set_style_bg_color(_privacy_label, privacy_color, 0);
    lv_obj_set_style_border_color(_privacy_label, privacy_color, 0);

    lv_label_set_text(_weather_label, state.weather);
    lv_label_set_text(_outdoor_label, state.outdoor_temp);
    lv_label_set_text(_humidity_label, state.humidity);
    lv_label_set_text(_air_label, state.air_quality);
    lv_label_set_text(_indoor_label, state.indoor_env);
    lv_label_set_text(_night_label, state.night_hint);

    HomeCareWeatherSnapshot weather_snapshot = {};
    if (homecare_weather_service_get_snapshot(&weather_snapshot)) {
        lv_label_set_text(_weather_city_label, weather_snapshot.city);
        lv_label_set_text(_weather_label, weather_snapshot.weather);
        lv_label_set_text(_outdoor_label, weather_snapshot.outdoor_temp);
        lv_label_set_text(_humidity_label, weather_snapshot.humidity);
        lv_label_set_text(_air_label, weather_snapshot.air_quality);
        lv_obj_set_style_text_color(_weather_label,
                                    weather_snapshot.has_live_data ? HUB_BLUE_COLOR : HUB_MUTED_COLOR, 0);
        lv_color_t air_color = HUB_MUTED_COLOR;
        if (weather_snapshot.air_quality_level == 1) {
            air_color = HUB_GREEN_COLOR;
        } else if (weather_snapshot.air_quality_level == 2) {
            air_color = HUB_BLUE_COLOR;
        } else if (weather_snapshot.air_quality_level == 3) {
            air_color = HUB_YELLOW_COLOR;
        } else if (weather_snapshot.air_quality_level >= 4) {
            air_color = HUB_RED_COLOR;
        }
        lv_obj_set_style_text_color(_air_label, air_color, 0);
        _weather_revision = weather_snapshot.revision;
    }

    // 刷新巡检小车区域：状态文字、路径、电量和路线进度。
    lv_label_set_text(_car_status_label, state.car_status);
    lv_obj_set_style_text_color(_car_status_label, _mode == MODE_FALL ? HUB_RED_COLOR : (_mode == MODE_BATHROOM ? HUB_ORANGE_COLOR : HUB_GREEN_COLOR), 0);
    lv_label_set_text_fmt(_car_position_label, "位置：%s", state.car_position);
    lv_label_set_text_fmt(_car_target_label, "目标：%s", state.car_target);
    lv_label_set_text_fmt(_car_phase_label, "阶段：%s", state.car_phase);
    lv_label_set_text(_obstacle_label, state.obstacle);
    lv_label_set_text(_sensor_label, state.sensor);
    lv_label_set_text(_voice_label, state.voice);
    lv_bar_set_value(_battery_bar, state.battery, LV_ANIM_ON);
    lv_label_set_text_fmt(_battery_label, "%d%%", state.battery);
    lv_bar_set_value(_route_bar, state.route_progress, LV_ANIM_ON);
    lv_label_set_text_fmt(_route_label, "%s > %s", state.car_position, state.car_target);

    if (_has_smartcar_attitude && _smartcar_attitude.valid) {
        lv_label_set_text(_car_status_label, "姿态回传中");
        lv_obj_set_style_text_color(_car_status_label, HUB_BLUE_COLOR, 0);
        lv_label_set_text(_car_position_label, "位置：MQTT 实时上报");
        lv_label_set_text(_car_target_label, "目标：smartcar/attitude");
        lv_label_set_text(_car_phase_label,
                          _smartcar_attitude.has_mag ? "阶段：姿态+磁航向在线" : "阶段：姿态在线");
        if (_smartcar_attitude.timestamp_ms > 0) {
            lv_label_set_text_fmt(_obstacle_label, "时间戳：%lld", _smartcar_attitude.timestamp_ms);
        } else {
            lv_label_set_text(_obstacle_label, "时间戳：MQTT");
        }
        lv_label_set_text_fmt(_sensor_label, "传感器：r=%.2f p=%.2f y=%.2f",
                              _smartcar_attitude.roll_deg,
                              _smartcar_attitude.pitch_deg,
                              _smartcar_attitude.yaw_deg);
    }

    for (int i = 0; i < 4; ++i) {
        // 刷新四个房间卡：名称、活动说明、风险等级、CSI 百分比和强调色。
        const RoomState &room = state.rooms[i];
        lv_label_set_text(_room_name_labels[i], room.name);
        lv_label_set_text_fmt(_room_activity_labels[i], "%s%s", room.activity, room.privacy_zone ? " | 私密" : "");
        lv_label_set_text(_room_risk_labels[i], room.risk);
        lv_obj_set_style_text_color(_room_risk_labels[i], room.accent, 0);
        lv_label_set_text_fmt(_room_csi_labels[i], "CSI %d%%", room.csi);
        setCardAccent(_room_cards[i], room.accent);
    }

    for (int i = 0; i < 4; ++i) {
        // 刷新四条预设事件，并按事件等级颜色设置卡片强调样式。
        const EventState &event = state.events[i];
        lv_label_set_text(_event_level_labels[i], event.level);
        lv_label_set_text(_event_time_labels[i], event.time);
        lv_label_set_text(_event_text_labels[i], event.text);
        lv_obj_set_style_text_color(_event_level_labels[i], event.accent, 0);
        lv_obj_set_style_bg_color(_event_level_labels[i], event.accent, 0);
        lv_obj_set_style_bg_opa(_event_level_labels[i], LV_OPA_20, 0);
        lv_obj_set_style_border_color(_event_level_labels[i], event.accent, 0);
        lv_obj_set_style_bg_color(_event_cards[i], event.accent, 0);
    }

    if (_has_mqtt_event) {
        // 如果收到过 MQTT 事件，则用最新外部事件覆盖事件列表第一条。
        lv_label_set_text(_event_level_labels[0], _mqtt_event.level);
        lv_label_set_text(_event_time_labels[0], _mqtt_event.time);
        lv_label_set_text(_event_text_labels[0], _mqtt_event.text);
        lv_obj_set_style_text_color(_event_level_labels[0], _mqtt_event_color, 0);
        lv_obj_set_style_bg_color(_event_level_labels[0], _mqtt_event_color, 0);
        lv_obj_set_style_bg_opa(_event_level_labels[0], LV_OPA_20, 0);
        lv_obj_set_style_border_color(_event_level_labels[0], _mqtt_event_color, 0);
        lv_obj_set_style_bg_color(_event_cards[0], _mqtt_event_color, 0);
    }
}

// 模式按钮回调：读取按钮上保存的模式编号，切换 UI 并发布模式到 MQTT。
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

// 操作按钮回调：发布巡检、回充、隐私切换、呼叫家人等动作。
void HomeCareHub::actionEventCb(lv_event_t *e)
{
    HomeCareHub *app = static_cast<HomeCareHub *>(lv_event_get_user_data(e));
    if (app == nullptr) {
        return;
    }
    lv_obj_t *btn = static_cast<lv_obj_t *>(lv_event_get_target(e));
    // 动作编号来自 createUi() 中写入按钮 user_data 的索引。
    int action = static_cast<int>(reinterpret_cast<intptr_t>(lv_obj_get_user_data(btn)));

    if (action == 0) {
        // 巡检：发布巡检动作，并回到正常演示场景。
        homecare_mqtt_bridge_publish_action(HOMECARE_MQTT_ACTION_PATROL);
        app->setMode(MODE_NORMAL);
    } else if (action == 1) {
        // 回充：发布回充动作，并立即给 UI 一个本地反馈。
        homecare_mqtt_bridge_publish_action(HOMECARE_MQTT_ACTION_RECHARGE);
        app->setMode(MODE_NORMAL);
        lv_label_set_text(app->_car_status_label, "回充中");
        lv_label_set_text(app->_car_phase_label, "阶段：回充指令");
        lv_bar_set_value(app->_route_bar, 30, LV_ANIM_ON);
    } else if (action == 3) {
        // 呼叫：通知外部系统呼叫家人，同时更新语音状态。
        homecare_mqtt_bridge_publish_action(HOMECARE_MQTT_ACTION_CALL_FAMILY);
        lv_label_set_text(app->_voice_label, "语音：呼叫家人");
    } else {
        // 隐私：切换本地隐私标志，并发布给外部系统同步。
        homecare_mqtt_bridge_publish_action(HOMECARE_MQTT_ACTION_PRIVACY_TOGGLE);
        app->_privacy_enabled = !app->_privacy_enabled;
        app->updateUi();
    }
}

// 横向分页滚动结束后，根据 scroll_x 计算当前页并刷新底部页码点。
void HomeCareHub::scrollEventCb(lv_event_t *e)
{
    HomeCareHub *app = static_cast<HomeCareHub *>(lv_event_get_user_data(e));
    if (app == nullptr || app->_pages == nullptr) {
        return;
    }

    const int page_w = app->_width - 28;
    const int stride = page_w + HUB_PAGE_GAP;
    if (stride <= 0) {
        return;
    }

    // 加 stride/2 实现四舍五入到最近页，而不是简单向下取整。
    int page = (lv_obj_get_scroll_x(app->_pages) + stride / 2) / stride;
    app->updatePageIndicator(page);
}

// LVGL 定时器共用回调：MQTT 定时器处理消息，演示定时器循环切换场景。
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
    // 非 MQTT 定时器即演示轮播定时器，按枚举顺序进入下一个场景。
    int next = (static_cast<int>(app->_mode) + 1) % MODE_MAX;
    app->setMode(static_cast<DemoMode>(next));
}
