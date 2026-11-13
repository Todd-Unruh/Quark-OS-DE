#include <quark/de_image.h>
#include <quark/de.h>

/* Backbuffer accessors */
uint32_t* de_bb_ptr(void);
int       de_bb_w(void);
int       de_bb_h(void);

/* Reuse the same fast_sqrt as de_widgets.c -- duplicated here to avoid a
 * cross-file dependency. It's static, so no link conflict. */
static float fast_sqrt(float x) {
    if (x <= 0.0f) return 0.0f;
    float g = x * 0.5f + 0.5f;
    for (int i = 0; i < 5; i++) g = 0.5f * (g + x / g);
    return g;
}

static inline uint32_t blend_px(uint32_t dst, uint32_t src, uint8_t a) {
    if (a == 0)   return dst;
    if (a == 255) return src & 0x00FFFFFF;
    uint32_t sr = (src >> 16) & 0xFF;
    uint32_t sg = (src >>  8) & 0xFF;
    uint32_t sb =  src        & 0xFF;
    uint32_t dr = (dst >> 16) & 0xFF;
    uint32_t dg = (dst >>  8) & 0xFF;
    uint32_t db =  dst        & 0xFF;
    uint32_t r = (dr * (255 - a) + sr * a) / 255;
    uint32_t g = (dg * (255 - a) + sg * a) / 255;
    uint32_t b = (db * (255 - a) + sb * a) / 255;
    return (r << 16) | (g << 8) | b;
}

void de_image_blit(const de_image_t* img, int x, int y, uint8_t alpha) {
    if (!img || !img->pixels || alpha == 0) return;
    uint32_t* bb = de_bb_ptr();
    if (!bb) return;
    int bbw = de_bb_w();
    int bbh = de_bb_h();

    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + img->w; if (x1 > bbw) x1 = bbw;
    int y1 = y + img->h; if (y1 > bbh) y1 = bbh;
    if (x0 >= x1 || y0 >= y1) return;

    for (int py = y0; py < y1; py++) {
        const uint32_t* src = img->pixels + (uint32_t)(py - y) * img->w;
        uint32_t* dst = bb + (uint32_t)py * bbw;
        for (int px = x0; px < x1; px++) {
            uint32_t s = src[px - x];
            uint32_t sa = (s >> 24) & 0xFF;
            if (sa == 0) continue;
            uint8_t a = (uint8_t)((sa * alpha) / 255);
            if (a == 0) continue;
            dst[px] = blend_px(dst[px], s, a);
        }
    }
}

void de_image_blit_scaled(const de_image_t* img, int x, int y,
                          int dw, int dh, uint8_t alpha) {
    if (!img || !img->pixels || alpha == 0 || dw <= 0 || dh <= 0) return;
    uint32_t* bb = de_bb_ptr();
    if (!bb) return;
    int bbw = de_bb_w();
    int bbh = de_bb_h();

    for (int dy = 0; dy < dh; dy++) {
        int py = y + dy;
        if (py < 0) continue;
        if (py >= bbh) break;
        int sy = dy * img->h / dh;
        if (sy >= img->h) sy = img->h - 1;
        const uint32_t* src = img->pixels + (uint32_t)sy * img->w;
        uint32_t* dst = bb + (uint32_t)py * bbw;
        for (int dx = 0; dx < dw; dx++) {
            int px = x + dx;
            if (px < 0) continue;
            if (px >= bbw) break;
            int sx = dx * img->w / dw;
            if (sx >= img->w) sx = img->w - 1;
            uint32_t s = src[sx];
            uint32_t sa = (s >> 24) & 0xFF;
            if (sa == 0) continue;
            uint8_t a = (uint8_t)((sa * alpha) / 255);
            if (a == 0) continue;
            dst[px] = blend_px(dst[px], s, a);
        }
    }
}

void de_image_blit_rounded(const de_image_t* img, int x, int y,
                           int dw, int dh, int radius, uint8_t alpha) {
    if (!img || !img->pixels || alpha == 0 || dw <= 0 || dh <= 0) return;
    if (radius > dw / 2) radius = dw / 2;
    if (radius > dh / 2) radius = dh / 2;
    if (radius <= 0) {
        de_image_blit_scaled(img, x, y, dw, dh, alpha);
        return;
    }

    uint32_t* bb = de_bb_ptr();
    if (!bb) return;
    int bbw = de_bb_w();
    int bbh = de_bb_h();

    float fr = (float)radius;
    for (int dy = 0; dy < dh; dy++) {
        int py = y + dy;
        if (py < 0) continue;
        if (py >= bbh) break;
        int sy = dy * img->h / dh;
        if (sy >= img->h) sy = img->h - 1;
        const uint32_t* src = img->pixels + (uint32_t)sy * img->w;
        uint32_t* dst = bb + (uint32_t)py * bbw;

        for (int dx = 0; dx < dw; dx++) {
            int px = x + dx;
            if (px < 0) continue;
            if (px >= bbw) break;

            /* Corner coverage test. */
            float cov = 1.0f;
            int in_l = dx < radius;
            int in_r = dx >= dw - radius;
            int in_t = dy < radius;
            int in_b = dy >= dh - radius;
            if ((in_l || in_r) && (in_t || in_b)) {
                float ccx = in_l ? (float)(radius - 1 - dx) : (float)(dx - (dw - radius));
                float ccy = in_t ? (float)(radius - 1 - dy) : (float)(dy - (dh - radius));
                /* Actually: measure from the arc center. */
                float acx = in_l ? fr : (float)(dw - radius) + 0.5f - fr;
                /* Simpler: distance from corner circle center. */
                float kx = in_l ? (float)dx - fr + 0.5f
                                : (float)dx - (float)(dw - radius) + fr - 0.5f;
                float ky = in_t ? (float)dy - fr + 0.5f
                                : (float)dy - (float)(dh - radius) + fr - 0.5f;
                (void)ccx; (void)ccy; (void)acx;
                float d = fast_sqrt(kx * kx + ky * ky);
                cov = fr + 0.5f - d;
                if (cov <= 0.0f) continue;
                if (cov > 1.0f) cov = 1.0f;
            }

            int sx = dx * img->w / dw;
            if (sx >= img->w) sx = img->w - 1;
            uint32_t s = src[sx];
            uint32_t sa = (s >> 24) & 0xFF;
            if (sa == 0) continue;

            uint32_t a = (uint32_t)sa * alpha / 255;
            a = (uint32_t)(a * cov);
            if (a == 0) continue;

            dst[px] = blend_px(dst[px], s, (uint8_t)a);
        }
    }
}