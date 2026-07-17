/**
 * @file HomeCareWeatherCity.hpp
 * @brief 天气服务城市选择模块头文件
 *
 * 提供天气查询所用城市的管理功能，包括城市列表、选中城市索引的持久化存储（NVS）、
 * 以及获取当前选中城市的名称和经纬度坐标等接口。
 */
#pragma once

#include <cstddef>
#include "esp_err.h"

/**
 * @struct HomeCareWeatherCity
 * @brief 城市信息数据结构
 *
 * 描述一个可选城市，包含名称和地理坐标（经纬度），用于构建天气 API 请求。
 */
struct HomeCareWeatherCity {
    const char *name;       /**< 城市名称（中文） */
    const char *latitude;   /**< 纬度字符串，如 "30.2741" */
    const char *longitude;  /**< 经度字符串，如 "120.1551" */
};

/** @brief 初始化城市选择模块，从 NVS 加载上次选中的城市索引 */
esp_err_t homecare_weather_city_init(void);

/** @return 城市列表中的城市总数 */
size_t homecare_weather_city_count(void);

/**
 * @brief 根据索引获取城市信息
 * @param index 城市索引（0-based）
 * @return 指向城市数据的指针，索引越界返回 nullptr
 */
const HomeCareWeatherCity *homecare_weather_city_get(size_t index);

/** @return 当前选中城市的索引 */
size_t homecare_weather_city_get_selected_index(void);

/** @return 当前选中城市的名称字符串 */
const char *homecare_weather_city_get_selected_name(void);

/**
 * @brief 获取当前选中城市的经纬度坐标
 * @param[out] latitude  输出纬度指针
 * @param[out] longitude 输出经度指针
 * @return 成功返回 true，任一参数为 nullptr 返回 false
 */
bool homecare_weather_city_get_selected_coordinates(const char **latitude, const char **longitude);

/**
 * @brief 切换选中的城市并持久化到 NVS
 * @param index 要选中的城市索引
 * @return ESP_OK 成功，ESP_ERR_INVALID_ARG 索引越界
 */
esp_err_t homecare_weather_city_select_index(size_t index);
