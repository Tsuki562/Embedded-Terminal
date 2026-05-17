from pathlib import Path


def read_text(relative_path: str) -> str:
    return Path(relative_path).read_text(encoding="utf-8")


def test_kconfig_exposes_runtime_weather_settings() -> None:
    source = read_text("main/Kconfig.projbuild")

    assert 'config HOMECARE_WEATHER_ENABLE' in source
    assert 'config HOMECARE_WEATHER_LATITUDE' in source
    assert 'config HOMECARE_WEATHER_LONGITUDE' in source
    assert 'config HOMECARE_WEATHER_REFRESH_MINUTES' in source


def test_homecare_hub_initializes_and_reads_weather_service() -> None:
    source = read_text("components/apps/homecare_hub/HomeCareHub.cpp")

    assert 'homecare_weather_service_init();' in source
    assert 'homecare_weather_service_get_snapshot(&weather_snapshot)' in source


def test_weather_service_uses_real_http_endpoints() -> None:
    source = read_text("components/apps/homecare_hub/HomeCareWeather.cpp")

    assert 'api.open-meteo.com/v1/forecast' in source
    assert 'air-quality-api.open-meteo.com/v1/air-quality' in source


def test_weather_service_initializes_esp_netif_before_http_task() -> None:
    source = read_text("components/apps/homecare_hub/HomeCareWeather.cpp")

    assert 'esp_netif_init()' in source


def test_weather_service_waits_for_ip_before_first_fetch() -> None:
    source = read_text("components/apps/homecare_hub/HomeCareWeather.cpp")

    assert 'bool network_ready = false;' in source
    assert 'TickType_t wait_ticks = network_ready ? refresh_ticks : portMAX_DELAY;' in source


def test_weather_service_uses_selected_city_coordinates() -> None:
    source = read_text("components/apps/homecare_hub/HomeCareWeather.cpp")

    assert 'homecare_weather_city_get_selected_coordinates' in source
    assert 'homecare_weather_service_request_refresh' in source


def test_settings_screen_exposes_weather_city_selector() -> None:
    source = read_text("components/apps/setting/Setting.cpp")

    assert '天气城市' in source
    assert 'homecare_weather_city_select_index' in source
    assert 'homecare_weather_city_get_selected_name' in source


if __name__ == "__main__":
    test_kconfig_exposes_runtime_weather_settings()
    test_homecare_hub_initializes_and_reads_weather_service()
    test_weather_service_uses_real_http_endpoints()
    test_weather_service_initializes_esp_netif_before_http_task()
    test_weather_service_waits_for_ip_before_first_fetch()
    test_weather_service_uses_selected_city_coordinates()
    test_settings_screen_exposes_weather_city_selector()
