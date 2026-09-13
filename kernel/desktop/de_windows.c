#include <quark/de.h>
#include <quark/de_apps.h>

static void draw_traffic(int x, int y, uint32_t color, char glyph) {
    de_fill_rounded(x, y, DE_BTN_W, DE_BTN_H, 3, color);
    uint32_t g = g_theme->btn_glyph;
    if (glyph == 'x') {
        for (int i = 3; i < DE_BTN_W - 3; i++) {
            int py1 = y + 3 + (i - 3);
            int py2 = y + DE_BTN_H - 4 - (i - 3);
            if (py1 < y + DE_BTN_H - 3) de_fill(x + i, py1, 1, 1, g);
            if (py2 > y + 2)            de_fill(x + i, py2, 1, 1, g);
        }
    } else if (glyph == '_') {
        de_fill(x + 5, y + DE_BTN_H - 6, DE_BTN_W - 10, 2, g);
    } else if (glyph == 'o') {
        de_border(x + 5, y + 4, DE_BTN_W - 10, DE_BTN_H - 8, g);
    }
}

void de_window_draw(window_t* w) {
    const de_theme_t* t = g_theme;

    /* Shadow */
    de_fill(w->x + 4, w->y + 4, w->width, w->height, t->win_shadow);
    de_fill(w->x + w->width, w->y + 4, DE_SHADOW, w->height, t->win_shadow);
    de_fill(w->x + 4, w->y + w->height, w->width, DE_SHADOW, t->win_shadow);

    /* Body + border */
    de_fill(w->x, w->y, w->width, w->height, t->win_body);
    de_border(w->x, w->y, w->width, w->height, t->win_border);

    /* Title bar */
    uint32_t ta = w->is_active ? t->win_title_a : t->win_title_inactive;
    uint32_t tb = w->is_active ? t->win_title_b : t->win_title_inactive;
    de_vgradient(w->x + 1, w->y + 1, w->width - 2, DE_TITLEBAR_H - 1, ta, tb);
    de_hline(w->x + 1, w->y + DE_TITLEBAR_H, w->width - 2, t->win_border);

    /* App icon */
    int icon_y = w->y + (DE_TITLEBAR_H - 12) / 2;
    de_fill(w->x + 6, icon_y, 12, 12, t->accent);
    de_border(w->x + 6, icon_y, 12, 12, t->win_border);

    /* Title */
    if (w->title) {
        int maxw = w->width - 110;
        char buf[96];
        int n = 0;
        while (w->title[n] && n < maxw / 8 && n < 95) { buf[n] = w->title[n]; n++; }
        buf[n] = '\0';
        int tw = n * 8;
        de_text(w->x + (w->width - tw) / 2,
                w->y + (DE_TITLEBAR_H - 8) / 2,
                buf, t->txt_dark);
    }

    /* Buttons */
    int bx_close = w->x + w->width - DE_BTN_W - 6;
    int bx_max   = bx_close - DE_BTN_W - DE_BTN_GAP;
    int bx_min   = bx_max   - DE_BTN_W - DE_BTN_GAP;
    int by       = w->y + (DE_TITLEBAR_H - DE_BTN_H) / 2;
    draw_traffic(bx_min,   by, t->btn_min,   '_');
    draw_traffic(bx_max,   by, t->btn_max,   'o');
    draw_traffic(bx_close, by, t->btn_close, 'x');

    /* Content: delegate to app, or draw settings body */
    if (w->app && w->app->on_draw) {
        w->app->on_draw(w->app_state, w);
    } else if (w->role == DE_ROLE_SETTINGS) {
        de_settings_draw(w);
    }

    /* Resize grip (bottom-right corner) */
    if (!w->is_maximized) {
        int gx = w->x + w->width - DE_RESIZE_GRIP;
        int gy = w->y + w->height - DE_RESIZE_GRIP;
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3 - i; j++) {
                de_fill(gx + 3 + i * 4, gy + 3 + j * 4, 2, 2, t->txt_dim);
            }
        }
    }
}

void de_windows_draw_all(void) {
    for (int i = DE_MAX_WINDOWS - 1; i >= 0; i--) {
        window_t* w = &g_windows[i];
        if (!w->is_active || w->is_minimized) continue;
        de_window_draw(w);
    }
}