#include <quark/de.h>
#include <quark/de_font.h>

extern uint32_t fb_width;
extern uint32_t fb_height;
extern uint32_t* framebuffer_pixels;

static uint32_t* bb = NULL;
static uint32_t  bb_w = 0, bb_h = 0;
static uint8_t   bb_static = 0;

volatile uint8_t g_frame_dirty = 1;
void de_mark_dirty(void) { g_frame_dirty = 1; }

/* Accessors implemented in de_damage.c */
int de_clip_get_x(void);
int de_clip_get_y(void);
int de_clip_get_w(void);
int de_clip_get_h(void);
int de_clip_is_active(void);

int de_fb_w(void)          { return (int)bb_w; }
int de_fb_h(void)          { return (int)bb_h; }
int de_screen_bottom(void) { return de_fb_h() - DE_DOCK_H; }

#define STATIC_BB_PIXELS (1024 * 768)
static uint32_t static_backbuffer[STATIC_BB_PIXELS];

int de_backbuffer_alloc(void) {
    uint32_t pitch_px = fb_pitch_get() / 4;
    if (pitch_px < fb_width) pitch_px = fb_width;
    uint32_t bytes = pitch_px * fb_height * sizeof(uint32_t);

    uint32_t* buf = (uint32_t*)kmalloc(bytes);
    if (buf) {
        bb = buf; bb_w = pitch_px; bb_h = fb_height; bb_static = 0;
        kprintf("[DE] Backbuffer: heap %u bytes (%ux%u)\n",
                bytes, fb_width, fb_height);
        return 0;
    }
    if (fb_width > 1024 || fb_height > 768) {
        kprintf("[DE] FATAL: %ux%u exceeds static fallback\n",
                fb_width, fb_height);
        return -1;
    }
    bb = static_backbuffer; bb_w = fb_width; bb_h = fb_height; bb_static = 1;
    return 0;
}

void de_backbuffer_release(void) {
    if (bb && !bb_static) kfree(bb);
    bb = NULL; bb_w = bb_h = 0; bb_static = 0;
}

void de_backbuffer_clear(void) {
    if (!bb) return;
    kmemset(bb, 0, bb_w * bb_h * sizeof(uint32_t));
}

uint32_t de_blend(uint32_t a, uint32_t b, uint8_t t) {
    uint32_t ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF;
    uint32_t br = (b >> 16) & 0xFF, bg = (b >> 8) & 0xFF, bbv = b & 0xFF;
    uint32_t r = (ar * (255 - t) + br * t) / 255;
    uint32_t g = (ag * (255 - t) + bg * t) / 255;
    uint32_t bl = (ab * (255 - t) + bbv * t) / 255;
    return (r << 16) | (g << 8) | bl;
}

/* Compute the intersection of the caller's rect with the clip rect and the
 * backbuffer bounds. Writes the result into x0,y0,x1,y1. */
static int clip_rect(int x, int y, int w, int h,
                     int* x0, int* y0, int* x1, int* y1) {
    int cx = 0, cy = 0, cw = (int)bb_w, ch = (int)bb_h;
    if (de_clip_is_active()) {
        cx = de_clip_get_x();
        cy = de_clip_get_y();
        cw = de_clip_get_w();
        ch = de_clip_get_h();
    }
    int rx0 = x, ry0 = y, rx1 = x + w, ry1 = y + h;
    if (rx0 < cx) rx0 = cx;
    if (ry0 < cy) ry0 = cy;
    if (rx1 > cx + cw) rx1 = cx + cw;
    if (ry1 > cy + ch) ry1 = cy + ch;
    if (rx0 < 0) rx0 = 0;
    if (ry0 < 0) ry0 = 0;
    if (rx1 > (int)bb_w) rx1 = (int)bb_w;
    if (ry1 > (int)bb_h) ry1 = (int)bb_h;
    if (rx0 >= rx1 || ry0 >= ry1) return 0;
    *x0 = rx0; *y0 = ry0; *x1 = rx1; *y1 = ry1;
    return 1;
}

