#include <quark/de.h>

static const de_theme_t theme_dark = {
    .wall_top = 0x00101828, .wall_bot = 0x00243850, .wall_grid = 0x001C2C42,
    .panel = 0x00202830, .panel_edge = 0x00405060, .panel_txt = 0x00E6ECF2,
    .panel_btn = 0x00303844, .panel_hover = 0x00404858,
    .panel_sel = 0x003A9EFF, .panel_sel_txt = 0x00FFFFFF,
    .win_body = 0x00202830,
    .win_title_a = 0x00303844, .win_title_b = 0x00202832,
    .win_title_inactive = 0x00242C34,
    .win_border = 0x00586878, .win_shadow = 0x00000000,
    .txt_dark = 0x00E6ECF2, .txt_light = 0x00FFFFFF, .txt_dim = 0x0090A0B0,
    .btn_close = 0x00E05A50, .btn_max = 0x0040B060,
    .btn_min = 0x00E0B040, .btn_glyph = 0x00202830,
    .widget_bg = 0x00202830, .widget_edge = 0x00586878,
    .widget_txt = 0x00E6ECF2, .widget_shadow = 0x00000000,
    .accent = 0x003A9EFF,
    .app_bg = 0x00182028, .app_alt_bg = 0x00242E3A, .app_fg = 0x00E6ECF2,
};

static const de_theme_t theme_light = {
    .wall_top = 0x00D8E0EC, .wall_bot = 0x00A8BCD8, .wall_grid = 0x00C0CEDC,
    .panel = 0x00E6EBF0, .panel_edge = 0x00C8CDD6, .panel_txt = 0x00202830,
    .panel_btn = 0x00D8DEE6, .panel_hover = 0x00C0CAD6,
    .panel_sel = 0x003A9EFF, .panel_sel_txt = 0x00FFFFFF,
    .win_body = 0x00F4F5F7,
    .win_title_a = 0x00E8ECF2, .win_title_b = 0x00CDD5DE,
    .win_title_inactive = 0x00E0E4EA,
    .win_border = 0x0090A0B0, .win_shadow = 0x00405060,
    .txt_dark = 0x00252A30, .txt_light = 0x00FFFFFF, .txt_dim = 0x00607080,
    .btn_close = 0x00E8A0A0, .btn_max = 0x00A8E0A0,
    .btn_min = 0x00E8D0A0, .btn_glyph = 0x00302820,
    .widget_bg = 0x00EEF2F7, .widget_edge = 0x00A8B2BE,
    .widget_txt = 0x00202830, .widget_shadow = 0x00182028,
    .accent = 0x003A9EFF,
    .app_bg = 0x00FCFCFD, .app_alt_bg = 0x00ECEEF2, .app_fg = 0x00252A30,
};

const de_theme_t* g_theme = &theme_dark;

const de_theme_t* de_theme_get(int id) {
    return (id == DE_THEME_LIGHT) ? &theme_light : &theme_dark;
}

void de_theme_apply(int id) {
    g_theme = de_theme_get(id);
    de_wallpaper_invalidate();
}