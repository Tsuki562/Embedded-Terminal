/**
 * @file HomeCareWeather.cpp
 * @brief 天气服务模块实现
 *
 * 后台任务定期从 Open-Meteo API 获取天气预报和空气质量数据，
 * 解析 JSON 响应后更新共享快照，供 UI 层读取。
 * 支持网络事件驱动的即时刷新和定时轮询两种模式。
 */
#include "HomeCareWeather.hpp"
#include "HomeCareWeatherCity.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "cJSON.h"
#include "esp_check.h"
#include "esp_crt_bundle.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

/* 编译期开关：可通过 menuconfig 或此处默认值控制天气功能是否启用 */
#ifndef CONFIG_HOMECARE_WEATHER_ENABLE
#define CONFIG_HOMECARE_WEATHER_ENABLE 1
#endif

/* 默认经纬度（杭州），仅在城市模块未提供坐标时回退使用 */
#ifndef CONFIG_HOMECARE_WEATHER_LATITUDE
#define CONFIG_HOMECARE_WEATHER_LATITUDE "30.2741"
#endif

#ifndef CONFIG_HOMECARE_WEATHER_LONGITUDE
#define CONFIG_HOMECARE_WEATHER_LONGITUDE "120.1551"
#endif

/* 天气数据自动刷新间隔（分钟） */
#ifndef CONFIG_HOMECARE_WEATHER_REFRESH_MINUTES
#define CONFIG_HOMECARE_WEATHER_REFRESH_MINUTES 15
#endif

namespace {

/* ---------- 常量定义 ---------- */

static const char *TAG = "homecare_weather";

static constexpr size_t kHttpBufferSize = 1024;   /**< HTTP 响应缓冲区大小（字节） */
static constexpr size_t kMinTlsDmaFree = 8192;    /**< TLS 连接所需的最小 DMA 空闲内存 */
static constexpr size_t kMinTlsDmaLargest = 4096; /**< TLS 连接所需的最小连续 DMA 块 */
static constexpr uint32_t kRequestTimeoutMs = 10000; /**< HTTP 请求超时时间（毫秒） */
static constexpr uint32_t kTaskStackSize = 6144;  /**< 天气任务栈大小（字节） */
static constexpr UBaseType_t kTaskPriority = 2;   /**< 天气任务优先级（较低，不阻塞 UI） */

/* ---------- 内部数据结构 ---------- */

/**
 * @struct ParsedWeatherData
 * @brief HTTP 响应解析后的中间数据
 *
 * 存储从天气预报和空气质量两个 API 响应中解析出的数值，
 * 由 build_snapshot_from_parsed() 转换为最终的 HomeCareWeatherSnapshot。
 */
struct ParsedWeatherData {
    int temperature_c;     /**< 温度（摄氏度，取整） */
    int humidity_percent;  /**< 相对湿度（百分比，取整） */
    int weather_code;      /**< WMO 天气编码 */
    bool is_day;           /**< 当前是否为白天 */
    bool has_air_quality;  /**< 是否成功获取到空气质量数据 */
    int us_aqi;            /**< 美国标准 AQI 指数 */
};

/* ---------- 模块全局状态 ---------- */

static SemaphoreHandle_t s_lock = nullptr;        /**< 互斥锁，保护 s_snapshot */
static TaskHandle_t s_task = nullptr;             /**< 天气后台任务句柄 */
static bool s_initialized = false;                /**< 服务是否已初始化 */
static volatile bool s_network_ready = false;     /**< 网络是否就绪（IP_EVENT_STA_GOT_IP 后置 true） */
static HomeCareWeatherSnapshot s_snapshot = {};   /**< 共享天气快照，由后台任务写入、UI 层读取 */

/* ---------- 内存诊断工具函数 ---------- */

/**
 * @brief 打印内部 DMA 堆的使用情况，用于调试 TLS 连接内存不足问题
 */
static void log_internal_dma_heap(const char *label)
{
    ESP_LOGI(TAG, "%s: dma_free=%u dma_largest=%u internal_free=%u internal_largest=%u",
             label,
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT)),
             static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT)),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)),
             static_cast<unsigned>(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)));
}

/**
 * @brief 检查是否有足够的 DMA 内存用于 TLS 握手
 *
 * ESP32 的 TLS（mbedTLS）要求 DMA 可达的内部 SRAM。若剩余不足，
 * 跳过本次请求以避免崩溃，等待下次刷新。
 */