void de_fill(int x, int y, int w, int h, uint32_t color) {
    if (!bb || w <= 0 || h <= 0) return;
    int x0, y0, x1, y1;
    if (!clip_rect(x, y, w, h, &x0, &y0, &x1, &y1)) return;
    for (int yy = y0; yy < y1; yy++) {
        uint32_t* row = bb + (uint32_t)yy * bb_w;
        for (int xx = x0; xx < x1; xx++) row[xx] = color;
    }
}

void de_fill_alpha(int x, int y, int w, int h, uint32_t color, uint8_t a) {
    if (!bb || w <= 0 || h <= 0) return;
    if (a == 0) return;
    if (a == 255) { de_fill(x, y, w, h, color); return; }
    int x0, y0, x1, y1;
    if (!clip_rect(x, y, w, h, &x0, &y0, &x1, &y1)) return;
    for (int yy = y0; yy < y1; yy++) {
        uint32_t* row = bb + (uint32_t)yy * bb_w;
        for (int xx = x0; xx < x1; xx++)
            row[xx] = de_blend(row[xx], color, a);
    }
}

/* Approximate sqrt for AA corner coverage. Newton iterations, converges in
 * ~5 steps to ~1% error for values we care about (0..1000). */
static float fast_sqrt(float x) {
    if (x <= 0.0f) return 0.0f;
    float g = x * 0.5f + 0.5f;
    for (int i = 0; i < 5; i++) g = 0.5f * (g + x / g);
    return g;
}

/* Correct rounded-rect decomposition:
 *
 *   +---------------------+
 *   | TL  |  top strip | TR|
 *   +-----+------------+--+
 *   |     |            |  |
 *   | left|   center   |rt|
 *   |     |            |  |
 *   +-----+------------+--+
 *   | BL  |  bot strip | BR|
 *   +---------------------+
 *
 * Strips are drawn fully opaque. Only TL/TR/BL/BR get per-pixel coverage.
 * This avoids the "square bite" artifact at strip/corner boundaries. */
void de_fill_rounded_alpha(int x, int y, int w, int h, int r,
                           uint32_t color, uint8_t a) {
    if (w <= 0 || h <= 0 || a == 0) return;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    if (r <= 0) { de_fill_alpha(x, y, w, h, color, a); return; }

    /* Center. */
    de_fill_alpha(x + r, y + r, w - 2 * r, h - 2 * r, color, a);

    /* Strips. */
    de_fill_alpha(x + r,     y,         w - 2 * r, r,         color, a);
    de_fill_alpha(x + r,     y + h - r, w - 2 * r, r,         color, a);
    de_fill_alpha(x,         y + r,     r,         h - 2 * r, color, a);
    de_fill_alpha(x + w - r, y + r,     r,         h - 2 * r, color, a);

    /* Corner arcs. */
    float fr = (float)r;
    for (int cy = 0; cy < r; cy++) {
        for (int cx = 0; cx < r; cx++) {
            float dx = (float)cx - fr + 0.5f;
            float dy = (float)cy - fr + 0.5f;
            float d  = fast_sqrt(dx * dx + dy * dy);
            float cov = fr + 0.5f - d;
            if (cov <= 0.0f) continue;
            if (cov > 1.0f) cov = 1.0f;
            uint8_t ca = (uint8_t)(cov * (float)a);
            if (ca == 0) continue;
            de_fill_alpha(x + cx,         y + cy,         1, 1, color, ca);
            de_fill_alpha(x + w - 1 - cx, y + cy,         1, 1, color, ca);
            de_fill_alpha(x + cx,         y + h - 1 - cy, 1, 1, color, ca);
            de_fill_alpha(x + w - 1 - cx, y + h - 1 - cy, 1, 1, color, ca);
        }
    }
}

void de_fill_rounded(int x, int y, int w, int h, int r, uint32_t color) {
    de_fill_rounded_alpha(x, y, w, h, r, color, 255);
}

