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

static void test_homecare_terminal_uses_rich_lvgl_style_properties()
{
    const std::string source = read_file("components/apps/homecare_hub/HomeCareHub.cpp");

    assert(source.find("lv_obj_set_style_bg_grad_color(_root") != std::string::npos);
    assert(source.find("lv_obj_set_style_bg_grad_color(panel") != std::string::npos);
    assert(source.find("lv_obj_set_style_shadow_width(panel") != std::string::npos);
    assert(source.find("lv_obj_set_style_outline_width(panel") != std::string::npos);
    assert(source.find("LV_STATE_PRESSED") != std::string::npos);
}

static void test_homecare_terminal_uses_lighter_status_treatments()
{
    const std::string source = read_file("components/apps/homecare_hub/HomeCareHub.cpp");

    assert(source.find("#define HUB_PANEL_COLOR        lv_color_hex(0x112236)") != std::string::npos);
    assert(source.find("#define HUB_PANEL_2_COLOR      lv_color_hex(0x0D1B2A)") != std::string::npos);
    assert(source.find("lv_obj_set_size(accent, 3, lv_obj_get_height(obj));") != std::string::npos);
    assert(source.find("lv_obj_set_style_border_color(obj, HUB_LINE_COLOR, 0);") != std::string::npos);
    assert(source.find("lv_obj_set_style_bg_grad_color(obj, lv_color_mix(lv_color_white(), color, 54), LV_PART_INDICATOR);") != std::string::npos);
    assert(source.find("lv_obj_set_style_bg_opa(_mode_label, LV_OPA_20, 0);") != std::string::npos);
    assert(source.find("lv_obj_set_style_bg_opa(_event_level_labels[i], LV_OPA_20, 0);") != std::string::npos);
    assert(source.find("lv_obj_set_size(event_dot, 6, 6);") != std::string::npos);
    assert(source.find("const char *button_icons[] = {LV_SYMBOL_REFRESH, LV_SYMBOL_HOME, LV_SYMBOL_EYE_OPEN, LV_SYMBOL_CALL};") != std::string::npos);
    assert(source.find("lv_label_set_text(weather_icon, LV_SYMBOL_HOME);") != std::string::npos);
}

static void test_third_page_uses_balanced_two_column_layout()
{
    const std::string source = read_file("components/apps/homecare_hub/HomeCareHub.cpp");

    assert(source.find("const int third_action_h = 42;") != std::string::npos);
    assert(source.find("const int third_content_h = page_h - third_action_h - third_gap;") != std::string::npos);
    assert(source.find("const int third_left_w = (page_w - 24 - third_gap) * 42 / 100;") != std::string::npos);
    assert(source.find("const int third_right_w = page_w - 24 - third_gap - third_left_w;") != std::string::npos);
    assert(source.find("lv_obj_t *event_area = createPanel(right, third_right_w, third_content_h, HUB_PANEL_2_COLOR);") != std::string::npos);
    assert(source.find("const int event_h = (third_content_h - 56 - event_gap * 3) / 4;") != std::string::npos);
    assert(source.find("lv_obj_align(event_row, LV_ALIGN_TOP_LEFT, 0, 42 + i * (event_h + event_gap));") != std::string::npos);
}

int main()
{
    test_homecare_terminal_uses_rich_lvgl_style_properties();
    test_homecare_terminal_uses_lighter_status_treatments();
    test_third_page_uses_balanced_two_column_layout();
    return 0;
}
