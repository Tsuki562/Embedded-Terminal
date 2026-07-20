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

static void require_not_contains(const std::string &source, const char *needle)
{
    if (source.find(needle) != std::string::npos) {
        std::cerr << "Unexpected source fragment: " << needle << "\n";
        std::exit(1);
    }
}

static void test_homecare_terminal_uses_vercel_inspired_palette()
{
    const std::string source = read_file("components/apps/homecare_hub/HomeCareHub.cpp");

    require_contains(source, "#define HUB_BG_COLOR           lv_color_hex(0xFAFAFA)");
    require_contains(source, "#define HUB_PANEL_COLOR        lv_color_hex(0xFFFFFF)");
    require_contains(source, "#define HUB_PANEL_SOLID_COLOR  lv_color_hex(0xF5F5F5)");
    require_contains(source, "#define HUB_LINE_COLOR         lv_color_hex(0xEBEBEB)");
    require_contains(source, "#define HUB_CYAN_COLOR         lv_color_hex(0x171717)");
    require_contains(source, "#define HUB_GREEN_COLOR        lv_color_hex(0x0070F3)");
    require_contains(source, "#define HUB_CORAL_COLOR        lv_color_hex(0xEE0000)");
    require_contains(source, "lv_obj_set_style_shadow_width(panel, 8, 0);");
    require_contains(source, "LV_STATE_PRESSED");
}

static void test_homecare_terminal_uses_three_paged_sections()
{
    const std::string source = read_file("components/apps/homecare_hub/HomeCareHub.cpp");

    require_contains(source, "_pages = lv_obj_create(_root);");
    require_contains(source, "lv_obj_add_flag(_pages, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_ONE);");
    require_contains(source, "lv_obj_set_scroll_snap_x(_pages, LV_SCROLL_SNAP_CENTER);");
    require_contains(source, "lv_obj_t *page_overview = lv_obj_create(_pages);");
    require_contains(source, "lv_obj_t *page_patrol = lv_obj_create(_pages);");
    require_contains(source, "lv_obj_t *page_control = lv_obj_create(_pages);");
    require_contains(source, "updatePageIndicator(0);");
    require_contains(source, "Astra Home");
    require_contains(source, "lv_obj_t *car = createPanel(page_patrol, col_w, content_h, HUB_PANEL_COLOR);");
    require_contains(source, "lv_obj_t *devices = createPanel(page_control, col_w, content_h, HUB_PANEL_COLOR);");
    require_contains(source, "_camera_panel = createPanel(security");
    require_contains(source, "lv_obj_t *assistant = createPanel(right_stack");
}

static void test_homecare_terminal_keeps_core_cards_and_controls()
{
    const std::string source = read_file("components/apps/homecare_hub/HomeCareHub.cpp");

    require_contains(source, "lv_obj_t *csi_panel = createPanel(page_overview, page_w, lower_h, HUB_PANEL_COLOR);");
    require_contains(source, "lv_obj_set_size(csi_panel, page_w, lower_h);");
    require_contains(source, "lv_obj_align(csi_panel, LV_ALIGN_BOTTOM_LEFT, 0, 0);");
    require_contains(source, "createSectionHeader(csi_panel, \"CSI ");
    require_contains(source, "const char *device_names[] = {");
    require_not_contains(source, "lv_obj_t *scene_dock = lv_obj_create(page_overview);");
    require_not_contains(source, "const char *scene_names[]");
    require_not_contains(source, "lv_obj_add_event_cb(scene, scenarioEventCb, LV_EVENT_CLICKED, this);");
    require_contains(source, "_temp_arc = lv_arc_create(comfort);");
    require_not_contains(source, "Battery");
    require_not_contains(source, "Route");
    require_not_contains(source, "_battery_bar");
    require_not_contains(source, "_route_bar");
    require_contains(source, "lv_obj_t *mic = lv_btn_create(assistant);");

    if (source.find("const char *energy_names[]") != std::string::npos) {
        std::cerr << "Energy card should not be created in the control page\n";
        std::exit(1);
    }
}

int main()
{
    test_homecare_terminal_uses_vercel_inspired_palette();
    test_homecare_terminal_uses_three_paged_sections();
    test_homecare_terminal_keeps_core_cards_and_controls();
    return 0;
}
