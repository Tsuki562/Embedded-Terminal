#include "HomeCareWeather.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "cJSON.h"
#include "esp_check.h"
#include "esp_crt_bundle.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#ifndef CONFIG_HOMECARE_WEATHER_ENABLE
#define CONFIG_HOMECARE_WEATHER_ENABLE 1
#endif

#ifndef CONFIG_HOMECARE_WEATHER_LATITUDE
#define CONFIG_HOMECARE_WEATHER_LATITUDE "30.2741"
#endif

#ifndef CONFIG_HOMECARE_WEATHER_LONGITUDE
#define CONFIG_HOMECARE_WEATHER_LONGITUDE "120.1551"
#endif

#ifndef CONFIG_HOMECARE_WEATHER_REFRESH_MINUTES
#define CONFIG_HOMECARE_WEATHER_REFRESH_MINUTES 15
#endif

namespace {

static const char *TAG = "homecare_weather";
static constexpr size_t kHttpBufferSize = 1024;
static constexpr uint32_t kRequestTimeoutMs = 10000;
static constexpr uint32_t kTaskStackSize = 6144;
static constexpr UBaseType_t kTaskPriority = 2;

struct ParsedWeatherData {
    int temperature_c;
    int humidity_percent;
    int weather_code;
    bool is_day;
    bool has_air_quality;
    int us_aqi;
};

static SemaphoreHandle_t s_lock = nullptr;
static TaskHandle_t s_task = nullptr;
static bool s_initialized = false;
static HomeCareWeatherSnapshot s_snapshot = {};

static void set_loading_snapshot(HomeCareWeatherSnapshot *snapshot)
{
    if (snapshot == nullptr) {
        return;
    }

    snapshot->has_live_data = false;
    snapshot->stale = false;
    snapshot->air_quality_level = 0;
    std::snprintf(snapshot->weather, sizeof(snapshot->weather), "获取中");
    std::snprintf(snapshot->outdoor_temp, sizeof(snapshot->outdoor_temp), "室外 --");
    std::snprintf(snapshot->humidity, sizeof(snapshot->humidity), "湿度 --");
    std::snprintf(snapshot->air_quality, sizeof(snapshot->air_quality), "空气 --");
}

static bool snapshots_equal(const HomeCareWeatherSnapshot &lhs, const HomeCareWeatherSnapshot &rhs)
{
    return lhs.has_live_data == rhs.has_live_data &&
           lhs.stale == rhs.stale &&
           lhs.air_quality_level == rhs.air_quality_level &&
           std::strcmp(lhs.weather, rhs.weather) == 0 &&
           std::strcmp(lhs.outdoor_temp, rhs.outdoor_temp) == 0 &&
           std::strcmp(lhs.humidity, rhs.humidity) == 0 &&
           std::strcmp(lhs.air_quality, rhs.air_quality) == 0;
}

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

static void build_weather_urls(char *forecast_url, size_t forecast_size,
                               char *air_url, size_t air_size)
{
    if (forecast_url != nullptr && forecast_size > 0) {
        std::snprintf(forecast_url, forecast_size,
                      "https://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s&current=temperature_2m,relative_humidity_2m,weather_code,is_day&forecast_days=1&timezone=auto",
                      CONFIG_HOMECARE_WEATHER_LATITUDE, CONFIG_HOMECARE_WEATHER_LONGITUDE);
    }
    if (air_url != nullptr && air_size > 0) {
        std::snprintf(air_url, air_size,
                      "https://air-quality-api.open-meteo.com/v1/air-quality?latitude=%s&longitude=%s&current=us_aqi&timezone=auto",
                      CONFIG_HOMECARE_WEATHER_LATITUDE, CONFIG_HOMECARE_WEATHER_LONGITUDE);
    }
}

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
    config.transport_type = HTTP_TRANSPORT_OVER_SSL;
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
    config.crt_bundle_attach = esp_crt_bundle_attach;
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

static void build_snapshot_from_parsed(const ParsedWeatherData &parsed, HomeCareWeatherSnapshot *snapshot)
{
    if (snapshot == nullptr) {
        return;
    }

    snapshot->has_live_data = true;
    snapshot->stale = false;
    snapshot->air_quality_level = parsed.has_air_quality ? air_quality_level_from_aqi(parsed.us_aqi) : 0;
    std::snprintf(snapshot->weather, sizeof(snapshot->weather), "%s",
                  weather_code_to_text(parsed.weather_code, parsed.is_day));
    std::snprintf(snapshot->outdoor_temp, sizeof(snapshot->outdoor_temp), "室外 %dC", parsed.temperature_c);
    std::snprintf(snapshot->humidity, sizeof(snapshot->humidity), "湿度 %d%%", parsed.humidity_percent);
    std::snprintf(snapshot->air_quality, sizeof(snapshot->air_quality), "空气 %s",
                  air_quality_text_from_level(snapshot->air_quality_level));
}

static void update_snapshot_if_unavailable(void)
{
    if (s_lock == nullptr) {
        return;
    }
    if (xSemaphoreTake(s_lock, portMAX_DELAY) != pdTRUE) {
        return;
    }

    if (!s_snapshot.has_live_data) {
        HomeCareWeatherSnapshot offline = s_snapshot;
        offline.has_live_data = false;
        offline.stale = true;
        offline.air_quality_level = 0;
        std::snprintf(offline.weather, sizeof(offline.weather), "离线");
        std::snprintf(offline.outdoor_temp, sizeof(offline.outdoor_temp), "室外 --");
        std::snprintf(offline.humidity, sizeof(offline.humidity), "湿度 --");
        std::snprintf(offline.air_quality, sizeof(offline.air_quality), "空气 --");
        commit_snapshot_locked(offline);
    }

    xSemaphoreGive(s_lock);
}

static esp_err_t fetch_and_update_snapshot(void)
{
    ParsedWeatherData parsed = {};
    char forecast_url[256] = {};
    char air_url[256] = {};
    char forecast_json[kHttpBufferSize] = {};
    char air_json[kHttpBufferSize] = {};

    build_weather_urls(forecast_url, sizeof(forecast_url), air_url, sizeof(air_url));

    ESP_RETURN_ON_ERROR(http_get_json(forecast_url, forecast_json, sizeof(forecast_json)),
                        TAG, "fetch forecast failed");
    ESP_RETURN_ON_FALSE(parse_weather_json(forecast_json, &parsed), ESP_FAIL,
                        TAG, "parse forecast failed");

    if (http_get_json(air_url, air_json, sizeof(air_json)) == ESP_OK) {
        (void)parse_air_quality_json(air_json, &parsed);
    } else {
        parsed.has_air_quality = false;
    }

    HomeCareWeatherSnapshot next_snapshot = {};
    build_snapshot_from_parsed(parsed, &next_snapshot);

    if (s_lock != nullptr && xSemaphoreTake(s_lock, portMAX_DELAY) == pdTRUE) {
        commit_snapshot_locked(next_snapshot);
        xSemaphoreGive(s_lock);
    }

    return ESP_OK;
}

static void weather_task(void *arg)
{
    (void)arg;
    const TickType_t refresh_ticks =
        pdMS_TO_TICKS(CONFIG_HOMECARE_WEATHER_REFRESH_MINUTES * 60 * 1000);
    bool network_ready = false;

    for (;;) {
        TickType_t wait_ticks = network_ready ? refresh_ticks : portMAX_DELAY;
        if (ulTaskNotifyTake(pdTRUE, wait_ticks) > 0) {
            network_ready = true;
        } else if (!network_ready) {
            continue;
        }

        if (fetch_and_update_snapshot() != ESP_OK) {
            update_snapshot_if_unavailable();
        }
    }
}

static void network_event_handler(void *handler_args, esp_event_base_t event_base,
                                  int32_t event_id, void *event_data)
{
    (void)handler_args;
    (void)event_data;

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        if (s_task != nullptr) {
            xTaskNotifyGive(s_task);
        }
    }
}

}  // namespace

esp_err_t homecare_weather_service_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

#if !CONFIG_HOMECARE_WEATHER_ENABLE
    s_initialized = true;
    return ESP_OK;
#else
    // The weather task resolves DNS as soon as it is notified, so make sure
    // lwIP/tcpip is initialized before any HTTP work can start.
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "init netif failed");

    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    s_lock = xSemaphoreCreateMutex();
    if (s_lock == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    set_loading_snapshot(&s_snapshot);

    ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                            network_event_handler, nullptr, nullptr),
                        TAG, "register ip event failed");

    if (xTaskCreate(weather_task, "homecare_weather", kTaskStackSize, nullptr,
                    kTaskPriority, &s_task) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "weather service initialized");
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
