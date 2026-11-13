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
    radio(x + 12, y + 6, g_gui_config.wallpaper == DE_WALL_DEFAULT);
    label(x, y, "Quark Default"); y += ROW_H;
    radio(x + 12, y + 6, g_gui_config.wallpaper == DE_WALL_MIDNIGHT);
    label(x, y, "Midnight"); y += ROW_H;
    radio(x + 12, y + 6, g_gui_config.wallpaper == DE_WALL_PLASMA_SUNSET);
    label(x, y, "Plasma Sunset"); y += ROW_H;
    radio(x + 12, y + 6, g_gui_config.wallpaper == DE_WALL_DARK);
    label(x, y, "Dark gradient"); y += ROW_H;
    radio(x + 12, y + 6, g_gui_config.wallpaper == DE_WALL_LIGHT);
    label(x, y, "Light gradient"); y += ROW_H + 6;

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

    enum {
        R_DARK, R_LIGHT,
        R_WP_DEFAULT, R_WP_MIDNIGHT, R_WP_PLASMA_SUNSET,
        R_WP_DARK, R_WP_LIGHT,
        R_SNAP, R_ICONS,
        R_COUNT
    };
    int tops[R_COUNT];
    int cy = y;
    cy += 20;
    tops[R_DARK]      = cy; cy += ROW_H;
    tops[R_LIGHT]     = cy; cy += ROW_H + 6;
    cy += 20;
    tops[R_WP_DEFAULT]       = cy; cy += ROW_H;
    tops[R_WP_MIDNIGHT]      = cy; cy += ROW_H;
    tops[R_WP_PLASMA_SUNSET] = cy; cy += ROW_H;
    tops[R_WP_DARK]          = cy; cy += ROW_H;
    tops[R_WP_LIGHT]         = cy; cy += ROW_H + 6;
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

                case R_WP_DEFAULT:
                    g_gui_config.wallpaper = DE_WALL_DEFAULT;
                    de_wallpaper_invalidate(); break;
                case R_WP_MIDNIGHT:
                    g_gui_config.wallpaper = DE_WALL_MIDNIGHT;
                    de_wallpaper_invalidate(); break;
                case R_WP_PLASMA_SUNSET:
                    g_gui_config.wallpaper = DE_WALL_PLASMA_SUNSET;
                    de_wallpaper_invalidate(); break;
                case R_WP_DARK:
                    g_gui_config.wallpaper = DE_WALL_DARK;
                    de_wallpaper_invalidate(); break;
                case R_WP_LIGHT:
                    g_gui_config.wallpaper = DE_WALL_LIGHT;
                    de_wallpaper_invalidate(); break;

                case R_SNAP:  g_gui_config.snap_enabled ^= 1; break;
                case R_ICONS: g_gui_config.show_icons ^= 1; break;
            }
            return 1;
        }
    }
    return 0;
}