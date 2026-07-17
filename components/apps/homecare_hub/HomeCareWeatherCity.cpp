/**
 * @file HomeCareWeatherCity.cpp
 * @brief 天气服务城市选择模块实现
 *
 * 管理内置城市列表（中国主要城市及其经纬度），通过 NVS 持久化存储用户选中的城市索引，
 * 使用 FreeRTOS 互斥锁保证多线程安全。
 */
#include "HomeCareWeatherCity.hpp"

#include <cstdint>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"

namespace {

static const char *TAG = "homecare_weather_city";

/* NVS 存储相关常量 */
static constexpr char kNvsNamespace[] = "storage";          /**< NVS 命名空间 */
static constexpr char kNvsKeyCityIndex[] = "weather_city";   /**< 存储选中城市索引的 NVS 键名 */
static constexpr size_t kDefaultCityIndex = 0;               /**< 默认选中城市索引（杭州） */

/**
 * @brief 内置城市列表
 *
 * 包含中国主要省会城市及直辖市的名称与经纬度坐标，用于 Open-Meteo API 查询。
 * 索引 0 为默认城市（杭州）。
 */
static const HomeCareWeatherCity s_cities[] = {
    {"杭州", "30.2741", "120.1551"},
    {"北京", "39.9042", "116.4074"},
    {"上海", "31.2304", "121.4737"},
    {"天津", "39.0842", "117.2009"},
    {"重庆", "29.5630", "106.5516"},
    {"石家庄", "38.0428", "114.5149"},
    {"太原", "37.8706", "112.5489"},
    {"呼和浩特", "40.8426", "111.7492"},
    {"沈阳", "41.8057", "123.4315"},
    {"长春", "43.8171", "125.3235"},
    {"哈尔滨", "45.8038", "126.5349"},
    {"南京", "32.0603", "118.7969"},
    {"合肥", "31.8206", "117.2272"},
    {"福州", "26.0745", "119.2965"},
    {"南昌", "28.6820", "115.8579"},
    {"济南", "36.6512", "117.1201"},
    {"郑州", "34.7473", "113.6254"},
    {"武汉", "30.5928", "114.3055"},
    {"长沙", "28.2282", "112.9388"},
    {"广州", "23.1291", "113.2644"},
    {"南宁", "22.8170", "108.3669"},
    {"海口", "20.0440", "110.1999"},
    {"成都", "30.5728", "104.0668"},
    {"贵阳", "26.6470", "106.6302"},
    {"昆明", "24.8797", "102.8332"},
    {"拉萨", "29.6525", "91.1721"},
    {"西安", "34.3416", "108.9398"},
    {"兰州", "36.0611", "103.8343"},
    {"西宁", "36.6171", "101.7782"},
    {"银川", "38.4872", "106.2309"},
    {"乌鲁木齐", "43.8256", "87.6168"},
    {"香港", "22.3193", "114.1694"},
    {"澳门", "22.1987", "113.5439"},
    {"深圳", "22.5431", "114.0579"},
    {"苏州", "31.2989", "120.5853"},
    {"宁波", "29.8683", "121.5440"},
    {"青岛", "36.0671", "120.3826"},
    {"大连", "38.9140", "121.6147"},
    {"厦门", "24.4798", "118.0894"},
    {"无锡", "31.4900", "120.3124"},
    {"佛山", "23.0215", "113.1214"},
    {"东莞", "23.0207", "113.7518"},
    {"珠海", "22.2710", "113.5767"},
    {"三亚", "18.2528", "109.5119"},
    {"洛阳", "34.6197", "112.4540"},
    {"烟台", "37.4638", "121.4479"},
    {"南通", "31.9802", "120.8943"},
};

/* 模块内部状态 */
static SemaphoreHandle_t s_lock = nullptr;   /**< 互斥锁，保护 s_selected_index 的并发访问 */
static size_t s_selected_index = kDefaultCityIndex;  /**< 当前选中的城市索引 */
static bool s_initialized = false;           /**< 模块是否已初始化 */

/** @brief 计算内置城市列表的元素数量 */
static constexpr size_t city_count(void)
{
    return sizeof(s_cities) / sizeof(s_cities[0]);
}

/**
 * @brief 校验并修正城市索引
 *
 * 若索引越界则回退到默认城市索引，防止 NVS 中存储了损坏的数据。
 */
static size_t sanitize_city_index(int32_t index)
{
    if (index < 0 || static_cast<size_t>(index) >= city_count()) {
        return kDefaultCityIndex;
    }
    return static_cast<size_t>(index);
}

/**
 * @brief 将选中的城市索引写入 NVS（调用者需持有 s_lock）
 * @param index 要持久化的城市索引
 */
static esp_err_t persist_selected_index_locked(size_t index)
{
    nvs_handle_t nvs_handle = 0;
    esp_err_t err = nvs_open(kNvsNamespace, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_i32(nvs_handle, kNvsKeyCityIndex, static_cast<int32_t>(index));
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
    }

    nvs_close(nvs_handle);
    return err;
}

/**
 * @brief 从 NVS 加载之前选中的城市索引（调用者需持有 s_lock）
 *
 * 若 NVS 中无记录（首次启动），写入默认值；若索引越界则修正并回写。
 */
static esp_err_t load_selected_index_locked(void)
{
    nvs_handle_t nvs_handle = 0;
    esp_err_t err = nvs_open(kNvsNamespace, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }

    int32_t stored_index = static_cast<int32_t>(kDefaultCityIndex);
    err = nvs_get_i32(nvs_handle, kNvsKeyCityIndex, &stored_index);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        /* 首次启动，NVS 中尚无记录，写入默认值 */
        s_selected_index = kDefaultCityIndex;
        err = nvs_set_i32(nvs_handle, kNvsKeyCityIndex, static_cast<int32_t>(s_selected_index));
        if (err == ESP_OK) {
            err = nvs_commit(nvs_handle);
        }
    } else if (err == ESP_OK) {
        s_selected_index = sanitize_city_index(stored_index);
        /* 若 NVS 中的索引越界，修正后回写 */
        if (s_selected_index != static_cast<size_t>(stored_index)) {
            err = nvs_set_i32(nvs_handle, kNvsKeyCityIndex, static_cast<int32_t>(s_selected_index));
            if (err == ESP_OK) {
                err = nvs_commit(nvs_handle);
            }
        }
    }