void de_fill_circle_alpha(int cx, int cy, int r, uint32_t color, uint8_t a) {
    if (r <= 0) return;
    float fr = (float)r;
    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            float ddx = (float)dx;
            float ddy = (float)dy;
            float d   = fast_sqrt(ddx * ddx + ddy * ddy);
            float cov = fr + 0.5f - d;
            if (cov <= 0.0f) continue;
            if (cov > 1.0f) cov = 1.0f;
            uint8_t ca = (uint8_t)(cov * (float)a);
            if (ca == 0) continue;
            de_fill_alpha(cx + dx, cy + dy, 1, 1, color, ca);
        }
    }
}

void de_fill_circle(int cx, int cy, int r, uint32_t color) {
    de_fill_circle_alpha(cx, cy, r, color, 255);
}

void de_vgradient(int x, int y, int w, int h, uint32_t top, uint32_t bot) {
    if (!bb || h <= 0) return;
    int y0, x0, x1, y1;
    if (!clip_rect(x, y, w, h, &x0, &y0, &x1, &y1)) return;
    for (int yy = y0; yy < y1; yy++) {
        uint8_t t = (uint8_t)((yy - y) * 255 / (h > 1 ? h - 1 : 1));
        uint32_t c = de_blend(top, bot, t);
        uint32_t* row = bb + (uint32_t)yy * bb_w;
        for (int xx = x0; xx < x1; xx++) row[xx] = c;
    }
}

void de_hline(int x, int y, int w, uint32_t c) { de_fill(x, y, w, 1, c); }
void de_vline(int x, int y, int h, uint32_t c) { de_fill(x, y, 1, h, c); }
void de_border(int x, int y, int w, int h, uint32_t c) {
    de_hline(x, y, w, c);
    de_hline(x, y + h - 1, w, c);
    de_vline(x, y, h, c);
    de_vline(x + w - 1, y, h, c);
}

void de_border_rounded(int x, int y, int w, int h, int r, uint32_t c) {
    if (w <= 0 || h <= 0) return;
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    if (r <= 0) { de_border(x, y, w, h, c); return; }

    /* Straight edges. */
    de_hline(x + r, y,         w - 2 * r, c);
    de_hline(x + r, y + h - 1, w - 2 * r, c);
    de_vline(x,         y + r, h - 2 * r, c);
    de_vline(x + w - 1, y + r, h - 2 * r, c);

    /* Corner arcs: trace the ring at radius (r - 1). */
    float fr   = (float)r;
    float ring = fr - 1.0f;
    for (int cy = 0; cy < r; cy++) {
        for (int cx = 0; cx < r; cx++) {
            float dx = (float)cx - fr + 0.5f;
            float dy = (float)cy - fr + 0.5f;
            float d  = fast_sqrt(dx * dx + dy * dy);
            if (d > ring + 0.5f) continue;
            if (d < ring - 0.5f) continue;
            de_fill(x + cx,         y + cy,         1, 1, c);
            de_fill(x + w - 1 - cx, y + cy,         1, 1, c);
            de_fill(x + cx,         y + h - 1 - cy, 1, 1, c);
            de_fill(x + w - 1 - cx, y + h - 1 - cy, 1, 1, c);
        }
    }
}

void de_line(int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = x1 - x0; if (dx < 0) dx = -dx;
    int dy = y1 - y0; if (dy < 0) dy = -dy;
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    while (1) {
        de_fill(x0, y0, 1, 1, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 <  dx) { err += dx; y0 += sy; }
    }
}

/* ===========================================================================
 * Soft drop shadow (layered rings, ~16 levels)
 * =========================================================================== */
