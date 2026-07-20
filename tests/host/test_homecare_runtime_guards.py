from pathlib import Path
import re


def read_text(relative_path: str) -> str:
    return Path(relative_path).read_text(encoding="utf-8", errors="ignore")


def test_mqtt_has_no_checked_in_credentials_and_validates_uri() -> None:
    bridge = read_text("components/apps/mqtt_bridge/HomeCareMqttBridge.cpp")
    kconfig = read_text("main/Kconfig.projbuild")
    defaults = read_text("sdkconfig.defaults")
    saved_config = read_text("sdkconfig.old") if Path("sdkconfig.old").exists() else ""

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
    assert "_battery_label" not in header
    assert "_battery_label" not in source
    assert "_route_label" not in header
    assert "_route_label" not in source


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

    assert "lv_obj_t *mini_metrics = lv_obj_create(weather);" in source
    assert "lv_obj_set_size(mini_metrics, col_w - 24, 34);" in source
    assert "lv_obj_align(mini_metrics, LV_ALIGN_BOTTOM_MID, 0, 0);" in source
    assert "lv_obj_set_flex_flow(mini_metrics, LV_FLEX_FLOW_ROW);" in source
    assert 'const char *metric_names[] = {"AQI", "CO2", "噪声"};' in source
    assert "lv_obj_t *metric = createPanel(mini_metrics, 66, 34, HUB_PANEL_SOLID_COLOR);" in source
    assert "lv_obj_align(*metric_values[i], LV_ALIGN_BOTTOM_RIGHT, 0, 1);" in source
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
    config_paths = [Path("sdkconfig.defaults"), Path("sdkconfig"), Path("sdkconfig.old")]
    configs = [path.read_text(encoding="utf-8", errors="ignore") for path in config_paths if path.exists()]

    for config in configs:
        assert "CONFIG_BSP_LCD_DPI_BUFFER_NUMS=3" in config
        assert "CONFIG_BSP_LCD_DPI_BUFFER_NUMS=2" not in config


def test_lvgl_adapter_continues_when_touch_controller_is_missing() -> None:
    source = read_text("main/lvgl_adapter_init.c")
    touch_failure = re.search(
        r"if \(err != ESP_OK\) \{\s*"
        r"ESP_LOGW\(TAG, \"Touch init failed \(%d\), continuing without touch input\", err\);\s*"
        r"\}",
        source,
        re.S,
    )
    assert touch_failure is not None
    assert 'ESP_LOGE(TAG, "Touch init failed (%d)", err);\n        return NULL;' not in source


def test_wifi_init_preserves_pending_scan_request() -> None:
    source = read_text("components/apps/setting/Setting.cpp")
    init_wifi = re.search(
        r"esp_err_t AppSettings::initWifi\(\)\s*\{(?P<body>.*?)\n\}\n\nvoid AppSettings::initWeatherCityUi",
        source,
        re.S,
    )
    assert init_wifi is not None
    assert "WIFI_EVENT_SCANING" not in init_wifi.group("body")


def test_homecare_hub_defers_network_services_until_wifi_has_ip() -> None:
    hub_source = read_text("components/apps/homecare_hub/HomeCareHub.cpp")
    setting_source = read_text("components/apps/setting/Setting.cpp")
    hub_init = re.search(
        r"bool HomeCareHub::init\(void\)\s*\{(?P<body>.*?)\n\}",
        hub_source,
        re.S,
    )

    assert hub_init is not None
    assert "homecare_mqtt_bridge_init" not in hub_init.group("body")
    assert "camera_mqtt_receiver_init" not in hub_init.group("body")
    assert "homecare_weather_service_init" not in hub_init.group("body")
    assert "startHomeCareNetworkServices" in setting_source
    assert "IP_EVENT_STA_GOT_IP" in setting_source


def test_homecare_hub_cleans_up_when_timer_create_fails() -> None:
    source = read_text("components/apps/homecare_hub/HomeCareHub.cpp")
    run_body = re.search(
        r"bool HomeCareHub::run\(void\)\s*\{(?P<body>.*?)\n\}\n\n/\*\* @brief",
        source,
        re.S,
    )

    assert run_body is not None
    assert "_mqtt_timer == nullptr" in run_body.group("body")
    assert 'ESP_LOGE(TAG, "Create HomeCare timers failed");' in run_body.group("body")
    assert "close();" in run_body.group("body")
    assert "return false;" in run_body.group("body")


