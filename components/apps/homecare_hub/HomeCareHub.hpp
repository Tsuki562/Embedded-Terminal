/**
 * @file HomeCareHub.hpp
 * @brief 家庭智能中控主界面应用头文件
 *
 * HomeCareHub 继承自 ESP_Brookesia_PhoneApp，是 ESP-Brookesia 手机框架下的
 * 一个全屏应用。它实现了智能家居仪表盘 UI，包含：
 * - 顶部状态栏（时间、日期、天气、隐私模式）
 * - 左侧面板（天气卡片、房间状态列表、自动化日程）
 * - 中央面板（场景模式切换、舒适度环形图表、实时监控）
 * - 右侧面板（常用设备控制、门厅安防摄像头、语音助手）
 *
 * 支持 MQTT 远程控制场景切换和事件接收，以及后台天气数据刷新。
 */
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

    /** @brief 启动应用：获取可视区域尺寸，创建 UI，启动定时刷新 */
    bool run(void);
    /** @brief 返回键处理：通知框架关闭应用 */
    bool back(void);
    /** @brief 关闭应用：销毁定时器和所有 UI 资源，释放指针 */
    bool close(void);
    /** @brief 应用初始化：初始化 MQTT 桥接和天气服务 */
    bool init(void) override;

private:

    /* ========== 场景模式枚举 ========== */

    /**
     * @enum DemoMode
     * @brief 应用的四种演示/工作场景模式
     *
     * 每种模式对应不同的仪表盘数据和 UI 配色方案。
     */
    enum DemoMode {
        MODE_NORMAL = 0,  /**< 日常模式：全屋安全，绿色主色调 */
        MODE_FALL,        /**< 异常复核模式：跌倒/高动态检测，珊瑚色警示 */
        MODE_BATHROOM,    /**< 浴室看护模式：久留检测与隐私保护，琥珀色 */
        MODE_NIGHT,       /**< 夜间守护模式：离床检测与低速跟随，蓝色 */
        MODE_MAX,         /**< 模式数量（用于数组大小和模运算） */
    };

    /* ========== UI 数据结构 ========== */

    /**
     * @struct RoomState
     * @brief 单个房间的显示状态
     */
    struct RoomState {
        const char *name;        /**< 房间名称（如"客厅"、"主卧"） */
        const char *activity;    /**< 当前活动描述（如"主灯 68% · 空调 25°"） */
        const char *risk;        /**< 状态标签（如"舒适"、"复核"、"看护"） */
        int csi;                 /**< CSI 信号强度/动态指标值 */
        bool occupied;           /**< 是否有人（影响图标样式） */
        bool privacy_zone;       /**< 是否为隐私保护区域（浴室等） */
        lv_color_t accent;       /**< 状态栏强调色 */
    };

    /**
     * @struct EventState
     * @brief 事件/自动化日程的显示状态
     */
    struct EventState {
        const char *level;   /**< 事件等级或标题（如"回家场景"、"跌倒复核"） */
        const char *time;    /**< 时间文本（如"18:30"、"22:31"） */
        const char *text;    /**< 事件描述（如"玄关灯、热水、空调"） */
        lv_color_t accent;   /**< 时间文本颜色（按等级区分） */
    };

    /**
     * @struct DashboardState
     * @brief 完整仪表盘的状态快照
     *
     * 每种 DemoMode 对应一个预设的 DashboardState，updateUi() 根据当前模式
     * 将这些数据渲染到各个 UI 控件上。
     */
    struct DashboardState {
        const char *mode_name;      /**< 模式显示名称（如"全屋安全"、"异常复核"） */
        const char *home_status;    /**< 系统状态描述（如"WiFi-CSI 在线 | 本地 AI 就绪"） */
        const char *privacy;        /**< 隐私/安防模式文本（如"在家模式"） */
        const char *weather;        /**< 天气描述 */
        const char *outdoor_temp;   /**< 室外温度 */
        const char *humidity;       /**< 湿度文本 */
        const char *air_quality;    /**< 空气质量文本 */
        const char *indoor_env;     /**< 室内环境描述 */
        const char *night_hint;     /**< 夜间/当前模式提示文本 */
        const char *car_status;     /**< 智能小车状态（如"待命中"、"前往现场"） */
        const char *car_position;   /**< 小车当前位置（如"充电桩"、"走廊"） */
        const char *car_target;     /**< 小车目标点（如"客厅巡检点"） */
        const char *car_phase;      /**< 小车任务阶段（如"CSI 看护"、"CSI 复核"） */
        const char *obstacle;       /**< 障碍物状态 */
        const char *sensor;         /**< 传感器状态 */
        const char *voice;          /**< 语音助手状态 */
        int battery;                /**< 小车电池电量百分比 */
        int route_progress;         /**< 路线进度百分比 */
        std::array<RoomState, 4> rooms;   /**< 四个房间的状态数组 */
        std::array<EventState, 4> events; /**< 四个事件/日程的状态数组 */
    };

    /* ========== 内部方法 ========== */

    /** @brief 构建完整的 UI 布局（三栏：左侧天气/房间/自动化 + 中央场景/舒适度 + 右侧设备/安防/语音） */
    bool createUi(void);
    /** @brief 切换场景模式并刷新 UI */
    void setMode(DemoMode mode);
    /** @brief 根据当前 _mode 重新渲染所有 UI 控件的数据和样式 */
    void updateUi(void);
    /** @brief 销毁 MQTT 与天气刷新定时器 */
    void deleteTimer(void);

    /* MQTT 相关 */
    /** @brief 从 MQTT 桥接读取并处理所有待处理的入站消息 */
    void applyMqttMessages(void);
    /** @brief 处理单条 MQTT 入站消息（模式切换、事件注入、小车姿态更新） */
    void applyMqttMessage(const HomeCareMqttInboundMessage &message);
    /** @brief 更新分页指示器圆点（当前页高亮拉长） */
    void updatePageIndicator(int page);
    /** @brief DemoMode → homecare_mqtt_mode_t 枚举转换 */
    homecare_mqtt_mode_t toMqttMode(DemoMode mode) const;
    /** @brief homecare_mqtt_mode_t → DemoMode 枚举转换 */
    DemoMode fromMqttMode(homecare_mqtt_mode_t mode) const;

    /* ========== UI 工厂方法 ========== */

    /** @brief 创建带边框、圆角和阴影的标准面板 */
    lv_obj_t *createPanel(lv_obj_t *parent, int32_t width, int32_t height, lv_color_t bg);
    /** @brief 创建带字体和颜色设置的文本标签 */
    lv_obj_t *createLabel(lv_obj_t *parent, const char *text, const lv_font_t *font, lv_color_t color);
    /** @brief 创建胶囊形状态徽标（图标 + 文本） */
    lv_obj_t *createStatusPill(lv_obj_t *parent, const char *icon, const char *text, lv_color_t color);
    /** @brief 创建区域标题栏（左侧标题 + 右侧备注） */
    lv_obj_t *createSectionHeader(lv_obj_t *parent, const char *title, const char *note,
                                  int32_t width, int32_t height);
    /** @brief 为卡片设置左侧强调色条和对应的阴影颜色 */
    void setCardAccent(lv_obj_t *obj, lv_obj_t *accent, lv_color_t color);
    /** @brief 设置透明背景的头部区域样式（仅底部边框线） */
    void styleHeader(lv_obj_t *obj);
    /** @brief 设置按钮样式（支持填充和描边两种风格，带按下态动画） */
    void styleButton(lv_obj_t *obj, lv_color_t bg, bool filled);
    /** @brief 设置进度条样式（圆形端点，渐变指示器） */
    void styleBar(lv_obj_t *obj, lv_color_t color);

    /* ========== LVGL 回调 ========== */

    /** @brief 场景按钮点击回调：切换 DemoMode 并通过 MQTT 发布模式变更 */
    static void scenarioEventCb(lv_event_t *e);
    /** @brief 操作按钮点击回调：设备开关切换、小车控制、语音呼叫、隐私模式切换 */
    static void actionEventCb(lv_event_t *e);
    /** @brief 分页滚动事件回调：根据滚动偏移计算当前页码并更新指示器 */
    static void scrollEventCb(lv_event_t *e);
    /** @brief 定时器回调：处理 MQTT 消息和天气更新 */
    static void timerCb(lv_timer_t *timer);

    /* ========== 成员变量 ========== */

    /* -- 状态变量 -- */
    DemoMode _mode;                          /**< 当前场景模式 */
    lv_timer_t *_mqtt_timer;                 /**< 1 秒 MQTT 轮询定时器 */
    uint16_t _width;                         /**< 应用可视区域宽度 */
    uint16_t _height;                        /**< 应用可视区域高度 */
    bool _privacy_enabled;                   /**< 隐私模式是否启用 */
    bool _has_mqtt_event;                    /**< 是否有待显示的 MQTT 事件 */
    HomeCareMqttEvent _mqtt_event;           /**< 最近一次 MQTT 事件数据 */
    lv_color_t _mqtt_event_color;            /**< MQTT 事件的等级颜色 */
    bool _has_smartcar_attitude;             /**< 是否收到过智能小车姿态数据 */
    HomeCareMqttSmartCarAttitude _smartcar_attitude; /**< 智能小车最新姿态数据 */
    homecare_mqtt_system_status_t _smartcar_system_status; /**< 小车状态机最新状态 */
    uint32_t _weather_revision;              /**< 上次渲染时的天气快照版本号（用于检测更新） */

    /* -- 顶部栏控件 -- */
    lv_obj_t *_root;                   /**< 根容器（全屏应用背景） */
    lv_obj_t *_pages;                  /**< 横向分页容器 */
    std::array<lv_obj_t *, 3> _page_dots; /**< 分页指示器圆点 */
    lv_obj_t *_time_label;             /**< 顶部时间文本（如"08:24"） */
    lv_obj_t *_date_label;             /**< 顶部日期文本（如"5月26日"） */
    lv_obj_t *_top_weather_label;      /**< 顶部状态栏的室外温度胶囊 */
    lv_obj_t *_mode_label;             /**< 顶部状态栏的模式名称胶囊（如"全屋安全"） */
    lv_obj_t *_status_label;           /**< 副标题文本（如"晴湾公寓 · 智能中控"） */
    lv_obj_t *_privacy_label;          /**< 顶部状态栏的隐私模式胶囊 */

    /* -- 左侧面板：天气卡片 -- */
    lv_obj_t *_weather_city_label;     /**< 天气卡片城市名 */
    lv_obj_t *_weather_label;          /**< 天气描述文本（如"多云"） */
    lv_obj_t *_weather_temp_label;     /**< 天气卡片主温度（如"25°"） */
    lv_obj_t *_weather_summary_label;  /**< 天气摘要（如"湿度 42%"） */
    lv_obj_t *_aqi_label;             /**< 空气质量指数值 */
    lv_obj_t *_co2_label;             /**< CO2 浓度值 */
    lv_obj_t *_noise_label;           /**< 噪声值 */
    lv_obj_t *_outdoor_label;         /**< 室外温度文本（快捷引用） */
    lv_obj_t *_humidity_label;        /**< 湿度文本（快捷引用） */
    lv_obj_t *_air_label;             /**< 空气质量文本（快捷引用） */
    lv_obj_t *_indoor_label;          /**< 室内环境文本（快捷引用） */
    lv_obj_t *_night_label;           /**< 夜间提示文本（快捷引用） */

    /* -- 中央面板：场景与舒适度 -- */
    lv_obj_t *_hero_kicker_label;     /**< 中央区域上方引导文本（如"LIVE · 客厅"） */
    lv_obj_t *_hero_room_label;       /**< 中央区域主标题（如"客厅主控"） */
    lv_obj_t *_hero_text_label;       /**< 中央区域描述文本 */
    lv_obj_t *_security_chip_label;   /**< 安全模式胶囊标签（如"在家模式"） */
    lv_obj_t *_comfort_temp_label;    /**< 舒适度面板温度值（如"25°"） */
    lv_obj_t *_comfort_status_label;  /**< 舒适度面板状态描述 */
    lv_obj_t *_temp_arc;              /**< 温度环形指示器（18°~30°） */
    lv_obj_t *_light_label;           /**< 灯光亮度百分比 */
    lv_obj_t *_power_label;           /**< 功耗值（如"1.8kW"） */
    lv_obj_t *_online_count_label;    /**< 在线设备数（预留） */
    lv_obj_t *_assistant_text_label;  /**< 语音助手最近识别文本 */

    /* -- 右侧面板：智能小车 -- */
    lv_obj_t *_car_status_label;      /**< 小车状态（如"待命中"、"前往现场"） */
    lv_obj_t *_car_position_label;    /**< 小车当前位置 */
    lv_obj_t *_car_target_label;      /**< 小车巡检目标 */
    lv_obj_t *_car_phase_label;       /**< 小车任务阶段 */
    lv_obj_t *_obstacle_label;        /**< 障碍物检测状态 */
    lv_obj_t *_sensor_label;          /**< 传感器状态 */
    lv_obj_t *_voice_label;           /**< 语音状态 */
    lv_obj_t *_return_button;          /**< 仅在 abnormal_ready 时可用的返航按钮 */

    /* -- 房间卡片组（4 间房） -- */
    std::array<lv_obj_t *, 4> _room_cards;            /**< 房间卡片容器 */
    std::array<lv_obj_t *, 4> _room_accent_bars;      /**< 房间卡片左侧强调色条 */
    std::array<lv_obj_t *, 4> _room_name_labels;      /**< 房间名称标签 */
    std::array<lv_obj_t *, 4> _room_activity_labels;  /**< 房间活动描述标签 */
    std::array<lv_obj_t *, 4> _room_risk_labels;      /**< 房间状态标签（如"舒适"、"复核"） */
    std::array<lv_obj_t *, 4> _room_csi_labels;       /**< 房间 CSI 指标标签（预留） */

    /* -- 事件/日程卡片组（4 条） -- */
    std::array<lv_obj_t *, 4> _event_cards;       /**< 事件卡片容器 */
    std::array<lv_obj_t *, 4> _event_dot_labels;  /**< 事件等级圆点（预留） */
    std::array<lv_obj_t *, 4> _event_level_labels; /**< 事件等级/标题文本 */
    std::array<lv_obj_t *, 4> _event_time_labels;  /**< 事件时间文本 */
    std::array<lv_obj_t *, 4> _event_text_labels;  /**< 事件描述文本 */

    /* -- 场景按钮组（4 个场景） -- */
    std::array<lv_obj_t *, 4> _scene_cards;       /**< 场景按钮容器 */
    std::array<lv_obj_t *, 4> _scene_name_labels; /**< 场景名称标签 */
    std::array<lv_obj_t *, 4> _scene_desc_labels; /**< 场景描述标签 */

    /* -- 设备控制卡片组（4 个设备） -- */
    std::array<lv_obj_t *, 4> _device_cards;        /**< 设备卡片容器 */
    std::array<lv_obj_t *, 4> _device_state_labels; /**< 设备状态描述标签 */
    std::array<lv_obj_t *, 4> _device_switches;     /**< 设备开关按钮 */
};