void de_shadow_rect(int x, int y, int w, int h, int radius,
                    int blur, uint32_t color, uint8_t alpha) {
    if (blur <= 0 || alpha == 0 || w <= 0 || h <= 0) return;

    for (int i = 1; i <= blur; i++) {
        uint32_t a = ((uint32_t)alpha * (blur - i + 1)) / blur;
        if (a == 0) continue;

        int bx = x - i;
        int by = y - i + (i / 3);
        int bw = w + 2 * i;
        int bh = h + 2 * i - (i / 3);
        int rr = radius + i;

        /* Straight strips. */
        de_fill_alpha(bx + rr, by,          bw - 2 * rr, 1, color, (uint8_t)a);
        de_fill_alpha(bx + rr, by + bh - 1, bw - 2 * rr, 1, color, (uint8_t)a);
        de_fill_alpha(bx,      by + rr,     1, bh - 2 * rr, color, (uint8_t)a);
        de_fill_alpha(bx + bw - 1, by + rr, 1, bh - 2 * rr, color, (uint8_t)a);

        /* Corner arcs. */
        float fr   = (float)rr;
        float ring = fr - 1.0f;
        for (int cy = 0; cy < rr; cy++) {
            for (int cx = 0; cx < rr; cx++) {
                float dx = (float)cx - fr + 0.5f;
                float dy = (float)cy - fr + 0.5f;
                float d  = fast_sqrt(dx * dx + dy * dy);
                if (d > ring + 0.5f) continue;
                if (d < ring - 0.5f) continue;
                de_fill_alpha(bx + cx,             by + cy,             1, 1, color, (uint8_t)a);
                de_fill_alpha(bx + bw - 1 - cx,    by + cy,             1, 1, color, (uint8_t)a);
                de_fill_alpha(bx + cx,             by + bh - 1 - cy,    1, 1, color, (uint8_t)a);
                de_fill_alpha(bx + bw - 1 - cx,    by + bh - 1 - cy,    1, 1, color, (uint8_t)a);
            }
        }
    }
}

/* ===========================================================================
 * Box blur (two-pass, horizontal then vertical) for vibrancy.
 * =========================================================================== */
void de_blur_region(int x, int y, int w, int h, int radius) {
    if (!bb || w <= 0 || h <= 0 || radius <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)bb_w) w = (int)bb_w - x;
    if (y + h > (int)bb_h) h = (int)bb_h - y;
    if (w <= 0 || h <= 0) return;

    uint32_t scratch_pixels[256 * 128];
    uint32_t* scratch = scratch_pixels;
    int heap = 0;
    if (w * h > 256 * 128) {
        scratch = (uint32_t*)kmalloc((uint32_t)w * h * sizeof(uint32_t));
        if (!scratch) return;
        heap = 1;
    }

    for (int yy = 0; yy < h; yy++)
        kmemcpy(scratch + yy * w, bb + (uint32_t)(y + yy) * bb_w + x,
                (size_t)w * sizeof(uint32_t));

    /* Horizontal pass. */
    for (int yy = 0; yy < h; yy++) {
        uint32_t* row = scratch + yy * w;
        for (int xx = 0; xx < w; xx++) {
            uint32_t sr = 0, sg = 0, sb = 0;
            int n = 0;
            int lo = xx - radius; if (lo < 0) lo = 0;
            int hi = xx + radius; if (hi >= w) hi = w - 1;
            for (int k = lo; k <= hi; k++) {
                uint32_t c = row[k];
                sr += (c >> 16) & 0xFF;
                sg += (c >>  8) & 0xFF;
                sb +=  c        & 0xFF;
                n++;
            }
            row[xx] = ((sr / n) << 16) | ((sg / n) << 8) | (sb / n);
        }
    }

    /* Vertical pass. */
    for (int yy = 0; yy < h; yy++) {
        int lo = yy - radius; if (lo < 0) lo = 0;
        int hi = yy + radius; if (hi >= h) hi = h - 1;
        for (int xx = 0; xx < w; xx++) {
            uint32_t sr = 0, sg = 0, sb = 0;
            int n = 0;
            for (int k = lo; k <= hi; k++) {
                uint32_t c = scratch[k * w + xx];
                sr += (c >> 16) & 0xFF;
                sg += (c >>  8) & 0xFF;
                sb +=  c        & 0xFF;
                n++;
            }
            bb[(uint32_t)(y + yy) * bb_w + (x + xx)] =
                ((sr / n) << 16) | ((sg / n) << 8) | (sb / n);
        }
    }

    if (heap) kfree(scratch);
}

/* ===========================================================================
 * Text — delegates to the baked font
 * =========================================================================== */

int de_text_width(const char* s) {
    return de_font_text_width(&de_font_body, s);
}

