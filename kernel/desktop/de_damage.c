/* =============================================================================
 * Quark-OS kernel/desktop/de_damage.c
 * Damage tracking + drawing clip rect.
 * ============================================================================= */

#include <quark/de.h>
#include <quark/de_damage.h>

/* ---- Damage list ------------------------------------------------------- */
static de_rect_t damage[DE_MAX_DAMAGE];
static int       damage_count = 0;

/* ---- Clip rect --------------------------------------------------------- */
static int clip_x = 0, clip_y = 0;
static int clip_w = 0, clip_h = 0;
static int clip_active = 0;

void de_clip_set(int x, int y, int w, int h) {
    clip_x = x; clip_y = y; clip_w = w; clip_h = h;
    clip_active = 1;
}

void de_clip_reset(void) {
    clip_active = 0;
}

/* Internal accessors used by de_widgets.c */
int  de_clip_get_x(void) { return clip_active ? clip_x : 0; }
int  de_clip_get_y(void) { return clip_active ? clip_y : 0; }
int  de_clip_get_w(void) { return clip_active ? clip_w : de_fb_w(); }
int  de_clip_get_h(void) { return clip_active ? clip_h : de_fb_h(); }
int  de_clip_is_active(void) { return clip_active; }

/* ---- Damage list management ------------------------------------------- */
void de_damage_clear(void) {
    damage_count = 0;
}

int de_damage_count(void) { return damage_count; }

void de_damage_add_all(void) {
    damage_count = 0;
    damage[damage_count].x = 0;
    damage[damage_count].y = 0;
    damage[damage_count].w = de_fb_w();
    damage[damage_count].h = de_fb_h();
    damage_count = 1;
}

void de_damage_add(int x, int y, int w, int h) {
    /* Clip against screen. */
    int sw = de_fb_w();
    int sh = de_fb_h();
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > sw) w = sw - x;
    if (y + h > sh) h = sh - y;
    if (w <= 0 || h <= 0) return;

    /* Try to coalesce with an existing rect that touches or overlaps. */
    for (int i = 0; i < damage_count; i++) {
        de_rect_t* r = &damage[i];
        int rx1 = r->x + r->w, ry1 = r->y + r->h;
        int nx1 = x + w, ny1 = y + h;
        int overlaps = !(nx1 <= r->x || x >= rx1 || ny1 <= r->y || y >= ry1);

        if (overlaps) {
            /* Union */
            int ux = x < r->x ? x : r->x;
            int uy = y < r->y ? y : r->y;
            int ux2 = nx1 > rx1 ? nx1 : rx1;
            int uy2 = ny1 > ry1 ? ny1 : ry1;
            r->x = ux;
            r->y = uy;
            r->w = ux2 - ux;
            r->h = uy2 - uy;
            return;
        }
    }

    /* If the list is full, fall back to redrawing the whole screen. */
    if (damage_count >= DE_MAX_DAMAGE) {
        de_damage_add_all();
        return;
    }

    damage[damage_count].x = x;
    damage[damage_count].y = y;
    damage[damage_count].w = w;
    damage[damage_count].h = h;
    damage_count++;
}

/* Iterate all rects. Call de_damage_clear() when done. */
static int iter_index = 0;

int de_damage_next(de_rect_t* out_rect) {
    if (iter_index >= damage_count) {
        iter_index = 0;
        return 0;
    }
    *out_rect = damage[iter_index++];
    return 1;
}