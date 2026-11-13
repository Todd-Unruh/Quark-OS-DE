#include <quark/de.h>
#include <quark/de_apps.h>
#include <quark/de_image.h>
#include <quark/de_sprites.h>
#include <quark/de_font.h>

extern uint32_t timer_get_ticks(void);

/* ------------------------------------------------------------------------- */
/* Geometry                                                                   */
/* ------------------------------------------------------------------------- */

int de_panel_y(void) { return de_fb_h() - DE_MENUBAR_H; }

static int dock_icon_size(void) { return 40; }
static int dock_icon_gap(void)  { return 12; }
static int dock_pad(void)       { return 20; }

/* The dock is a floating strip centered at the bottom of the screen. */
static int dock_top(void) { return de_fb_h() - DE_DOCK_H; }

static int dock_count(void) {
    return 2 + app_registry_dock_count();
}

static int dock_total_w(void) {
    int n = dock_count();
    int s = dock_icon_size();
    int g = dock_icon_gap();
    return dock_pad() * 2 + n * s + (n - 1) * g;
}

static int dock_x0(void) {
    int w = dock_total_w();
    int x = (de_fb_w() - w) / 2;
    if (x < 8) x = 8;
    return x;
}

static void dock_icon_rect(int i, int* out_x, int* out_y, int* out_w, int* out_h) {
    int s = dock_icon_size();
    int g = dock_icon_gap();
    *out_x = dock_x0() + dock_pad() + i * (s + g);
    *out_y = dock_top() + (DE_DOCK_H - s) / 2;
    *out_w = s;
    *out_h = s;
}

static const de_image_t* dock_sprite(int i) {
    if (i == 0) return &img_launcher_icon;
    int n = app_registry_dock_count();
    if (i == n + 1) return &img_settings_icon;
    const app_t* a = app_registry_dock_at(i - 1);
    if (a && a->icon_image) return a->icon_image;
    return NULL;
}

/* ------------------------------------------------------------------------- */
/* Menu bar (top)                                                             */
/* ------------------------------------------------------------------------- */

static void draw_menu_bar(void) {
    const de_theme_t* t = g_theme;
    int w = de_fb_w();

    de_fill_alpha(0, 0, w, DE_MENUBAR_H, t->panel, t->panel_alpha);
    de_fill_alpha(0, DE_MENUBAR_H - 1, w, 1, 0x000000, 40);

    /* Left: Q launcher icon at menu-bar scale, then "Quark-OS" text. */
    int icon_h = DE_MENUBAR_H - 6;           /* 18 px */
    int icon_w = icon_h;                     /* keep square */
    int icon_y = (DE_MENUBAR_H - icon_h) / 2;
    de_image_blit_scaled(&img_launcher_icon, 6, icon_y, icon_w, icon_h, 220);

    int text_y = (DE_MENUBAR_H - de_font_body.ascent) / 2;
    de_text(6 + icon_w + 6, text_y, "Quark-OS", t->panel_txt);

    /* Right: clock. */
    char buf[24];
    de_format_clock(buf, sizeof(buf));
    int tw = de_text_width(buf);
    de_text(w - tw - 10, text_y, buf, t->panel_txt);
}

/* ------------------------------------------------------------------------- */
/* Dock (bottom, floating)                                                    */
/* ------------------------------------------------------------------------- */

static void draw_dock(void) {
    const de_theme_t* t = g_theme;
    int dt = dock_top();
    int dx = dock_x0() - dock_pad() / 2;
    int dw = dock_total_w();
    int dh = DE_DOCK_H;

    /* Rounded translucent background strip. */
    de_fill_rounded_alpha(dx, dt, dw, dh, 20, t->panel, t->panel_alpha);
    de_border_rounded(dx, dt, dw, dh, 20, t->widget_edge);

    int n = dock_count();
    for (int i = 0; i < n; i++) {
        int ix, iy, iw, ih;
        dock_icon_rect(i, &ix, &iy, &iw, &ih);

        /* Magnification. */
        float dxf = (float)(mouse_x - (ix + iw / 2));
        float dyf = (float)(mouse_y - (iy + ih / 2));
        float d2 = dxf * dxf + dyf * dyf;
        int mag = 0;
        if (d2 < 90.0f * 90.0f) {
            float f = 1.0f - d2 / (90.0f * 90.0f);
            mag = (int)(f * 8.0f);
        }
        int ex = ix - mag / 2;
        int ey = iy - mag;
        int ew = iw + mag;
        int eh = ih + mag;

        const de_image_t* sprite = dock_sprite(i);
        if (sprite) {
            de_image_blit_scaled(sprite, ex, ey, ew, eh, 255);
        } else {
            de_fill_rounded(ex, ey, ew, eh, 10, t->app_alt_bg);
            de_border_rounded(ex, ey, ew, eh, 10, t->widget_edge);
            const app_t* a = (i >= 1 && i <= app_registry_dock_count())
                             ? app_registry_dock_at(i - 1) : NULL;
            char c = (a && a->icon_char) ? a->icon_char[0] : '?';
            char s[2] = { c, 0 };
            int tw = de_text_width(s);
            de_text(ex + (ew - tw) / 2,
                    ey + (eh - de_font_body.ascent) / 2,
                    s, t->accent);
        }

        /* Running-app dot. */
        if (i >= 1 && i <= app_registry_dock_count()) {
            const app_t* a = app_registry_dock_at(i - 1);
            int running = 0;
            for (int k = 0; k < DE_MAX_WINDOWS; k++) {
                if (g_windows[k].is_active && g_windows[k].app == a) {
                    running = 1; break;
                }
            }
            if (running) {
                de_fill_alpha(ix + iw / 2 - 2, dt + dh - 6,
                              4, 4, 0x000000, 160);
            }
        }
    }
}

void de_panel_draw(void) {
    draw_menu_bar();
    draw_dock();
}

/* ------------------------------------------------------------------------- */
/* Hit testing                                                                */
/* ------------------------------------------------------------------------- */

int de_panel_hit_any(int mx, int my) {
    (void)mx;
    if (my < DE_MENUBAR_H) return 1;
    if (my >= dock_top()) return 1;
    return 0;
}

int de_panel_hit_launcher(int mx, int my) {
    if (my < dock_top()) return 0;
    int ix, iy, iw, ih;
    dock_icon_rect(0, &ix, &iy, &iw, &ih);
    return mx >= ix && mx < ix + iw && my >= iy && my < iy + ih;
}

int de_panel_hit_task(int mx, int my, int* out_index) {
    if (my < dock_top()) return 0;
    int n = dock_count();
    for (int i = 1; i < n; i++) {
        int ix, iy, iw, ih;
        dock_icon_rect(i, &ix, &iy, &iw, &ih);
        if (mx >= ix && mx < ix + iw && my >= iy && my < iy + ih) {
            if (out_index) *out_index = i;
            return 1;
        }
    }
    return 0;
}