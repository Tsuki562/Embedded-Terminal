from pathlib import Path
import re


def read_text(relative_path: str) -> str:
    return Path(relative_path).read_text(encoding="utf-8", errors="ignore")


def test_mqtt_has_no_checked_in_credentials_and_validates_uri() -> None:
    bridge = read_text("components/apps/mqtt_bridge/HomeCareMqttBridge.cpp")
    kconfig = read_text("main/Kconfig.projbuild")
    defaults = read_text("sdkconfig.defaults")
    saved_config = read_text("sdkconfig.old")

    combined = "\n".join([bridge, kconfig, defaults, saved_config])
    assert "wsh040428" not in combined
    assert '#define CONFIG_HOMECARE_MQTT_USERNAME ""' in bridge
    assert '#define CONFIG_HOMECARE_MQTT_PASSWORD ""' in bridge
    assert 'default ""' in kconfig
    assert "broker_uri_is_valid" in bridge
    assert "MQTT disabled: invalid broker URI" in bridge


def test_mqtt_publish_requires_connected_state() -> None:
    bridge = read_text("components/apps/mqtt_bridge/HomeCareMqttBridge.cpp")

    assert "msg == nullptr || s_client == nullptr || !s_connected" in bridge
    assert "homecare_mqtt_bridge_publish_event" in bridge
    assert "event == nullptr || s_client == nullptr || !s_connected" in bridge


def test_setting_never_logs_wifi_password_and_copies_password_with_password_size() -> None:
    source = read_text("components/apps/setting/Setting.cpp")

    assert "password:%s" not in source
    assert "password_len=%u" in source
    assert "sizeof(wifi_config.sta.password)" in source
    assert "st_wifi_password[sizeof(st_wifi_password) - 1] = '\\0';" in source


def test_setting_background_tasks_have_stop_flags_and_handles() -> None:
    header = read_text("components/apps/setting/Setting.hpp")
    source = read_text("components/apps/setting/Setting.cpp")

    assert "TaskHandle_t _home_refresh_task;" in header
    assert "volatile bool _home_refresh_task_stop;" in header
    assert "static volatile bool s_wifi_scan_task_stop" in source
    assert "_home_refresh_task_stop = true;" in source
    assert "s_wifi_scan_task_stop = true;" in source
    assert "while (!app->_home_refresh_task_stop)" in source
    assert "while (!s_wifi_scan_task_stop)" in source


def test_homecare_hub_clears_all_lvgl_pointers_and_does_not_use_card_user_data() -> None:
    header = read_text("components/apps/homecare_hub/HomeCareHub.hpp")
    source = read_text("components/apps/homecare_hub/HomeCareHub.cpp")

    assert "std::array<lv_obj_t *, 4> _room_accent_bars;" in header
    assert "void setCardAccent(lv_obj_t *obj, lv_obj_t *accent, lv_color_t color);" in header
    assert "lv_obj_get_user_data(obj)" not in source
    assert "lv_obj_set_user_data(obj, accent)" not in source
    assert "_room_accent_bars.fill(nullptr);" in source
    assert "_event_dot_labels.fill(nullptr);" in source
    assert "_battery_label = nullptr;" in source


def test_weather_fetch_uses_dma_guard_and_heap_json_buffer() -> None:
    source = read_text("components/apps/homecare_hub/HomeCareWeather.cpp")

    assert "has_enough_dma_for_tls" in source
    assert "MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA | MALLOC_CAP_8BIT" in source
    assert "heap_caps_malloc(kHttpBufferSize" in source
    assert "heap_caps_free(json_buffer);" in source
    assert "skip weather fetch: insufficient DMA heap" in source


def test_weather_uses_plain_http_to_avoid_tls_dma_pressure_by_default() -> None:
    source = read_text("components/apps/homecare_hub/HomeCareWeather.cpp")

    assert '"http://api.open-meteo.com/v1/forecast?' in source
    assert '"http://air-quality-api.open-meteo.com/v1/air-quality?' in source
    assert "url_uses_tls" in source
    assert "config.transport_type = url_uses_tls(url) ? HTTP_TRANSPORT_OVER_SSL : HTTP_TRANSPORT_OVER_TCP;" in source
    assert 'if (url_uses_tls(forecast_url) && !has_enough_dma_for_tls("forecast"))' in source


def test_homecare_weather_metrics_use_fixed_row_layout_to_avoid_overlap() -> None:
    source = read_text("components/apps/homecare_hub/HomeCareHub.cpp")

    assert "lv_obj_t *weather_metrics = lv_obj_create(weather);" in source
    assert "lv_obj_set_size(weather_metrics, third_left_w - 28, 84);" in source
    assert "lv_obj_align(weather_metrics, LV_ALIGN_BOTTOM_LEFT, 0, 0);" in source
    assert "lv_obj_set_flex_flow(weather_metrics, LV_FLEX_FLOW_COLUMN);" in source
    assert "lv_obj_t *weather_rows[3]" in source
    assert "_humidity_label = createLabel(weather_rows[1]" in source
    assert "_air_label = createLabel(weather_rows[2]" in source
    assert "lv_obj_align(_humidity_label, LV_ALIGN_TOP_LEFT, 26, 130);" not in source
    assert "lv_obj_align(_air_label, LV_ALIGN_BOTTOM_LEFT, 26, 0);" not in source


def test_video_player_handles_empty_sd_video_list_without_creating_player() -> None:
    source = read_text("components/apps/video_player/VideoPlayer.cpp")
    close_body = re.search(r"bool AppVideoPlayer::close\(void\)\s*\{(?P<body>.*?)\n\}", source, re.S)

    assert '_video_name = nullptr;' in source
    assert "_midea_info_vect.empty()" in source
    assert "No .mjpeg files found" in source
    assert "return;" in source
    assert "esp_lvgl_simple_player_del();" in source
    assert close_body is not None
    assert "esp_lv_adapter_unlock();" not in close_body.group("body")


def test_bsp_extra_handles_optional_byte_count_and_safe_file_path_copy() -> None:
    source = read_text("common_components/bsp_extra/src/bsp_board_extra.c")

    assert "if (bytes_read) {" in source
    assert "if (bytes_written) {" in source
    assert "snprintf(audio_file_path, sizeof(audio_file_path), \"%s\", filename);" in source
    assert "snprintf(audio_file_path, sizeof(audio_file_path), \"%s\", file_path);" in source
    assert "memcpy(audio_file_path" not in source


def test_display_keeps_required_triple_dpi_frame_buffers() -> None:
    configs = [
        read_text("sdkconfig.defaults"),
        read_text("sdkconfig"),
        read_text("sdkconfig.old"),
    ]

    for config in configs:
        assert "CONFIG_BSP_LCD_DPI_BUFFER_NUMS=3" in config
        assert "CONFIG_BSP_LCD_DPI_BUFFER_NUMS=2" not in config
