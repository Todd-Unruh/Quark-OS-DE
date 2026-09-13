#include <quark/de.h>
#include <quark/de_apps.h>

extern uint32_t timer_get_ticks(void);

static int panel_launcher_x(void) { return 8; }
static int panel_task_x0(void)    { return 8 + DE_LAUNCHER_W + 8; }
static int panel_tray_x0(void)    { return de_fb_w() - DE_TRAY_W - 8; }

void de_panel_draw(void) {
    const de_theme_t* t = g_theme;
    int py = de_panel_y();

    de_fill(0, py, de_fb_w(), DE_PANEL_H, t->panel);
    de_hline(0, py, de_fb_w(), t->panel_edge);

    {
        int lx = panel_launcher_x();
        int ly = py + 4;
        int lw = DE_LAUNCHER_W - 8;
        int lh = DE_PANEL_H - 8;
        uint32_t bg = de_launcher_is_open() ? t->panel_sel : t->panel_btn;
        uint32_t fg = de_launcher_is_open() ? t->panel_sel_txt : t->panel_txt;
        de_fill_rounded(lx, ly, lw, lh, 6, bg);
        de_text_centered(lx + lw / 2, ly + (lh - 8) / 2, "Q", fg);
    }

    {
        int x = panel_task_x0();
        int y = py + 4;
        int h = DE_PANEL_H - 8;
        for (int i = 0; i < DE_MAX_WINDOWS; i++) {
            window_t* w = &g_windows[i];
            if (!w->is_active) continue;
            if (x + DE_TASK_BTN_W > panel_tray_x0()) break;
            uint8_t sel = (i == 0) ? 1 : 0;
            uint32_t bg = sel ? t->panel_sel : t->panel_btn;
            uint32_t tx = sel ? t->panel_sel_txt : t->panel_txt;
            de_fill_rounded(x, y, DE_TASK_BTN_W, h, 5, bg);
            int iy = y + (h - 12) / 2;
            de_fill(x + 8, iy, 12, 12, sel ? 0x00FFFFFF : t->accent);
            de_border(x + 8, iy, 12, 12, t->panel_edge);
            de_text_clipped(x + 26, y + (h - 8) / 2,
                            w->title ? w->title : "",
                            DE_TASK_BTN_W - 30, tx);
            x += DE_TASK_BTN_W + 4;
        }
    }

    {
        int x0 = panel_tray_x0();
        int ty = py + 4;
        int th = DE_PANEL_H - 8;
        de_fill_rounded(x0, ty, DE_TRAY_W, th, 5, t->panel_btn);
        int ix = x0 + 10;
        de_fill(ix,      ty + th / 2 - 1, 3, 6,  t->panel_txt);
        de_fill(ix + 4,  ty + th / 2 - 3, 3, 8,  t->panel_txt);
        de_fill(ix + 8,  ty + th / 2 - 5, 3, 10, t->panel_txt);
        ix += 18;
        de_fill(ix, ty + th / 2 - 3, 4, 6, t->panel_txt);
        de_border(ix + 5, ty + th / 2 - 5, 4, 10, t->panel_txt);
        ix += 18;
        de_border(ix, ty + th / 2 - 4, 16, 8, t->panel_txt);
        de_fill(ix + 2, ty + th / 2 - 2, 10, 4, t->panel_txt);

        char buf[24];
        de_format_clock(buf, sizeof(buf));
        int tw = de_text_width(buf);
        de_text(x0 + DE_TRAY_W - tw - 10, ty + (th - 8) / 2,
                buf, t->panel_txt);
    }
}

int de_panel_hit_any(int mx, int my) {
    return my >= de_panel_y() && my < de_fb_h() && mx >= 0 && mx < de_fb_w();
}

int de_panel_hit_launcher(int mx, int my) {
    if (my < de_panel_y() + 4) return 0;
    if (my >= de_panel_y() + DE_PANEL_H - 4) return 0;
    int lx = panel_launcher_x();
    return mx >= lx && mx < lx + DE_LAUNCHER_W;
}

int de_panel_hit_task(int mx, int my, int* out_index) {
    if (my < de_panel_y() + 4) return 0;
    if (my >= de_panel_y() + DE_PANEL_H - 4) return 0;
    if (mx < panel_task_x0()) return 0;
    if (mx >= panel_tray_x0()) return 0;
    int x = panel_task_x0();
    for (int i = 0; i < DE_MAX_WINDOWS; i++) {
        if (!g_windows[i].is_active) continue;
        if (x + DE_TASK_BTN_W > panel_tray_x0()) break;
        if (mx >= x && mx < x + DE_TASK_BTN_W) {
            if (out_index) *out_index = i;
            return 1;
        }
        x += DE_TASK_BTN_W + 4;
    }
    return 0;
}