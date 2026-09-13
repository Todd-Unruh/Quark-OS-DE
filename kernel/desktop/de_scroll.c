/* =============================================================================
 * Quark-OS kernel/desktop/de_scroll.c
 * Shared scroll container helper for apps.
 * ============================================================================= */

#include <quark/de.h>
#include <quark/de_apps.h>

void de_scroll_clamp(de_scroll_t* s) {
    if (!s) return;
    int max = s->content_h - s->viewport_h;
    if (max < 0) max = 0;
    if (s->scroll_y < 0) s->scroll_y = 0;
    if (s->scroll_y > max) s->scroll_y = max;
}

int de_scroll_wheel(de_scroll_t* s, int dy) {
    if (!s) return 0;
    int before = s->scroll_y;
    s->scroll_y -= dy * 24;
    de_scroll_clamp(s);
    return s->scroll_y != before;
}

void de_scroll_draw(window_t* w, de_scroll_t* s,
                    int vx, int vy, int vw, int vh) {
    if (!s) return;
    const de_theme_t* t = g_theme;

    /* Scrollbar track */
    int bar_x = vx + vw - 8;
    int bar_w = 6;
    de_fill(bar_x, vy, bar_w, vh, t->app_alt_bg);

    if (s->content_h <= s->viewport_h) return;

    /* Thumb */
    int thumb_h = vh * vh / s->content_h;
    if (thumb_h < 20) thumb_h = 20;
    int max_scroll = s->content_h - s->viewport_h;
    int thumb_y = vy;
    if (max_scroll > 0)
        thumb_y = vy + (vh - thumb_h) * s->scroll_y / max_scroll;

    de_fill_rounded(bar_x, thumb_y, bar_w, thumb_h, 3, t->accent);
}

int de_scroll_hit_scrollbar(window_t* w, de_scroll_t* s,
                            int vx, int vy, int vw, int vh,
                            int mx, int my) {
    (void)w;
    if (!s) return 0;
    if (s->content_h <= s->viewport_h) return 0;
    int bar_x = vx + vw - 8;
    if (mx < bar_x || mx >= bar_x + 6) return 0;
    if (my < vy || my >= vy + vh) return 0;
    return 1;
}

void de_scroll_drag_scrollbar(de_scroll_t* s,
                              int vy, int vh,
                              int mx, int my) {
    (void)mx;
    if (!s) return;
    if (s->content_h <= s->viewport_h) return;

    int thumb_h = vh * vh / s->content_h;
    if (thumb_h < 20) thumb_h = 20;
    int max_scroll = s->content_h - s->viewport_h;
    int rel = my - vy - thumb_h / 2;
    int range = vh - thumb_h;
    if (range <= 0) return;

    s->scroll_y = rel * max_scroll / range;
    de_scroll_clamp(s);
}