static bool has_enough_dma_for_tls(const char *label)
{
    const size_t dma_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    const size_t dma_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
    if (dma_free < kMinTlsDmaFree || dma_largest < kMinTlsDmaLargest) {
        ESP_LOGW(TAG, "skip weather fetch: insufficient DMA heap at %s, dma_free=%u dma_largest=%u",
                 label ? label : "unknown",
                 static_cast<unsigned>(dma_free),
                 static_cast<unsigned>(dma_largest));
        return false;
    }
    return true;
}

/** @brief 判断 URL 是否使用 TLS（https:// / mqtts:// / wss://） */
static bool url_uses_tls(const char *url)
{
    return url != nullptr &&
           (std::strncmp(url, "https://", 8) == 0 || std::strncmp(url, "mqtts://", 8) == 0 || std::strncmp(url, "wss://", 6) == 0);
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

/* ---------- 快照辅助函数 ---------- */

/**
 * @brief 将快照设为"加载中"占位状态
 *
 * 在服务初始化后、首次成功获取天气数据前显示。
 */
static void set_loading_snapshot(HomeCareWeatherSnapshot *snapshot)
{
    if (snapshot == nullptr) {
        return;
    }

    snapshot->has_live_data = false;
    snapshot->stale = false;
    snapshot->air_quality_level = 0;
    std::snprintf(snapshot->city, sizeof(snapshot->city), "%s", homecare_weather_city_get_selected_name());
    std::snprintf(snapshot->weather, sizeof(snapshot->weather), "获取中");
    std::snprintf(snapshot->outdoor_temp, sizeof(snapshot->outdoor_temp), "室外 --");
    std::snprintf(snapshot->humidity, sizeof(snapshot->humidity), "湿度 --");
    std::snprintf(snapshot->air_quality, sizeof(snapshot->air_quality), "空气 --");
}

/** @brief 比较两个快照内容是否相同（不含 revision 字段） */
static bool snapshots_equal(const HomeCareWeatherSnapshot &lhs, const HomeCareWeatherSnapshot &rhs)
{
    return lhs.has_live_data == rhs.has_live_data &&
           lhs.stale == rhs.stale &&
           lhs.air_quality_level == rhs.air_quality_level &&
           std::strcmp(lhs.city, rhs.city) == 0 &&
           std::strcmp(lhs.weather, rhs.weather) == 0 &&
           std::strcmp(lhs.outdoor_temp, rhs.outdoor_temp) == 0 &&
           std::strcmp(lhs.humidity, rhs.humidity) == 0 &&
           std::strcmp(lhs.air_quality, rhs.air_quality) == 0;
}

/**
 * @brief 将新快照提交到全局 s_snapshot（调用者需持有 s_lock）
 *
 * 若内容与上一次相同则不递增 revision，避免 UI 无意义重绘。
 */
static void commit_snapshot_locked(const HomeCareWeatherSnapshot &next_snapshot)
{
    HomeCareWeatherSnapshot snapshot = next_snapshot;
    if (snapshots_equal(s_snapshot, snapshot)) {
        snapshot.revision = s_snapshot.revision;
    } else {
        snapshot.revision = s_snapshot.revision + 1;
    }
    s_snapshot = snapshot;
}

/* ---------- JSON 解析辅助 ---------- */

/** @brief 从 cJSON 节点安全读取布尔值（兼容 bool 和 number 类型） */
static bool json_get_bool(cJSON *item, bool *out)
{
    if (item == nullptr || out == nullptr) {
        return false;
    }

    if (cJSON_IsBool(item)) {
        *out = cJSON_IsTrue(item);
        return true;
    }
    if (cJSON_IsNumber(item)) {
        *out = item->valuedouble != 0.0;
        return true;
    }
    return false;
}

/**
 * @brief 将 WMO 天气编码转换为中文天气描述
 *
 * WMO 天气编码参见 Open-Meteo 文档。
 * 部分编码（如 0、1）会根据白天/夜晚返回不同文本。
 */
static const char *weather_code_to_text(int weather_code, bool is_day)
{
    switch (weather_code) {
    case 0:
        return is_day ? "晴朗" : "晴夜";
    case 1:
        return is_day ? "少云" : "晴夜少云";
    case 2:
        return "多云";
    case 3:
        return "阴天";
    case 45:
    case 48:
        return "有雾";
    case 51:
    case 53:
    case 55:
        return "毛毛雨";
    case 56:
    case 57:
        return "冻毛毛雨";
    case 61:
        return "小雨";
    case 63:
        return "中雨";
    case 65:
        return "大雨";
    case 66:
    case 67:
        return "冻雨";
    case 71:
        return "小雪";
    case 73:
        return "中雪";
    case 75:
        return "大雪";
    case 77:
        return "雪粒";
    case 80:
        return "阵雨";
    case 81:
        return "较强阵雨";
    case 82:
        return "暴雨";
    case 85:
    case 86:
        return "阵雪";
    case 95:
        return "雷暴";
    case 96:
    case 99:
        return "雷暴冰雹";
    default:
        return "天气未知";
    }
}

/**
 * @brief 根据美国标准 AQI 值映射为等级（1~6）
 *
 * 等级: 1=优(0-50), 2=良(51-100), 3=轻度(101-150),
 *       4=中度(151-200), 5=重度(201-300), 6=严重(>300)
 */
static uint8_t air_quality_level_from_aqi(int us_aqi)
{
    if (us_aqi < 0) {
        return 0;
    }
    if (us_aqi <= 50) {
        return 1;
    }
    if (us_aqi <= 100) {
        return 2;
    }
    if (us_aqi <= 150) {
        return 3;
    }
    if (us_aqi <= 200) {
        return 4;
    }
    if (us_aqi <= 300) {
        return 5;
    }
    return 6;
}

/** @brief 将空气质量等级转换为中文文本 */
static const char *air_quality_text_from_level(uint8_t level)
{
    switch (level) {
    case 1:
        return "优";
    case 2:
        return "良";
    case 3:
        return "轻度";
    case 4:
        return "中度";
    case 5:
        return "重度";
    case 6:
        return "严重";
    default:
        return "--";
    }
}

/* ---------- HTTP 请求与数据获取 ---------- */

/**
 * @brief 根据当前选中城市的坐标构建 Open-Meteo API URL
 *
 * 生成两个 URL：天气预报（含温度、湿度、天气编码、昼夜）和空气质量（US AQI）。
 */
static void build_weather_urls(char *forecast_url, size_t forecast_size,
                               char *air_url, size_t air_size)
{
    const char *latitude = nullptr;
    const char *longitude = nullptr;
    if (!homecare_weather_city_get_selected_coordinates(&latitude, &longitude)) {
        latitude = CONFIG_HOMECARE_WEATHER_LATITUDE;
        longitude = CONFIG_HOMECARE_WEATHER_LONGITUDE;
    }

    if (forecast_url != nullptr && forecast_size > 0) {
        std::snprintf(forecast_url, forecast_size,
                      "http://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s&current=temperature_2m,relative_humidity_2m,weather_code,is_day&forecast_days=1&timezone=auto",
                      latitude, longitude);
    }
    if (air_url != nullptr && air_size > 0) {
        std::snprintf(air_url, air_size,
                      "http://air-quality-api.open-meteo.com/v1/air-quality?latitude=%s&longitude=%s&current=us_aqi&timezone=auto",
                      latitude, longitude);
    }
}

/**
 * @brief 执行 HTTP GET 请求并将响应体写入缓冲区
 *
 * 自动根据 URL scheme 选择 TCP 或 TLS 传输。仅接受 200 状态码。
 * 响应超出缓冲区大小时返回失败，防止截断数据导致 JSON 解析错误。
 */
static esp_err_t http_get_json(const char *url, char *response, size_t response_size)
{
    if (url == nullptr || response == nullptr || response_size < 2) {
        return ESP_ERR_INVALID_ARG;
    }

    response[0] = '\0';

    esp_http_client_config_t config = {};
    config.url = url;
    config.timeout_ms = kRequestTimeoutMs;
    config.method = HTTP_METHOD_GET;
    config.transport_type = url_uses_tls(url) ? HTTP_TRANSPORT_OVER_SSL : HTTP_TRANSPORT_OVER_TCP;
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
    if (url_uses_tls(url)) {
        config.crt_bundle_attach = esp_crt_bundle_attach;
    }
#endif

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        esp_http_client_cleanup(client);
        return err;
    }

    (void)esp_http_client_fetch_headers(client);
    const int status = esp_http_client_get_status_code(client);
    if (status != 200) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    /* 分块读取响应体 */
    size_t total = 0;
    int read_len = 0;
    while ((read_len = esp_http_client_read(client, response + total,
                                            static_cast<int>(response_size - 1 - total))) > 0) {
        total += static_cast<size_t>(read_len);
        if (total >= response_size - 1) {
            break;
        }
    }
    response[total] = '\0';

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (read_len < 0 || total == 0 || total >= response_size - 1) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

/**
 * @brief 解析天气预报 API 的 JSON 响应
 *
 * 提取 current 字段下的 temperature_2m、relative_humidity_2m、weather_code、is_day。
 */
static bool parse_weather_json(const char *json, ParsedWeatherData *out)
{
    if (json == nullptr || out == nullptr) {
        return false;
    }

    cJSON *root = cJSON_Parse(json);
    if (root == nullptr) {
        return false;
    }

    bool ok = false;
    cJSON *current = cJSON_GetObjectItemCaseSensitive(root, "current");
    cJSON *temperature = current ? cJSON_GetObjectItemCaseSensitive(current, "temperature_2m") : nullptr;
    cJSON *humidity = current ? cJSON_GetObjectItemCaseSensitive(current, "relative_humidity_2m") : nullptr;
    cJSON *weather_code = current ? cJSON_GetObjectItemCaseSensitive(current, "weather_code") : nullptr;
    cJSON *is_day = current ? cJSON_GetObjectItemCaseSensitive(current, "is_day") : nullptr;

    if (current != nullptr &&
        cJSON_IsNumber(temperature) &&
        cJSON_IsNumber(humidity) &&
        cJSON_IsNumber(weather_code) &&
        json_get_bool(is_day, &out->is_day)) {
        out->temperature_c = static_cast<int>(std::lround(temperature->valuedouble));
        out->humidity_percent = static_cast<int>(std::lround(humidity->valuedouble));
        out->weather_code = static_cast<int>(std::lround(weather_code->valuedouble));
        ok = true;
    }

    cJSON_Delete(root);
    return ok;
}

/**
 * @brief 解析空气质量 API 的 JSON 响应
 *
 * 提取 current.us_aqi 字段。空气质量为可选数据，获取失败不影响天气主数据。
 */
static bool parse_air_quality_json(const char *json, ParsedWeatherData *out)
{
    if (json == nullptr || out == nullptr) {
        return false;
    }

    cJSON *root = cJSON_Parse(json);
    if (root == nullptr) {
        return false;
    }

    bool ok = false;
    cJSON *current = cJSON_GetObjectItemCaseSensitive(root, "current");
    cJSON *us_aqi = current ? cJSON_GetObjectItemCaseSensitive(current, "us_aqi") : nullptr;
    if (cJSON_IsNumber(us_aqi)) {
        out->us_aqi = static_cast<int>(std::lround(us_aqi->valuedouble));
        out->has_air_quality = true;
        ok = true;
    }

    cJSON_Delete(root);
    return ok;
}

/** @brief 将解析后的数值数据转换为快照的格式化文本字段 */
static void build_snapshot_from_parsed(const ParsedWeatherData &parsed, HomeCareWeatherSnapshot *snapshot)
{
    if (snapshot == nullptr) {
        return;
    }

    snapshot->has_live_data = true;
    snapshot->stale = false;
    snapshot->air_quality_level = parsed.has_air_quality ? air_quality_level_from_aqi(parsed.us_aqi) : 0;
    std::snprintf(snapshot->city, sizeof(snapshot->city), "%s", homecare_weather_city_get_selected_name());
    std::snprintf(snapshot->weather, sizeof(snapshot->weather), "%s",
                  weather_code_to_text(parsed.weather_code, parsed.is_day));
    std::snprintf(snapshot->outdoor_temp, sizeof(snapshot->outdoor_temp), "室外 %dC", parsed.temperature_c);
    std::snprintf(snapshot->humidity, sizeof(snapshot->humidity), "湿度 %d%%", parsed.humidity_percent);
    std::snprintf(snapshot->air_quality, sizeof(snapshot->air_quality), "空气 %s",
                  air_quality_text_from_level(snapshot->air_quality_level));
}

/**
 * @brief 当无实时数据或城市切换后，将快照降级为离线/占位状态
 *
 * 由 fetch_and_update_snapshot 失败时调用，确保 UI 始终有可显示的内容。
 */
static void update_snapshot_if_unavailable(void)
{
    if (s_lock == nullptr) {
        return;
    }
    if (xSemaphoreTake(s_lock, portMAX_DELAY) != pdTRUE) {
        return;
    }

    const char *selected_city = homecare_weather_city_get_selected_name();
    const bool city_changed = std::strcmp(s_snapshot.city, selected_city) != 0;
    if (!s_snapshot.has_live_data || city_changed) {
        HomeCareWeatherSnapshot offline = s_snapshot;
        offline.has_live_data = false;
        offline.stale = true;
        offline.air_quality_level = 0;
        std::snprintf(offline.city, sizeof(offline.city), "%s", selected_city);
        std::snprintf(offline.weather, sizeof(offline.weather), "离线");
        std::snprintf(offline.outdoor_temp, sizeof(offline.outdoor_temp), "室外 --");
        std::snprintf(offline.humidity, sizeof(offline.humidity), "湿度 --");
        std::snprintf(offline.air_quality, sizeof(offline.air_quality), "空气 --");
        commit_snapshot_locked(offline);
    }

    xSemaphoreGive(s_lock);
}

/**
 * @brief 执行一次完整的天气数据获取流程
 *
 * 步骤: 构建 URL → 分配缓冲区（优先 PSRAM） → HTTP GET 天气预报 → 解析 →
 *       HTTP GET 空气质量（可选） → 解析 → 构建快照 → 提交到全局状态。
 * 空气质量获取失败不会导致整体失败。
 */
static esp_err_t fetch_and_update_snapshot(void)
{
    ParsedWeatherData parsed = {};
    HomeCareWeatherSnapshot next_snapshot = {};
    char forecast_url[256] = {};
    char air_url[256] = {};
    esp_err_t err = ESP_OK;
    char *json_buffer = nullptr;

    build_weather_urls(forecast_url, sizeof(forecast_url), air_url, sizeof(air_url));

    /* 优先从 PSRAM 分配缓冲区以节省宝贵的内部 SRAM */
    json_buffer = static_cast<char *>(heap_caps_malloc(kHttpBufferSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (json_buffer == nullptr) {
        json_buffer = static_cast<char *>(heap_caps_malloc(kHttpBufferSize, MALLOC_CAP_8BIT));
    }
    if (json_buffer == nullptr) {
        return ESP_ERR_NO_MEM;
    }
    json_buffer[0] = '\0';

    /* 获取天气预报数据 */
    log_internal_dma_heap("before weather forecast fetch");
    if (!has_enough_dma_for_tls("forecast")) {
        err = ESP_ERR_NO_MEM;
        goto cleanup;
    }
    if (url_uses_tls(forecast_url) && !has_enough_dma_for_tls("forecast")) {
        err = ESP_ERR_NO_MEM;
        goto cleanup;
    }

    err = http_get_json(forecast_url, json_buffer, kHttpBufferSize);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "fetch forecast failed: %s", esp_err_to_name(err));
        goto cleanup;
    }
    if (!parse_weather_json(json_buffer, &parsed)) {
        ESP_LOGE(TAG, "parse forecast failed");
        err = ESP_FAIL;
        goto cleanup;
    }

    /* 获取空气质量数据（可选，失败不影响天气主数据） */
    json_buffer[0] = '\0';
    if (has_enough_dma_for_tls("air") &&
        http_get_json(air_url, json_buffer, kHttpBufferSize) == ESP_OK) {
        (void)parse_air_quality_json(json_buffer, &parsed);
    } else {
        parsed.has_air_quality = false;
    }

    /* 构建快照并提交 */
    build_snapshot_from_parsed(parsed, &next_snapshot);

    if (s_lock != nullptr && xSemaphoreTake(s_lock, portMAX_DELAY) == pdTRUE) {
        commit_snapshot_locked(next_snapshot);
        xSemaphoreGive(s_lock);
    }

    ESP_LOGI(TAG, "weather updated: %s | %s | %s | %s | %s%s",
             next_snapshot.city,
             next_snapshot.weather,
             next_snapshot.outdoor_temp,
             next_snapshot.humidity,
             next_snapshot.air_quality,
             parsed.has_air_quality ? "" : " (AQI unavailable)");

cleanup:
    heap_caps_free(json_buffer);
    return err;
}

/* ---------- 后台任务与网络事件 ---------- */

/**
 * @brief 天气后台任务主循环
 *
 * 网络未就绪时阻塞等待通知；网络就绪后先延迟 15 秒（让 MQTT 的 TLS 握手先完成，
 * 避免两个 TLS 连接同时争抢稀缺的 DMA 内存），然后执行天气获取。
 * 获取失败时降级为离线状态。
 */
static void weather_task(void *arg)
{
    (void)arg;
    const TickType_t refresh_ticks =
        pdMS_TO_TICKS(CONFIG_HOMECARE_WEATHER_REFRESH_MINUTES * 60 * 1000);
    bool network_ready = false;

    for (;;) {
        network_ready = s_network_ready;
        TickType_t wait_ticks = network_ready ? refresh_ticks : portMAX_DELAY;
        if (ulTaskNotifyTake(pdTRUE, wait_ticks) > 0) {
            network_ready = s_network_ready;
        } else if (!network_ready) {
            continue;
        }

        if (!network_ready) {
            continue;
        }

        /* MQTT 也在 GOT_IP 事件后启动。延迟 15 秒让其 TLS 握手先完成，
         * 避免天气 HTTPS 请求与 MQTT 同时争抢内部 DMA 可达的 SRAM。 */
        vTaskDelay(pdMS_TO_TICKS(15000));
        if (!s_network_ready) {
            continue;
        }

        if (fetch_and_update_snapshot() != ESP_OK) {
            update_snapshot_if_unavailable();
        }
    }
}

/**
 * @brief Wi-Fi/IP 事件回调
 *
 * 获取 IP 后标记网络就绪并唤醒天气任务；
 * 断开连接后清除标记并唤醒任务使其进入等待状态。
 */
static void network_event_handler(void *handler_args, esp_event_base_t event_base,
                                  int32_t event_id, void *event_data)
{
    (void)handler_args;
    (void)event_data;

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_network_ready = true;
        if (s_task != nullptr) {
            xTaskNotifyGive(s_task);
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_network_ready = false;
        if (s_task != nullptr) {
            xTaskNotifyGive(s_task);
        }
    }
}

}  // namespace

