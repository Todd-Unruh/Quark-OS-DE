/* =============================================================================
 * Quark-OS kernel/desktop/de_spotlight.c
 * macOS-style Spotlight: a centered search bar that filters the app registry.
 * ============================================================================= */

#include <quark/de.h>
#include <quark/de_apps.h>
#include <quark/de_image.h>
#include <quark/de_sprites.h>
#include <quark/de_font.h>

#define SP_W        520
#define SP_BAR_H    48
#define SP_RESULT_H 34
#define SP_MAX_RES  8
#define SP_PAD      8
#define SP_QUERY_MAX 64

typedef struct {
    int  used;
    int  app_index;              /* index into app_registry */
    char name[48];
    char id[32];
} spotlight_result_t;

static uint8_t  open_flag = 0;
static char     query[SP_QUERY_MAX];
static int      query_len = 0;
static int      selected = 0;
static int      result_count = 0;
static spotlight_result_t results[SP_MAX_RES];

/* Simple case-insensitive substring match. Returns 1 on match. */
static int icontains(const char* haystack, const char* needle) {
    if (!needle || !needle[0]) return 1;
    int hn = 0;
    while (needle[hn]) hn++;

    for (int i = 0; haystack[i]; i++) {
        int j = 0;
        while (j < hn && haystack[i + j]) {
            char a = haystack[i + j];
            char b = needle[j];
            if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
            if (b >= 'A' && b <= 'Z') b = (char)(b + 32);
            if (a != b) break;
            j++;
        }
        if (j == hn) return 1;
    }
    return 0;
}

