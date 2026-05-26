#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static std::string read_file(const char *path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input.good()) {
        std::cerr << "Could not read " << path << "\n";
        std::exit(1);
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

static void require_contains(const std::string &source, const char *needle)
{
    if (source.find(needle) == std::string::npos) {
        std::cerr << "Missing expected source fragment: " << needle << "\n";
        std::exit(1);
    }
}

static void test_homecare_terminal_uses_index_inspired_palette_for_7inch_screen()
{
    const std::string source = read_file("components/apps/homecare_hub/HomeCareHub.cpp");

    require_contains(source, "#define HUB_BG_COLOR           lv_color_hex(0x0C0F10)");
    require_contains(source, "#define HUB_PANEL_COLOR        lv_color_hex(0x121718)");
    require_contains(source, "#define HUB_PANEL_SOLID_COLOR  lv_color_hex(0x151B1D)");
    require_contains(source, "#define HUB_CYAN_COLOR         lv_color_hex(0x3FE0D0)");
    require_contains(source, "#define HUB_GREEN_COLOR        lv_color_hex(0x9BE86F)");
    require_contains(source, "#define HUB_AMBER_COLOR        lv_color_hex(0xFFC35A)");
    require_contains(source, "#define HUB_CORAL_COLOR        lv_color_hex(0xFF7F73)");
    require_contains(source, "lv_obj_set_style_shadow_width(panel, 18, 0);");
    require_contains(source, "LV_STATE_PRESSED");
}

static void test_homecare_terminal_uses_7inch_single_screen_sections()
{
    const std::string source = read_file("components/apps/homecare_hub/HomeCareHub.cpp");

    require_contains(source, "const int left_w = 240;");
    require_contains(source, "const int right_w = 240;");
    require_contains(source, "const int center_w = content_w - left_w - right_w - gap * 2;");
    require_contains(source, "lv_obj_set_size(top_actions, 310, 40);");
    require_contains(source, "const int devices_h = 158;");
    require_contains(source, "const int security_h = 108;");
    require_contains(source, "const int energy_h = 96;");
    require_contains(source, "const int assistant_h = content_h - devices_h - security_h - energy_h - gap * 3;");
    require_contains(source, "Astra Home");
    require_contains(source, "晴湾公寓 · 智能中控");
    require_contains(source, "客厅主控");
    require_contains(source, "常用设备");
    require_contains(source, "门厅安防");
    require_contains(source, "语音助手");
    require_contains(source, "createCameraPreview(security");
}

static void test_homecare_terminal_keeps_index_like_cards_and_controls()
{
    const std::string source = read_file("components/apps/homecare_hub/HomeCareHub.cpp");

    require_contains(source, "const char *room_names[] = {\"客厅\", \"主卧\", \"厨房\", \"书房\"};");
    require_contains(source, "const char *scene_names[] = {\"晨起\", \"会客\", \"观影\", \"睡眠\"};");
    require_contains(source, "const char *device_names[] = {\"客厅主灯\", \"中央空调\", \"南向窗帘\", \"客厅音响\"};");
    require_contains(source, "const char *energy_names[] = {\"空调\", \"厨电\", \"照明\", \"影音\"};");
    require_contains(source, "lv_obj_set_style_bg_color(scene, i == 0 ? HUB_CYAN_COLOR : HUB_PANEL_SOLID_COLOR, 0);");
    require_contains(source, "lv_obj_t *temp_arc = lv_arc_create(comfort);");
    require_contains(source, "lv_obj_t *mic = lv_btn_create(assistant);");
    require_contains(source, "lv_obj_add_event_cb(scene, scenarioEventCb, LV_EVENT_CLICKED, this);");
}

int main()
{
    test_homecare_terminal_uses_index_inspired_palette_for_7inch_screen();
    test_homecare_terminal_uses_7inch_single_screen_sections();
    test_homecare_terminal_keeps_index_like_cards_and_controls();
    return 0;
}