    nvs_close(nvs_handle);
    return err;
}

/**
 * @brief 延迟初始化：首次调用时创建互斥锁并从 NVS 加载城市索引
 *
 * 使用 double-checked locking 模式避免重复初始化。
 */
static esp_err_t ensure_initialized(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    if (s_lock == nullptr) {
        s_lock = xSemaphoreCreateMutex();
        if (s_lock == nullptr) {
            return ESP_ERR_NO_MEM;
        }
    }

    if (xSemaphoreTake(s_lock, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }

    if (!s_initialized) {
        esp_err_t err = load_selected_index_locked();
        if (err != ESP_OK) {
            s_selected_index = kDefaultCityIndex;
            ESP_LOGW(TAG, "load city index failed: %s, fallback to %s",
                     esp_err_to_name(err), s_cities[s_selected_index].name);
        }
        s_initialized = true;
    }

    xSemaphoreGive(s_lock);
    return ESP_OK;
}

}  // namespace

esp_err_t homecare_weather_city_init(void)
{
    return ensure_initialized();
}

size_t homecare_weather_city_count(void)
{
    (void)ensure_initialized();
    return city_count();
}

const HomeCareWeatherCity *homecare_weather_city_get(size_t index)
{
    (void)ensure_initialized();
    if (index >= city_count()) {
        return nullptr;
    }
    return &s_cities[index];
}

size_t homecare_weather_city_get_selected_index(void)
{
    (void)ensure_initialized();
    return s_selected_index;
}

const char *homecare_weather_city_get_selected_name(void)
{
    (void)ensure_initialized();
    return s_cities[s_selected_index].name;
}

bool homecare_weather_city_get_selected_coordinates(const char **latitude, const char **longitude)
{
    (void)ensure_initialized();

    if (latitude == nullptr || longitude == nullptr) {
        return false;
    }

    *latitude = s_cities[s_selected_index].latitude;
    *longitude = s_cities[s_selected_index].longitude;
    return true;
}

esp_err_t homecare_weather_city_select_index(size_t index)
{
    const esp_err_t init_err = ensure_initialized();
    if (init_err != ESP_OK) {
        return init_err;
    }
    if (index >= city_count()) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_lock == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    if (xSemaphoreTake(s_lock, portMAX_DELAY) != pdTRUE) {
        return ESP_FAIL;
    }

    s_selected_index = index;
    const esp_err_t err = persist_selected_index_locked(s_selected_index);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "selected city: %s (%s,%s)",
                 s_cities[s_selected_index].name,
                 s_cities[s_selected_index].latitude,
                 s_cities[s_selected_index].longitude);
    } else {
        ESP_LOGE(TAG, "persist selected city failed: %s", esp_err_to_name(err));
    }

    xSemaphoreGive(s_lock);
    return err;
}
