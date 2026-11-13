#include <quark/de_font.h>
#include <quark/de.h>

extern uint32_t fb_width;
extern uint32_t fb_height;

/* The backbuffer is owned by de_widgets.c but we need direct write access
 * for speed. de_bb_ptr() gives us the base. */
static inline int in_bounds(int x, int y) {
    return x >= 0 && y >= 0 && x < de_bb_w() && y < de_bb_h();
}

// Cursor drawing function
extern uint32_t timer_get_ticks(void);



static const de_glyph_t* find_glyph(const de_font_t* f, uint16_t code) {
    if (code < f->cstart || code > f->cend) return NULL;
    /* glyphs are stored in codepoint order, so this is a direct index. */
    uint32_t i = code - f->cstart;
    if (i >= f->glyph_count) return NULL;
    return &f->glyphs[i];
}

static int kerning(const de_font_t* f, uint16_t left, uint16_t right) {
    for (uint32_t i = 0; i < f->kern_count; i++) {
        if (f->kerns[i].left == left && f->kerns[i].right == right)
            return (int)f->kerns[i].delta;
    }
    return 0;
}

int de_font_text_width(const de_font_t* f, const char* s) {
    int w = 0;
    uint16_t prev = 0;
    while (*s) {
        const de_glyph_t* g = find_glyph(f, (uint8_t)*s);
        if (!g) { s++; continue; }
        if (prev) w += kerning(f, prev, (uint8_t)*s);
        w += g->advance;
        prev = (uint8_t)*s;
        s++;
    }
    return w;
}

int de_font_text_width_n(const de_font_t* f, const char* s, int max_chars) {
    int w = 0;
    int n = 0;
    uint16_t prev = 0;
    while (*s && n < max_chars) {
        const de_glyph_t* g = find_glyph(f, (uint8_t)*s);
        if (!g) { s++; n++; continue; }
        if (prev) w += kerning(f, prev, (uint8_t)*s);
        w += g->advance;
        prev = (uint8_t)*s;
        s++; n++;
    }
    return w;
}

void de_font_draw_char(const de_font_t* f, int x, int y, char c,
                       uint32_t color, uint8_t alpha) {
    const de_glyph_t* g = find_glyph(f, (uint8_t)c);
    if (!g || g->w == 0) return;

    uint32_t* bb = de_bb_ptr();
    int bbw = de_bb_w();
    int bbh = de_bb_h();

    /* y is the baseline. Bitmap top-left is at (x + g->bx, y + g->by). */
    int bx = x + g->bx;
    int by = y + g->by;

    uint32_t cr = (color >> 16) & 0xFF;
    uint32_t cg = (color >>  8) & 0xFF;
    uint32_t cb =  color        & 0xFF;

    for (int row = 0; row < g->h; row++) {
        int py = by + row;
        if (py < 0 || py >= bbh) continue;
        uint32_t* dst = bb + (uint32_t)py * bbw;
        uint32_t src_off = g->offset + (uint32_t)row * g->w;
        for (int col = 0; col < g->w; col++) {
            int px = bx + col;
            if (px < 0 || px >= bbw) continue;
            uint8_t cov = f->atlas[src_off + col];
            if (cov == 0) continue;
            /* Combine glyph coverage with caller's alpha. */
            uint32_t a = ((uint32_t)cov * (uint32_t)alpha) / 255;
            if (a == 0) continue;
            uint32_t dst_px = dst[px];
            uint32_t dr = (dst_px >> 16) & 0xFF;
            uint32_t dg = (dst_px >>  8) & 0xFF;
            uint32_t db =  dst_px        & 0xFF;
            uint32_t r = (dr * (255 - a) + cr * a) / 255;
            uint32_t gg= (dg * (255 - a) + cg * a) / 255;
            uint32_t b = (db * (255 - a) + cb * a) / 255;
            dst[px] = (r << 16) | (gg << 8) | b;
        }
    }
}

void de_font_draw_text(const de_font_t* f, int x, int y, const char* s,
                       uint32_t color, uint8_t alpha) {
    uint16_t prev = 0;
    while (*s) {
        const de_glyph_t* g = find_glyph(f, (uint8_t)*s);
        if (!g) { s++; continue; }
        if (prev) x += kerning(f, prev, (uint8_t)*s);
        de_font_draw_char(f, x, y, *s, color, alpha);
        x += g->advance;
        prev = (uint8_t)*s;
        s++;
    }
}



void de_font_draw_caret(const de_font_t* f, int x, int baseline_y,
                        uint32_t color) {
    /* Blink at ~500ms on / 500ms off (25 ticks at 100Hz). */
    if (((timer_get_ticks() / 25) & 1) == 0) return;

    int top = baseline_y - f->ascent;
    int h = f->ascent;
    if (h < 6) h = 6;
    de_fill(x, top, 2, h, color);
}