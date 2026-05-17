#include <cassert>
#include <fstream>
#include <sstream>
#include <string>

static std::string read_file(const char *path)
{
    std::ifstream input(path, std::ios::binary);
    assert(input.good());
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

static void test_wifi_connect_success_returns_to_main_settings_page()
{
    const std::string source = read_file("components/apps/setting/Setting.cpp");

    assert(source.find("app->stopWifiScan();\n            // Return to the main settings page so the Wi-Fi scan loop stays off while MQTT starts.\n            lv_scr_load(ui_ScreenSettingMain);") != std::string::npos);
}

static void test_stop_wifi_scan_requests_driver_to_stop_scanning()
{
    const std::string source = read_file("components/apps/setting/Setting.cpp");

    assert(source.find("esp_err_t ret = esp_wifi_scan_stop();") != std::string::npos);
    assert(source.find("esp_wifi_scan_stop failed: %s") != std::string::npos);
}

static void test_wifi_screen_auto_scan_is_blocked_when_already_connected()
{
    const std::string source = read_file("components/apps/setting/Setting.cpp");

    assert(source.find("!(xEventGroupGetBits(s_wifi_event_group) & WIFI_EVENT_CONNECTED)") != std::string::npos);
}

static void test_wifi_driver_is_started_lazily_for_manual_configuration()
{
    const std::string source = read_file("components/apps/setting/Setting.cpp");

    assert(source.find("bool AppSettings::ensureWifiTaskStarted(void)") != std::string::npos);
    assert(source.find("xTaskCreate(wifiScanTask, \"WiFi Scan\"") != std::string::npos);
    assert(source.find("&wifi_scan_handle_task") != std::string::npos);
    assert(source.find("setNvsParam(NVS_KEY_WIFI_ENABLE, 0);") != std::string::npos);
    assert(source.find("(app->_screen_index == UI_WIFI_SCAN_INDEX) && (app->_nvs_param_map[NVS_KEY_WIFI_ENABLE] == true)") == std::string::npos);
}

static void test_wifi_init_failure_is_logged_instead_of_aborting()
{
    const std::string source = read_file("components/apps/setting/Setting.cpp");

    assert(source.find("esp_wifi_init failed: %s") != std::string::npos);
    assert(source.find("esp_wifi_start failed: %s") != std::string::npos);
    assert(source.find("Init Wi-Fi failed: %s") != std::string::npos);
}

int main()
{
    test_wifi_connect_success_returns_to_main_settings_page();
    test_stop_wifi_scan_requests_driver_to_stop_scanning();
    test_wifi_screen_auto_scan_is_blocked_when_already_connected();
    test_wifi_driver_is_started_lazily_for_manual_configuration();
    test_wifi_init_failure_is_logged_instead_of_aborting();
    return 0;
}