static void copy_str(char* dst, const char* src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

/* Recompute the results list based on the current query. */
static void rebuild_results(void) {
    result_count = 0;
    int total = app_registry_count();

    for (int i = 0; i < total && result_count < SP_MAX_RES; i++) {
        const app_t* a = app_registry_at(i);
        if (!a) continue;

        /* Match against name and id. */
        if (query[0] == '\0' ||
            icontains(a->name, query) ||
            icontains(a->id, query)) {
            spotlight_result_t* r = &results[result_count];
            r->used = 1;
            r->app_index = i;
            copy_str(r->name, a->name, sizeof(r->name));
            copy_str(r->id,   a->id,   sizeof(r->id));
            result_count++;
        }
    }

    if (result_count < SP_MAX_RES &&
        (query[0] == '\0' || icontains("System Settings", query) ||
         icontains("settings", query))) {
        spotlight_result_t* r = &results[result_count++];
        r->used = 1;
        r->app_index = -1;
        copy_str(r->name, "System Settings", sizeof(r->name));
        copy_str(r->id, "settings", sizeof(r->id));
    }

    /* Clamp selection. */
    if (selected >= result_count) selected = result_count - 1;
    if (selected < 0) selected = 0;
}

/* Launch the currently-selected result and close. */
static void launch_selected(void) {
    if (result_count == 0) return;
    if (selected < 0 || selected >= result_count) return;
    int ai = results[selected].app_index;
    if (ai < 0) {
        de_wm_create(DE_ROLE_SETTINGS, "System Settings", 220, 60, 380, 380);
        de_spotlight_close();
        return;
    }
    const app_t* a = app_registry_at(ai);
    if (a) de_app_open(a->id);
    de_spotlight_close();
}

/* --------------------------------------------------------------------------- */
/* Geometry                                                                    */
/* --------------------------------------------------------------------------- */

/* Top-left of the whole spotlight panel. */
static int sp_x(void) { return (de_fb_w() - SP_W) / 2; }
static int sp_y(void) { return de_fb_h() / 4; }

/* Height depends on whether we have results. */
static int sp_h(void) {
    if (query_len == 0) return SP_BAR_H;
    int rows = result_count;
    if (rows == 0) rows = 1;              /* "no results" row */
    if (rows > SP_MAX_RES) rows = SP_MAX_RES;
    return SP_BAR_H + rows * SP_RESULT_H + SP_PAD;
}

void de_spotlight_rect(int* x, int* y, int* w, int* h) {
    *x = sp_x(); *y = sp_y(); *w = SP_W; *h = sp_h();
}

/* --------------------------------------------------------------------------- */
/* Open / close / query                                                        */
/* --------------------------------------------------------------------------- */

void de_spotlight_open(void) {
    if (open_flag) return;
    open_flag = 1;
    query[0] = '\0';
    query_len = 0;
    selected = 0;
    rebuild_results();
    de_damage_add_all();
}

void de_spotlight_close(void) {
    if (!open_flag) return;
    open_flag = 0;
    de_damage_add_all();
}

int de_spotlight_is_open(void) { return open_flag; }

void de_spotlight_toggle(void) {
    if (open_flag) de_spotlight_close();
    else           de_spotlight_open();
}

/* --------------------------------------------------------------------------- */
/* Hit testing                                                                 */
/* --------------------------------------------------------------------------- */

int de_spotlight_hit(int mx, int my) {
    if (!open_flag) return -1;
    int x, y, w, h;
    de_spotlight_rect(&x, &y, &w, &h);
    if (mx < x || mx >= x + w) return -1;
    if (my < y || my >= y + h) return -1;

    /* Bar itself is index -1. */
    if (my < y + SP_BAR_H) return -1;

    /* Results start below the bar. */
    int rel = (my - y - SP_BAR_H) / SP_RESULT_H;
    if (rel < 0 || rel >= result_count) return -1;
    return rel;
}

int de_spotlight_click(int mx, int my) {
    if (!open_flag) return 0;
    int hit = de_spotlight_hit(mx, my);

    /* Click outside the panel: dismiss. */
    int x, y, w, h;
    de_spotlight_rect(&x, &y, &w, &h);
    if (mx < x || mx >= x + w || my < y || my >= y + h) {
        de_spotlight_close();
        return 1;
    }

    if (hit < 0) return 1;      /* click on bar: swallow but do nothing */
    selected = hit;
    launch_selected();
    return 1;
}

/* --------------------------------------------------------------------------- */
/* Keyboard                                                                    */
/* --------------------------------------------------------------------------- */

int de_spotlight_handle_key(de_key_event_t* k) {
    if (!open_flag || !k) return 0;

    if (k->extended) {
        switch (k->keycode) {
            case DE_KEY_UP:
                if (selected > 0) selected--;
                return 1;
            case DE_KEY_DOWN:
                if (selected < result_count - 1) selected++;
                return 1;
            case DE_KEY_ESC:
                de_spotlight_close();
                return 1;
            default:
                break;
        }
        return 1;   /* swallow all extended keys while open */
    }

    if (k->ascii == 27) {       /* ESC */
        de_spotlight_close();
        return 1;
    }
    if (k->ascii == '\n') {
        launch_selected();
        return 1;
    }
    if (k->ascii == '\b') {
        if (query_len > 0) {
            query_len--;
            query[query_len] = '\0';
            selected = 0;
            rebuild_results();
            de_damage_add_all();
        }
        return 1;
    }
    if (k->ascii >= 32 && k->ascii < 127 && query_len < SP_QUERY_MAX - 1) {
        query[query_len++] = k->ascii;
        query[query_len] = '\0';
        selected = 0;
        rebuild_results();
        de_damage_add_all();
        return 1;
    }
    return 1;   /* swallow everything while open */
}

/* --------------------------------------------------------------------------- */
/* Draw                                                                        */
/* --------------------------------------------------------------------------- */

static void draw_search_icon(int x, int y, uint32_t color) {
    /* A small magnifying glass: circle + handle. */
    de_fill_circle(x + 7, y + 7, 5, color);
    de_fill_circle(x + 7, y + 7, 3, 0x00202024);   /* punch out center */
    de_fill(x + 11, y + 11, 5, 2, color);
}

static void draw_bar(void) {
    const de_theme_t* t = g_theme;
    int x = sp_x();
    int y = sp_y();
    int w = SP_W;
    int h = SP_BAR_H;

    /* Shadow + translucent background. */
    de_shadow_rect(x, y, w, h, 12, 16, 0x000000, 130);
    de_fill_rounded_alpha(x, y, w, h, 12, t->widget_bg, 235);
    de_border_rounded(x, y, w, h, 12, t->widget_edge);

    /* Magnifier icon on the left. */
    draw_search_icon(x + 14, y + (h - 16) / 2, t->txt_dim);

    /* Query text or placeholder. */
    int text_y = y + (h - de_font_body.ascent) / 2;
    if (query_len == 0) {
        de_text(x + 42, text_y, "Search apps...", t->txt_dim);
    } else {
        de_text(x + 42, text_y, query, t->widget_txt);
    }

    /* Blinking caret after the query. */
    if (open_flag) {
        int caret_x = x + 42 + de_text_width(query);
        uint32_t blink = (timer_get_ticks() / 25) & 1;
        if (blink) {
            de_fill(caret_x + 1, text_y + de_font_body.ascent, 2,
                    de_font_body.ascent, t->accent);
        }
    }
}

static void draw_results(void) {
    if (query_len == 0) return;

    const de_theme_t* t = g_theme;
    int x = sp_x();
    int y = sp_y() + SP_BAR_H;
    int w = SP_W;

    if (result_count == 0) {
        de_fill_rounded_alpha(x, y, w, SP_RESULT_H, 0, t->widget_bg, 235);
        de_fill_rounded_alpha(x, y, w, SP_RESULT_H, 12,
                              t->widget_bg, 235);
        de_border_rounded(x, y, w, SP_RESULT_H, 12, t->widget_edge);
        de_text(x + 16, y + (SP_RESULT_H - de_font_body.ascent) / 2,
                "No matching apps", t->txt_dim);
        return;
    }

    for (int i = 0; i < result_count; i++) {
        int ry = y + i * SP_RESULT_H;

        int is_sel = (i == selected);
        uint32_t bg_a = is_sel ? 220 : 235;

        /* Row background: rounded top on first, bottom on last. */
        int top_r    = (i == 0)                 ? 12 : 0;
        int bottom_r = (i == result_count - 1)  ? 12 : 0;

        /* Draw the row using a rounded rect that only rounds the
         * appropriate corners. Simplest: draw a plain rect then
         * overdraw the outer corners on first/last row. */
        if (is_sel) {
            de_fill_alpha(x + 4, ry, w - 8, SP_RESULT_H - 1,
                          t->accent, 200);
        } else {
            de_fill_alpha(x + 4, ry, w - 8, SP_RESULT_H - 1,
                          t->widget_bg, bg_a);
        }
        (void)top_r; (void)bottom_r;

        /* Icon sprite. */
        int icon_x = x + 14;
        int icon_y = ry + (SP_RESULT_H - 22) / 2;
        const app_t* a = app_registry_at(results[i].app_index);
        const de_image_t* icon = a ? a->icon_image : &img_settings_icon;
        if (icon) {
            de_image_blit_scaled(icon, icon_x, icon_y, 22, 22, 255);
        } else {
            de_fill_rounded(icon_x, icon_y, 22, 22, 5, t->accent);
        }

        /* Name. */
        uint32_t fg = is_sel ? 0x00FFFFFF : t->widget_txt;
        de_text(x + 46, ry + (SP_RESULT_H - de_font_body.ascent) / 2,
                results[i].name, fg);

        /* Dim the app id on the right. */
        int idw = de_text_width(results[i].id);
        uint32_t dim = is_sel ? 0x00DDE6F0 : t->txt_dim;
        de_text(x + w - idw - 16,
                ry + (SP_RESULT_H - de_font_body.ascent) / 2,
                results[i].id, dim);
    }
}

void de_spotlight_draw(void) {
    if (!open_flag) return;

    /* Dim the desktop behind. */
    de_fill_alpha(0, 0, de_fb_w(), de_fb_h(), 0x000000, 100);

    draw_bar();
    draw_results();
}