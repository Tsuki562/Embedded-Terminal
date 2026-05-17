#pragma once

#include <cstddef>
#include "esp_err.h"

struct HomeCareWeatherCity {
    const char *name;
    const char *latitude;
    const char *longitude;
};

esp_err_t homecare_weather_city_init(void);
size_t homecare_weather_city_count(void);
const HomeCareWeatherCity *homecare_weather_city_get(size_t index);
size_t homecare_weather_city_get_selected_index(void);
const char *homecare_weather_city_get_selected_name(void);
bool homecare_weather_city_get_selected_coordinates(const char **latitude, const char **longitude);
esp_err_t homecare_weather_city_select_index(size_t index);
