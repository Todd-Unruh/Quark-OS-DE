#include <quark/de.h>

#define MAX_TOASTS 4
#define TOAST_W    260
#define TOAST_H    64
#define TOAST_LIFE 400
#define TOAST_PAD  8

typedef struct {
    int      active;
    uint32_t age;
    char     title[40];
    char     body[64];
} toast_t;

static toast_t toasts[MAX_TOASTS];

static void copy_str(char* dst, const char* src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

void de_notify_init(void) {
    kmemset(toasts, 0, sizeof(toasts));
}

void de_notify(const char* title, const char* body) {
    for (int i = MAX_TOASTS - 1; i > 0; i--) toasts[i] = toasts[i - 1];
    toasts[0].active = 1;
    toasts[0].age = 0;
    copy_str(toasts[0].title, title ? title : "", sizeof(toasts[0].title));
    copy_str(toasts[0].body,  body  ? body  : "", sizeof(toasts[0].body));
    de_damage_add(de_fb_w() - TOAST_W - 16, 0,
                  TOAST_W + 32, MAX_TOASTS * (TOAST_H + TOAST_PAD) + 32);
}

void de_notify_tick(void) {
    for (int i = 0; i < MAX_TOASTS; i++) {
        if (!toasts[i].active) continue;
        toasts[i].age++;
        if (toasts[i].age >= TOAST_LIFE) {
            toasts[i].active = 0;
            de_damage_add(de_fb_w() - TOAST_W - 16, 0,
                          TOAST_W + 32, MAX_TOASTS * (TOAST_H + TOAST_PAD) + 32);
        }
    }
}

int de_notify_has_any(void) {
    for (int i = 0; i < MAX_TOASTS; i++) if (toasts[i].active) return 1;
    return 0;
}

void de_notify_rect(int* x, int* y, int* w, int* h) {
    *x = de_fb_w() - TOAST_W - 16;
    *y = 0;
    *w = TOAST_W + 32;
    *h = MAX_TOASTS * (TOAST_H + TOAST_PAD) + 32;
}

void de_notify_draw(void) {
    const de_theme_t* t = g_theme;
    int x = de_fb_w() - TOAST_W - 16;
    int y = 16;

    for (int i = 0; i < MAX_TOASTS; i++) {
        if (!toasts[i].active) { y += TOAST_H + TOAST_PAD; continue; }

        de_fill(x + 3, y + 3, TOAST_W, TOAST_H, t->widget_shadow);
        de_fill_rounded(x, y, TOAST_W, TOAST_H, 6, t->widget_bg);
        de_border(x, y, TOAST_W, TOAST_H, t->widget_edge);
        de_fill(x, y, 4, TOAST_H, t->accent);

        de_text(x + 12, y + 10, toasts[i].title, t->widget_txt);
        de_text_clipped(x + 12, y + 30, toasts[i].body,
                        TOAST_W - 24, 0x00A0A8B8);

        y += TOAST_H + TOAST_PAD;
    }
}