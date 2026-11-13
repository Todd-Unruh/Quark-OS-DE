#include <quark/de.h>
#include <quark/de_apps.h>
#include <quark/de_font.h>

/* Draw one macOS traffic-light button (circle + optional glyph). */
static void draw_traffic(int cx, int cy, int r, uint32_t color,
                         char glyph, uint8_t show_glyph) {
    de_fill_circle(cx, cy, r, color);
    if (!show_glyph) return;
    uint32_t g = g_theme->btn_glyph;
    int s = r / 2;
    if (glyph == 'x') {
        for (int i = -s; i <= s; i++) {
            de_fill(cx + i, cy + i, 1, 1, g);
            de_fill(cx + i, cy - i, 1, 1, g);
        }
    } else if (glyph == '-') {
        de_fill(cx - s, cy, s * 2 + 1, 1, g);
    } else if (glyph == '+') {
        de_fill(cx - s, cy, s * 2 + 1, 1, g);
        de_fill(cx, cy - s, 1, s * 2 + 1, g);
    }
}

void de_window_draw(window_t* w) {
    const de_theme_t* t = g_theme;

    /* --- Animation: open scale (0.9..1.0). --------------------------- */
    float open_t = de_anim_value(&w->anim_open);
    if (de_anim_done(&w->anim_open)) open_t = 1.0f;
    float scale = 0.90f + 0.10f * open_t;

    int wx = w->x;
    int wy = w->y;
    int ww = w->width;
    int wh = w->height;
    if (open_t < 1.0f) {
        int nw = (int)(ww * scale);
        int nh = (int)(wh * scale);
        wx = w->x + (ww - nw) / 2;
        wy = w->y + (wh - nh) / 2;
        ww = nw; wh = nh;
    }

    /* --- Shadow ------------------------------------------------------- */
    de_shadow_rect(wx, wy, ww, wh, DE_WIN_RADIUS,
                   DE_SHADOW_BLUR, t->win_shadow, t->shadow_alpha);

    /* --- Body (opaque) ----------------------------------------------- */
    de_fill_rounded(wx, wy, ww, wh, DE_WIN_RADIUS, t->win_body);

    /* --- Title bar (opaque, drawn on top of body) -------------------- */
    uint32_t ta = w->is_active ? t->win_title_a : t->win_title_inactive;

    /* Top rounded cap. */
    de_fill_rounded(wx, wy, ww, DE_TITLEBAR_H, DE_WIN_RADIUS, ta);
    /* Square off the bottom of the title bar to fill the flat middle
     * strip between the corner arcs. */
    if (ww - 2 * DE_WIN_RADIUS > 0 && DE_TITLEBAR_H - DE_WIN_RADIUS > 0) {
        de_fill(wx + DE_WIN_RADIUS,
                wy + DE_WIN_RADIUS,
                ww - 2 * DE_WIN_RADIUS,
                DE_TITLEBAR_H - DE_WIN_RADIUS,
                ta);
    }

    /* Hairline under title bar (opaque, subtle). */
    de_fill(wx + 1, wy + DE_TITLEBAR_H, ww - 2, 1,
            0x00202428);  /* slightly darker than win_title_a */

    /* --- Rounded 1px border ------------------------------------------ */
    de_border_rounded(wx, wy, ww, wh, DE_WIN_RADIUS, t->win_border);

    /* --- Title ------------------------------------------------------- */
    if (w->title) {
        uint32_t title_col = w->is_active ? t->txt_dark : t->txt_dim;
        int maxw = ww - 200;
        char buf[96];
        int n = 0;
        /* Truncate to fit the width using real font metrics. */
        while (w->title[n] && n < 95) {
            buf[n] = w->title[n];
            buf[n + 1] = '\0';
            if (de_text_width(buf) > maxw) { buf[n] = '\0'; break; }
            n++;
        }
        buf[n] = '\0';
        int tw = de_text_width(buf);
        de_text(wx + (ww - tw) / 2,
                wy + (DE_TITLEBAR_H - de_font_body.ascent) / 2,
                buf, title_col);
    }

    /* --- Traffic lights: LEFT side, order close/min/max -------------- */
    int r  = DE_BTN_W / 2;
    int by = wy + DE_TITLEBAR_H / 2;
    int bx = wx + DE_BTN_LEFT_PAD + r;
    uint8_t show_glyph = (uint8_t)(w->is_hovered && w->is_active);

    draw_traffic(bx,                                  by, r, t->btn_close, 'x', show_glyph);
    draw_traffic(bx + DE_BTN_W + DE_BTN_GAP,          by, r, t->btn_min,   '-', show_glyph);
    draw_traffic(bx + (DE_BTN_W + DE_BTN_GAP) * 2,    by, r, t->btn_max,   '+', show_glyph);

    /* --- Content: delegate to app ------------------------------------ */
    if (w->app && w->app->on_draw) {
        w->app->on_draw(w->app_state, w);
    } else if (w->role == DE_ROLE_SETTINGS) {
        de_settings_draw(w);
    }

    /* --- Resize grip -------------------------------------------------- */
    if (!w->is_maximized) {
        int gx = wx + ww - DE_RESIZE_GRIP;
        int gy = wy + wh - DE_RESIZE_GRIP;
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