/* ========== 对外接口实现 ========== */

esp_err_t homecare_weather_service_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

#if !CONFIG_HOMECARE_WEATHER_ENABLE
    s_initialized = true;
    return ESP_OK;
#else
    /* 确保 lwIP/tcpip 已初始化，天气任务的 DNS 解析依赖它 */
    esp_err_t netif_err = esp_netif_init();
    if (netif_err != ESP_OK && netif_err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "init netif failed: %s", esp_err_to_name(netif_err));
        return netif_err;
    }
    ESP_RETURN_ON_ERROR(homecare_weather_city_init(), TAG, "init weather city failed");

    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    s_lock = xSemaphoreCreateMutex();
    if (s_lock == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    /* 设置初始占位快照，在首次数据到达前显示"获取中" */
    set_loading_snapshot(&s_snapshot);

    /* 注册网络状态变化事件 */
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                            network_event_handler, nullptr, nullptr),
                        TAG, "register ip event failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                                            network_event_handler, nullptr, nullptr),
                        TAG, "register wifi event failed");

    /* 创建天气后台任务 */
    if (xTaskCreate(weather_task, "homecare_weather", kTaskStackSize, nullptr,
                    kTaskPriority, &s_task) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    s_initialized = true;
    if (station_has_ip()) {
        s_network_ready = true;
        if (s_task != nullptr) {
            xTaskNotifyGive(s_task);
        }
    }
    ESP_LOGI(TAG, "weather service initialized");
    return ESP_OK;
#endif
}

esp_err_t homecare_weather_service_request_refresh(void)
{
#if !CONFIG_HOMECARE_WEATHER_ENABLE
    return ESP_OK;
#else
    if (!s_initialized || s_task == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!s_network_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    /* 若数据已就绪且非过期状态，不重复触发 */
    if (!s_snapshot.has_live_data && !s_snapshot.stale) {
        return ESP_OK;
    }

    xTaskNotifyGive(s_task);
    return ESP_OK;
#endif
}

bool homecare_weather_service_get_snapshot(HomeCareWeatherSnapshot *out)
{
#if !CONFIG_HOMECARE_WEATHER_ENABLE
    (void)out;
    return false;
#else
    if (!s_initialized || out == nullptr || s_lock == nullptr) {
        return false;
    }

    if (xSemaphoreTake(s_lock, portMAX_DELAY) != pdTRUE) {
        return false;
    }

    *out = s_snapshot;
    xSemaphoreGive(s_lock);
    return true;
#endif
}
