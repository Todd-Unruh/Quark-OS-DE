#include <quark/de.h>

#define ROW_H 24

static void radio(int x, int y, int selected) {
    const de_theme_t* t = g_theme;
    de_border(x, y, 12, 12, t->win_border);
    if (selected) de_fill(x + 3, y + 3, 6, 6, t->accent);
}

static void label(int x, int y, const char* s) {
    de_text(x + 32, y, s, g_theme->txt_dark);
}

void de_settings_draw(window_t* w) {
    const de_theme_t* t = g_theme;
    int x = w->x + 14;
    int y = w->y + DE_TITLEBAR_H + 12;

    de_text(x, y, "Appearance", t->accent); y += 20;
    radio(x + 12, y + 6, g_gui_config.theme == DE_THEME_DARK);
    label(x, y, "Dark theme"); y += ROW_H;
    radio(x + 12, y + 6, g_gui_config.theme == DE_THEME_LIGHT);
    label(x, y, "Light theme"); y += ROW_H + 6;

    de_text(x, y, "Wallpaper", t->accent); y += 20;
    radio(x + 12, y + 6, g_gui_config.wallpaper == DE_WALL_FLOW);
    label(x, y, "Plasma Flow"); y += ROW_H;
    radio(x + 12, y + 6, g_gui_config.wallpaper == DE_WALL_DARK);
    label(x, y, "Dark gradient"); y += ROW_H;
    radio(x + 12, y + 6, g_gui_config.wallpaper == DE_WALL_LIGHT);
    label(x, y, "Light gradient"); y += ROW_H;
    radio(x + 12, y + 6, g_gui_config.wallpaper == DE_WALL_SOLID_A);
    label(x, y, "Solid blue"); y += ROW_H;
    radio(x + 12, y + 6, g_gui_config.wallpaper == DE_WALL_SOLID_B);
    label(x, y, "Solid black"); y += ROW_H + 6;

    de_text(x, y, "Floating Clock", t->accent); y += 20;
    radio(x + 12, y + 6, g_gui_config.clock_pos == DE_CLOCK_TR);
    label(x, y, "Top Right"); y += ROW_H;
    radio(x + 12, y + 6, g_gui_config.clock_pos == DE_CLOCK_TL);
    label(x, y, "Top Left"); y += ROW_H;
    radio(x + 12, y + 6, g_gui_config.clock_pos == DE_CLOCK_BR);
    label(x, y, "Bottom Right"); y += ROW_H;
    radio(x + 12, y + 6, g_gui_config.clock_pos == DE_CLOCK_BL);
    label(x, y, "Bottom Left"); y += ROW_H + 4;

    radio(x + 12, y + 6, g_gui_config.clock_24h);
    label(x, y, "24-hour"); y += ROW_H;
    radio(x + 12, y + 6, g_gui_config.clock_show_date);
    label(x, y, "Show date"); y += ROW_H;
    radio(x + 12, y + 6, g_gui_config.clock_show_secs);
    label(x, y, "Show seconds"); y += ROW_H + 6;

    de_text(x, y, "Behavior", t->accent); y += 20;
    radio(x + 12, y + 6, g_gui_config.snap_enabled);
    label(x, y, "Snap windows"); y += ROW_H;
    radio(x + 12, y + 6, g_gui_config.show_icons);
    label(x, y, "Show desktop icons"); y += ROW_H;
}

/* Handles click. Returns 1 if consumed. */
int de_settings_click(window_t* w, int mx, int my) {
    int x = w->x + 14;
    int y = w->y + DE_TITLEBAR_H + 12;
    if (mx < x || mx >= w->x + w->width - 14) return 0;

    /* Sections: header, 2 rows, header, 5 rows, header, 4 rows, 3 rows, header, 2 rows */
    enum {
        R_DARK, R_LIGHT,
        R_FLOW, R_DARKGRAD, R_LIGHTGRAD, R_SOLID_A, R_SOLID_B,
        R_CTR, R_CTL, R_CBR, R_CBL,
        R_24H, R_DATE, R_SECS,
        R_SNAP, R_ICONS,
        R_COUNT
    };
    int tops[R_COUNT];
    int cy = y;
    cy += 20;
    tops[R_DARK]      = cy; cy += ROW_H;
    tops[R_LIGHT]     = cy; cy += ROW_H + 6;
    cy += 20;
    tops[R_FLOW]      = cy; cy += ROW_H;
    tops[R_DARKGRAD]  = cy; cy += ROW_H;
    tops[R_LIGHTGRAD] = cy; cy += ROW_H;
    tops[R_SOLID_A]   = cy; cy += ROW_H;
    tops[R_SOLID_B]   = cy; cy += ROW_H + 6;
    cy += 20;
    tops[R_CTR]       = cy; cy += ROW_H;
    tops[R_CTL]       = cy; cy += ROW_H;
    tops[R_CBR]       = cy; cy += ROW_H;
    tops[R_CBL]       = cy; cy += ROW_H + 4;
    tops[R_24H]       = cy; cy += ROW_H;
    tops[R_DATE]      = cy; cy += ROW_H;
    tops[R_SECS]      = cy; cy += ROW_H + 6;
    cy += 20;
    tops[R_SNAP]      = cy; cy += ROW_H;
    tops[R_ICONS]     = cy;

    for (int i = 0; i < R_COUNT; i++) {
        if (my >= tops[i] && my < tops[i] + ROW_H - 2) {
            switch (i) {
                case R_DARK:  g_gui_config.theme = DE_THEME_DARK;
                              de_theme_apply(g_gui_config.theme); break;
                case R_LIGHT: g_gui_config.theme = DE_THEME_LIGHT;
                              de_theme_apply(g_gui_config.theme); break;
                case R_FLOW:      g_gui_config.wallpaper = DE_WALL_FLOW;
                                  de_wallpaper_invalidate(); break;
                case R_DARKGRAD:  g_gui_config.wallpaper = DE_WALL_DARK;
                                  de_wallpaper_invalidate(); break;
                case R_LIGHTGRAD: g_gui_config.wallpaper = DE_WALL_LIGHT;
                                  de_wallpaper_invalidate(); break;
                case R_SOLID_A:   g_gui_config.wallpaper = DE_WALL_SOLID_A;
                                  de_wallpaper_invalidate(); break;
                case R_SOLID_B:   g_gui_config.wallpaper = DE_WALL_SOLID_B;
                                  de_wallpaper_invalidate(); break;
                case R_CTR: g_gui_config.clock_pos = DE_CLOCK_TR; de_clock_snap(); break;
                case R_CTL: g_gui_config.clock_pos = DE_CLOCK_TL; de_clock_snap(); break;
                case R_CBR: g_gui_config.clock_pos = DE_CLOCK_BR; de_clock_snap(); break;
                case R_CBL: g_gui_config.clock_pos = DE_CLOCK_BL; de_clock_snap(); break;
                case R_24H:  g_gui_config.clock_24h ^= 1; break;
                case R_DATE: g_gui_config.clock_show_date ^= 1; break;
                case R_SECS: g_gui_config.clock_show_secs ^= 1; break;
                case R_SNAP:  g_gui_config.snap_enabled ^= 1; break;
                case R_ICONS: g_gui_config.show_icons ^= 1; break;
            }
            return 1;
        }
    }
    return 0;
}