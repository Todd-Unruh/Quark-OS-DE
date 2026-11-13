#include <quark/de.h>

static const de_theme_t theme_dark = {
    .wall_top = 0x001A1D22, .wall_bot = 0x00262B33, .wall_grid = 0x00202830,
    .panel = 0x001A1D22, .panel_edge = 0x003A3F46, .panel_txt = 0x00E6ECF2,
    .panel_btn = 0x002A2E34, .panel_hover = 0x003A4048,
    .panel_sel = 0x000A84FF, .panel_sel_txt = 0x00FFFFFF,
    .win_body = 0x00242426,
    .win_title_a = 0x002C2E32, .win_title_b = 0x002C2E32,
    .win_title_inactive = 0x0022262B,
    .win_border = 0x003A3F46, .win_shadow = 0x00000000,
    .txt_dark = 0x00E6ECF2, .txt_light = 0x00FFFFFF, .txt_dim = 0x0090A0B0,
    .btn_close = 0x00FF5F57, .btn_max = 0x0028C840,
    .btn_min = 0x00FEBC2E, .btn_glyph = 0x004A2C28,
    .widget_bg = 0x002A2E34, .widget_edge = 0x003A3F46,
    .widget_txt = 0x00E6ECF2, .widget_shadow = 0x00000000,
    .accent = 0x000A84FF,
    .app_bg = 0x001E2024, .app_alt_bg = 0x00282C32, .app_fg = 0x00E6ECF2,

    .panel_alpha     = 190,
    .titlebar_alpha  = 210,
    .win_alpha       = 245,
    .shadow_alpha    = 110,
    .hover_alpha     = 40,
};

static const de_theme_t theme_light = {
    .wall_top = 0x00E8ECF2, .wall_bot = 0x00C4CFDE, .wall_grid = 0x00D8DEE6,
    .panel = 0x00EAEAEC, .panel_edge = 0x00D8D8DA, .panel_txt = 0x00202830,
    .panel_btn = 0x00DCDCDE, .panel_hover = 0x00CCCCCE,
    .panel_sel = 0x000A84FF, .panel_sel_txt = 0x00FFFFFF,
    .win_body = 0x00F5F5F7,
    .win_title_a = 0x00EBEBED, .win_title_b = 0x00EBEBED,
    .win_title_inactive = 0x00E8E8EA,
    .win_border = 0x00C8C8CA, .win_shadow = 0x00405060,
    .txt_dark = 0x00252A30, .txt_light = 0x00FFFFFF, .txt_dim = 0x00607080,
    .btn_close = 0x00FF5F57, .btn_max = 0x0028C840,
    .btn_min = 0x00FEBC2E, .btn_glyph = 0x004A2C28,
    .widget_bg = 0x00EEF2F7, .widget_edge = 0x00A8B2BE,
    .widget_txt = 0x00202830, .widget_shadow = 0x00182028,
    .accent = 0x000A84FF,
    .app_bg = 0x00FCFCFD, .app_alt_bg = 0x00ECEEF2, .app_fg = 0x00252A30,

    .panel_alpha     = 210,
    .titlebar_alpha  = 230,
    .win_alpha       = 250,
    .shadow_alpha    = 70,
    .hover_alpha     = 30,
};

const de_theme_t* g_theme = &theme_dark;

const de_theme_t* de_theme_get(int id) {
    return (id == DE_THEME_LIGHT) ? &theme_light : &theme_dark;
}

void de_theme_apply(int id) {
    g_theme = de_theme_get(id);
    de_wallpaper_invalidate();
}