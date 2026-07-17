/**
 * @file HomeCareWeather.hpp
 * @brief 天气服务模块头文件
 *
 * 定义天气数据快照结构体和后台天气服务的对外接口。
 * 天气服务通过 Open-Meteo API 获取实时天气和空气质量数据，
 * 以线程安全的快照方式向 UI 层提供数据。
 */
#pragma once

#include <cstdint>
#include "esp_err.h"

/**
 * @struct HomeCareWeatherSnapshot
 * @brief 天气数据快照
 *
 * 由后台天气任务填充，UI 层通过 get_snapshot 读取。
 * revision 字段用于 UI 层检测数据是否更新，避免无意义的重绘。
 */
struct HomeCareWeatherSnapshot {
    bool has_live_data;          /**< 是否已成功获取过一次实时天气数据 */
    bool stale;                  /**< 数据是否过期（如网络断开后降级为离线状态） */
    uint8_t air_quality_level;   /**< 空气质量等级（0=未知, 1=优, 2=良, 3=轻度, 4=中度, 5=重度, 6=严重） */
    uint32_t revision;           /**< 数据版本号，每次数据变化时递增，供 UI 检测变更 */
    char city[24];               /**< 当前城市名称 */
    char weather[24];            /**< 天气描述文本（如"晴朗"、"多云"、"小雨"） */
    char outdoor_temp[24];       /**< 室外温度文本（如"室外 24°C"） */
    char humidity[24];           /**< 湿度文本（如"湿度 58%"） */
    char air_quality[24];        /**< 空气质量文本（如"空气 优"） */
};

/** @brief 初始化天气服务：创建后台任务、注册网络事件监听 */
esp_err_t homecare_weather_service_init(void);

/** @brief 手动触发天气数据刷新（需网络就绪） */
esp_err_t homecare_weather_service_request_refresh(void);

/**
 * @brief 获取当前天气数据快照（线程安全）
 * @param[out] out 输出快照结构体指针
 * @return 成功获取返回 true，服务未初始化或参数为空返回 false
 */
bool homecare_weather_service_get_snapshot(HomeCareWeatherSnapshot *out);
