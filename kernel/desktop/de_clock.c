#include <quark/de.h>

#define CLOCK_W 150
#define CLOCK_H 64

static int wx = 0, wy = 0;
static int dragging = 0;
static int drag_ox = 0, drag_oy = 0;

void de_clock_snap(void) {
    int margin = 16;
    switch (g_gui_config.clock_pos) {
        case DE_CLOCK_TR: wx = de_fb_w() - CLOCK_W - margin; wy = margin; break;
        case DE_CLOCK_TL: wx = margin; wy = margin; break;
        case DE_CLOCK_BR:
            wx = de_fb_w() - CLOCK_W - margin;
            wy = de_panel_y() - CLOCK_H - margin;
            break;
        case DE_CLOCK_BL:
            wx = margin;
            wy = de_panel_y() - CLOCK_H - margin;
            break;
    }
    de_damage_add(wx, wy, CLOCK_W + 6, CLOCK_H + 6);
}

void de_clock_init(void) { de_clock_snap(); }
int  de_clock_is_dragging(void) { return dragging; }

void de_clock_rect(int* x, int* y, int* w, int* h) {
    *x = wx; *y = wy; *w = CLOCK_W; *h = CLOCK_H;
}

int de_clock_hit(int mx, int my) {
    return mx >= wx && mx < wx + CLOCK_W &&
           my >= wy && my < wy + CLOCK_H;
}

void de_clock_begin_drag(int mx, int my) {
    dragging = 1;
    drag_ox = mx - wx;
    drag_oy = my - wy;
}

void de_clock_continue_drag(int mx, int my) {
    int old_x = wx, old_y = wy;
    wx = mx - drag_ox;
    wy = my - drag_oy;
    if (wx < 0) wx = 0;
    if (wy < 0) wy = 0;
    if (wx > de_fb_w() - CLOCK_W) wx = de_fb_w() - CLOCK_W;
    if (wy > de_panel_y() - CLOCK_H) wy = de_panel_y() - CLOCK_H;
    de_damage_add(old_x - 2, old_y - 2, CLOCK_W + 4, CLOCK_H + 4);
    de_damage_add(wx - 2, wy - 2, CLOCK_W + 4, CLOCK_H + 4);
}

void de_clock_end_drag(void) { dragging = 0; }

void de_clock_draw(void) {
    const de_theme_t* t = g_theme;

    de_fill(wx + 3, wy + 3, CLOCK_W, CLOCK_H, t->widget_shadow);
    de_fill_rounded(wx, wy, CLOCK_W, CLOCK_H, 8, t->widget_bg);
    de_border(wx, wy, CLOCK_W, CLOCK_H, t->widget_edge);

    int h = 0, m = 0, s = 0;
    de_now_hms(&h, &m, &s);
    char timebuf[24];
    int i = 0;
    int pm = 0;
    if (!g_gui_config.clock_24h) {
        pm = (h >= 12);
        h = h % 12;
        if (h == 0) h = 12;
    }
    if (h < 10) timebuf[i++] = '0' + h;
    else { timebuf[i++] = '0' + h / 10; timebuf[i++] = '0' + h % 10; }
    timebuf[i++] = ':';
    timebuf[i++] = '0' + m / 10; timebuf[i++] = '0' + m % 10;
    if (g_gui_config.clock_show_secs) {
        timebuf[i++] = ':';
        timebuf[i++] = '0' + s / 10; timebuf[i++] = '0' + s % 10;
    }
    if (!g_gui_config.clock_24h) {
        timebuf[i++] = ' ';
        timebuf[i++] = pm ? 'P' : 'A';
        timebuf[i++] = 'M';
    }
    timebuf[i] = '\0';

    if (g_gui_config.clock_show_date) {
        de_text_centered(wx + CLOCK_W / 2, wy + 8, "Quark OS", 0x00607080);
        de_text_centered(wx + CLOCK_W / 2, wy + 24, timebuf, t->widget_txt);
        de_text_centered(wx + CLOCK_W / 2, wy + 42, "Mon Jan 01", 0x00607080);
    } else {
        de_text_centered(wx + CLOCK_W / 2, wy + CLOCK_H / 2 - 4,
                         timebuf, t->widget_txt);
    }
}