def test_homecare_hub_uses_only_mqtt_poll_timer_for_mode_updates() -> None:
    header = read_text("components/apps/homecare_hub/HomeCareHub.hpp")
    source = read_text("components/apps/homecare_hub/HomeCareHub.cpp")

    assert "lv_timer_t *_timer" not in header
    assert "lv_timer_create(timerCb, 6000, this)" not in source
    assert "_mqtt_timer = lv_timer_create(timerCb, 1000, this);" in source
    assert "app->setMode(static_cast<DemoMode>(next));" not in source


def test_homecare_bridge_subscribes_to_system_status() -> None:
    bridge = read_text("components/apps/mqtt_bridge/HomeCareMqttBridge.cpp")

    assert 'CONFIG_HOMECARE_MQTT_INBOUND_SYSTEM_STATUS_TOPIC "smartcar/system/status"' in bridge
    assert "CONFIG_HOMECARE_MQTT_INBOUND_SYSTEM_STATUS_TOPIC);" in bridge


def test_homecare_bridge_subscribes_to_console_csi_mqtt_topics() -> None:
    bridge = read_text("components/apps/mqtt_bridge/HomeCareMqttBridge.cpp")
    kconfig = read_text("main/Kconfig.projbuild")
    protocol_header = read_text("components/apps/mqtt_bridge/HomeCareMqttProtocol.hpp")
    protocol_source = read_text("components/apps/mqtt_bridge/HomeCareMqttProtocol.cpp")

    assert 'CONFIG_HOMECARE_MQTT_INBOUND_CSI_SUMMARY_TOPIC "devices/+/csi/summary"' in bridge
    assert 'CONFIG_HOMECARE_MQTT_INBOUND_RADAR_RESULT_TOPIC "devices/+/radar/result"' in bridge
    assert "CONFIG_HOMECARE_MQTT_INBOUND_CSI_SUMMARY_TOPIC, 0);" in bridge
    assert "CONFIG_HOMECARE_MQTT_INBOUND_RADAR_RESULT_TOPIC, 0);" in bridge
    assert 'default "devices/+/csi/summary"' in kconfig
    assert 'default "devices/+/radar/result"' in kconfig
    assert "HomeCareMqttCsiSummary" in protocol_header
    assert "HomeCareMqttRadarResult" in protocol_header
    assert "has_csi_summary" in protocol_header
    assert "has_radar_result" in protocol_header
    assert 'topic_has_suffix(topic, topic_len, "/csi/summary")' in protocol_source
    assert 'topic_has_suffix(topic, topic_len, "/radar/result")' in protocol_source


def test_homecare_hub_displays_csi_summary_waveform() -> None:
    header = read_text("components/apps/homecare_hub/HomeCareHub.hpp")
    source = read_text("components/apps/homecare_hub/HomeCareHub.cpp")

    assert "HomeCareMqttCsiSummary _latest_csi_summary;" in header
    assert "bool _has_csi_summary;" in header
    assert "lv_obj_t *_csi_panel;" in header
    assert "lv_obj_t *_csi_status_label;" in header
    assert "lv_obj_t *_csi_chart;" in header
    assert "lv_chart_series_t *_csi_chart_series;" in header
    assert "void updateCsiWaveformUi(void);" in header
    assert 'createSectionHeader(csi_panel, "CSI 波形", "MQTT 实时"' in source
    assert "_csi_chart = lv_chart_create" in source
    assert "lv_chart_add_series(_csi_chart" in source
    assert 'createSectionHeader(rooms, "房间"' not in source
    assert 'lv_obj_t *rooms = createPanel' not in source
    assert "lv_obj_t *scene_dock = lv_obj_create(page_overview);" not in source
    assert "lv_obj_set_size(csi_panel, page_w, lower_h);" in source
    assert "lv_obj_align(csi_panel, LV_ALIGN_BOTTOM_LEFT, 0, 0);" in source
    assert "lv_obj_t *csi_panel = createPanel(page_overview, page_w, lower_h, HUB_PANEL_COLOR);" in source
    assert "等待 CSI summary" not in source
    assert "lv_obj_set_size(_csi_chart, page_w - 24, lower_h - 46);" in source
    assert "lower_h - 76" not in source
    assert "message.has_csi_summary && message.csi_summary.valid" in source
    assert "_latest_csi_summary = message.csi_summary;" in source
    assert "updateCsiWaveformUi();" in source
    assert "lv_chart_set_value_by_id(_csi_chart, _csi_chart_series" in source
    assert 'lv_label_set_text_fmt(_csi_status_label, "CSI %s | energy %.2f | RSSI %d | points %d"' in source