void de_text(int x, int y, const char* s, uint32_t color) {
    /* Callers pass y as the top of the ascender box. Fonts draw from the
     * baseline, so shift down by the ascent. */
    de_font_draw_text(&de_font_body, x, y + de_font_body.ascent, s,
                      color, 255);
}

void de_text_clipped(int x, int y, const char* s, int max_w, uint32_t color) {
    if (max_w <= 0) return;
    /* Find how many characters fit in max_w (leaving room for ".."). */
    int total = 0;
    while (s[total]) total++;
    int n = total;
    while (n > 0 && de_font_text_width_n(&de_font_body, s, n) > max_w - 12) n--;
    if (n >= total) {
        de_text(x, y, s, color);
        return;
    }
    if (n < 3) return;
    char buf[128];
    int i;
    for (i = 0; i < n - 2 && i < 126; i++) buf[i] = s[i];
    buf[i++] = '.';
    buf[i++] = '.';
    buf[i] = '\0';
    de_text(x, y, buf, color);
}

void de_text_centered(int cx, int y, const char* s, uint32_t color) {
    int w = de_text_width(s);
    de_text(cx - w / 2, y, s, color);
}

/* ===========================================================================
 * Cursor
 * =========================================================================== */
static const uint8_t cur_fill[16] = {
    0b10000000, 0b11000000, 0b11100000, 0b11110000,
    0b11111000, 0b11111100, 0b11111110, 0b11111111,
    0b11111000, 0b11011000, 0b10001100, 0b00001100,
    0b00000110, 0b00000110, 0b00000000, 0b00000000,
};
static const uint8_t cur_outline[16] = {
    0b11000000, 0b11100000, 0b11110000, 0b11111000,
    0b11111100, 0b11111110, 0b11111111, 0b11111111,
    0b11111111, 0b11111100, 0b11011110, 0b10011110,
    0b00001111, 0b00001111, 0b00000110, 0b00000000,
};

void de_cursor_draw(void) {
    if (!bb) return;
    int mx = mouse_x, my = mouse_y;
    for (int y = 0; y < 16; y++) {
        int py = my + y;
        if (py < 0 || py >= (int)bb_h) continue;
        uint32_t* row = bb + (uint32_t)py * bb_w;
        uint8_t outline = cur_outline[y];
        uint8_t fill    = cur_fill[y];
        for (int x = 0; x < 16; x++) {
            int px = mx + x;
            if (px < 0 || px >= (int)bb_w) continue;
            if (!(outline & (0x80 >> x))) continue;
            row[px] = (fill & (0x80 >> x)) ? 0x00000000 : 0x00FFFFFF;
        }
    }
}

/* ===========================================================================
 * Blit
 * =========================================================================== */
void de_blit_to_screen(void) {
    if (!bb || !framebuffer_pixels) return;
    uint32_t pitch_px = fb_pitch_get() / 4;
    uint32_t cw = (bb_w < pitch_px) ? bb_w : pitch_px;
    uint32_t ch = (bb_h < fb_height) ? bb_h : fb_height;
    for (uint32_t y = 0; y < ch; y++) {
        kmemcpy(framebuffer_pixels + y * pitch_px,
                bb + y * bb_w,
                cw * sizeof(uint32_t));
    }
}

void de_blit_region(int x, int y, int w, int h) {
    if (!bb || !framebuffer_pixels) return;
    int x0 = x, y0 = y, x1 = x + w, y1 = y + h;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > de_fb_w()) x1 = de_fb_w();
    if (y1 > de_fb_h()) y1 = de_fb_h();
    if (x0 >= x1 || y0 >= y1) return;
    uint32_t pitch_px = fb_pitch_get() / 4;
    for (int yy = y0; yy < y1; yy++) {
        kmemcpy(framebuffer_pixels + (uint32_t)yy * pitch_px + x0,
                bb + (uint32_t)yy * bb_w + x0,
                (size_t)(x1 - x0) * sizeof(uint32_t));
    }
}

uint32_t* de_bb_ptr(void) { return bb; }
int       de_bb_w(void)   { return (int)bb_w; }
int       de_bb_h(void)   { return (int)bb_h; }