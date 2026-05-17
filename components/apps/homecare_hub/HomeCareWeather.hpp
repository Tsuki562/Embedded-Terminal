#pragma once

#include <cstdint>
#include "esp_err.h"

struct HomeCareWeatherSnapshot {
    bool has_live_data;
    bool stale;
    uint8_t air_quality_level;
    uint32_t revision;
    char city[24];
    char weather[24];
    char outdoor_temp[24];
    char humidity[24];
    char air_quality[24];
};

esp_err_t homecare_weather_service_init(void);
esp_err_t homecare_weather_service_request_refresh(void);
bool homecare_weather_service_get_snapshot(HomeCareWeatherSnapshot *out);