def test_homecare_hub_embeds_camera_preview_on_second_page() -> None:
    header = read_text("components/apps/homecare_hub/HomeCareHub.hpp")
    source = read_text("components/apps/homecare_hub/HomeCareHub.cpp")

    assert '#include "camera_viewer/CameraMqttReceiver.hpp"' in source
    assert '#include "esp_timer.h"' in source
    assert "void refreshCameraPreview(bool force);" in header
    assert "lv_obj_t *_camera_panel;" in header
    assert "lv_obj_t *_camera_image;" in header
    assert "lv_obj_t *_camera_status_label;" in header
    assert "lv_obj_t *_camera_empty_label;" in header
    assert "uint32_t _camera_frame_version;" in header

    assert 'createSectionHeader(security, "摄像头", "cam/jpeg 实时"' in source
    assert "_camera_panel = createPanel(security" in source
    assert "_camera_image = lv_img_create(_camera_panel);" in source
    assert "_camera_empty_label = createLabel(_camera_panel" in source
    assert "camera_mqtt_receiver_set_active(true);" in source
    assert "camera_mqtt_receiver_get_snapshot(&snapshot, _camera_frame_version)" in source
    assert "camera_mqtt_receiver_get_status(&snapshot)" in source
    assert "camera_mqtt_receiver_publish_mode(true)" in source
    assert "lv_img_set_src(_camera_image, snapshot.image);" in source
    assert "refreshCameraPreview(false);" in source
    assert "#if 0\nvoid HomeCareHub::refreshCameraPreview" not in source


def test_homecare_hub_third_page_replaces_common_devices_with_device_presence() -> None:
    header = read_text("components/apps/homecare_hub/HomeCareHub.hpp")
    source = read_text("components/apps/homecare_hub/HomeCareHub.cpp")

    assert 'createSectionHeader(devices, "设备在线", "MQTT 实时"' in source
    assert 'const char *device_names[] = {"智能小车", "摄像头", "CSI 感知"};' in source
    assert "const int device_count = 3;" in source
    assert "lv_obj_t *_smartcar_online_label;" in header
    assert "lv_obj_t *_camera_online_label;" in header
    assert "lv_obj_t *_csi_online_label;" in header
    assert "void updateDevicePresenceUi(void);" in header

    assert "updateDevicePresenceUi();" in source
    assert "CameraMqttSnapshot camera_snapshot = {};" in source
    assert "camera_mqtt_receiver_get_status(&camera_snapshot);" in source
    assert "_smartcar_system_status != HOMECARE_MQTT_SYSTEM_STATUS_UNKNOWN" in source
    assert "_has_smartcar_attitude" in source
    assert "_has_csi_summary && _latest_csi_summary.valid" in source
    assert "camera_snapshot.mqtt_connected" in source
    assert 'lv_label_set_text(_smartcar_online_label, smartcar_online ? "在线" : "离线");' in source
    assert 'lv_label_set_text(_camera_online_label, camera_online ? "在线" : "离线");' in source
    assert 'lv_label_set_text(_csi_online_label, csi_online ? "在线" : "离线");' in source

    assert 'createSectionHeader(devices, "常用设备"' not in source
    assert "homecare/device/light" not in source
    assert "homecare/device/air_conditioner" not in source
    assert "homecare/device/curtain" not in source
    assert "homecare/device/speaker" not in source
    assert "_device_switches" not in source


