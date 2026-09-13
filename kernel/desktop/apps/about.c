/* =============================================================================
 * Quark-OS kernel/desktop/apps/about.c
 * Simple "About Quark-OS" dialog window.
 * ============================================================================= */

#include <quark/de_apps.h>

typedef struct { int unused; } about_state_t;

static void* about_open(void) {
    about_state_t* s = (about_state_t*)kmalloc(sizeof(about_state_t));
    if (s) kmemset(s, 0, sizeof(*s));
    return s;
}

static void about_close(void* st) { if (st) kfree(st); }

static void about_draw(void* st, window_t* w) {
    (void)st;
    const de_theme_t* t = g_theme;
    int cx = w->x + 4;
    int cy = w->y + DE_TITLEBAR_H + 4;
    int cw = w->width - 8;
    int ch = w->height - DE_TITLEBAR_H - 8;

    de_fill(cx, cy, cw, ch, t->app_bg);
    de_border(cx, cy, cw, ch, t->win_border);

    const char* lines[] = {
        "Quark-OS v3.0",
        "32-bit protected mode",
        "",
        "Kernel:  monolithic i386",
        "Display: VESA linear FB",
        "Input:   PS/2 kbd + mouse",
        "FS:      RamFS",
        "",
        "(c) 2024, Quark Project",
    };
    int count = (int)(sizeof(lines) / sizeof(lines[0]));
    int y = cy + (ch - count * 16) / 2;
    if (y < cy + 8) y = cy + 8;

    for (int i = 0; i < count; i++) {
        de_text_centered(w->x + w->width / 2, y, lines[i], t->app_fg);
        y += 16;
    }
}

const app_t app_about = {
    .id = "about",
    .name = "About Quark",
    .icon_char = "i",
    .default_w = 260,
    .default_h = 260,
    .on_open = about_open,
    .on_close = about_close,
    .on_draw = about_draw,
    .on_click = NULL,
    .on_key = NULL,
    .on_scroll = NULL,
};