def test_homecare_hub_second_page_car_buttons_follow_mqtt_protocol() -> None:
    source = read_text("components/apps/homecare_hub/HomeCareHub.cpp")
    protocol = read_text("components/apps/mqtt_bridge/HomeCareMqttProtocol.cpp")

    assert 'const char *place_texts[] = {"浴室异常", "卧室异常", "厨房异常"};' in source
    assert "const int place_action_ids[] = {5, 6, 7};" in source
    assert 'const char *car_action_texts[] = {"巡航", "返航", "停止"};' in source
    assert "const int car_action_ids[] = {0, 1, 2};" in source

    assert "HOMECARE_MQTT_ACTION_PATROL" in source
    assert "HOMECARE_MQTT_ACTION_RECHARGE" in source
    assert "HOMECARE_MQTT_ACTION_STOP" in source
    assert "HOMECARE_MQTT_ACTION_ABNORMAL_BATHROOM" in source
    assert "HOMECARE_MQTT_ACTION_ABNORMAL_BEDROOM" in source
    assert "HOMECARE_MQTT_ACTION_ABNORMAL_KITCHEN" in source

    assert 'case HOMECARE_MQTT_ACTION_PATROL: state = "cruise"; break;' in protocol
    assert 'case HOMECARE_MQTT_ACTION_RECHARGE: state = "return_home"; break;' in protocol
    assert 'case HOMECARE_MQTT_ACTION_ABNORMAL_BATHROOM: state = "abnormal"; place = "bathroom"; break;' in protocol
    assert 'case HOMECARE_MQTT_ACTION_ABNORMAL_BEDROOM: state = "abnormal"; place = "bedroom"; break;' in protocol
    assert 'case HOMECARE_MQTT_ACTION_ABNORMAL_KITCHEN: state = "abnormal"; place = "kitchen"; break;' in protocol
    assert '"{\\"cmd\\":\\"system\\",\\"state\\":\\"%s\\",\\"place\\":\\"%s\\",\\"requestId\\":\\"%s\\"}"' in protocol
    assert '"{\\"requestId\\":\\"%s\\",\\"data\\":{\\"action\\":\\"stop\\"}}"' in protocol


def test_smartcar_controls_use_absolute_protocol_topic_and_gate_return() -> None:
    protocol = read_text("components/apps/mqtt_bridge/HomeCareMqttProtocol.cpp")
    bridge = read_text("components/apps/mqtt_bridge/HomeCareMqttBridge.cpp")
    hub = read_text("components/apps/homecare_hub/HomeCareHub.cpp")

    assert '"smartcar/cmd"' in protocol
    assert 'state = "cruise";' in protocol
    assert 'state = "return_home";' in protocol
    assert '"{\\\"requestId\\\":\\\"%s\\\",\\\"data\\\":{\\\"action\\\":\\\"stop\\\"}}"' in protocol
    for place in ("bathroom", "bedroom", "kitchen"):
        assert f'place = "{place}";' in protocol
    assert "publish_absolute(&msg)" in bridge
    assert "HOMECARE_MQTT_SYSTEM_STATUS_ABNORMAL_READY" in hub
    assert "LV_STATE_DISABLED" in hub
    assert 'const char *place_texts[] = {"浴室异常", "卧室异常", "厨房异常"};' in hub
    assert 'const char *car_action_texts[] = {"巡航", "返航", "停止"};' in hub


def test_esp_hosted_uses_onboard_c6_over_sdio_not_h2_spi() -> None:
    configs = {
        "sdkconfig.defaults": read_text("sdkconfig.defaults"),
        "sdkconfig": read_text("sdkconfig"),
    }

    for name, config in configs.items():
        assert "CONFIG_SLAVE_IDF_TARGET_ESP32C6=y" in config, name
        assert "CONFIG_ESP_HOSTED_CP_TARGET_ESP32C6=y" in config, name
        assert "CONFIG_ESP_HOSTED_P4_DEV_BOARD_FUNC_BOARD=y" in config, name
        assert "CONFIG_ESP_HOSTED_SDIO_HOST_INTERFACE=y" in config, name
        assert "CONFIG_ESP_HOSTED_CP_TARGET_ESP32H2=y" not in config, name
        assert "CONFIG_ESP_HOSTED_SPI_HOST_INTERFACE=y" not in config, name


def test_project_sets_esp_idf_version_before_loading_component_kconfig() -> None:
    source = read_text("CMakeLists.txt")
    env_pos = source.find('set(ENV{ESP_IDF_VERSION} "5.5")')
    include_pos = source.find('include($ENV{IDF_PATH}/tools/cmake/project.cmake)')

    assert env_pos >= 0
    assert include_pos >= 0
    assert env_pos < include_pos
