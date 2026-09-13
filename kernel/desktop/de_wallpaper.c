/* =============================================================================
 * Quark-OS kernel/desktop/de_wallpaper.c
 * KDE Plasma "Flow" wallpaper with a cached render buffer.
 * Damage-tracked: samples from cache per region instead of redrawing the
 * whole screen every frame.
 * ============================================================================= */

#include <quark/de.h>

extern uint32_t* de_bb_ptr(void);
extern int       de_bb_w(void);
extern int       de_bb_h(void);

static uint32_t* cache = NULL;
static int       cache_w = 0, cache_h = 0;
static uint32_t  cache_wall_id = 0xFFFFFFFFu;

/* --------------------------------------------------------------------------- */
/* Integer sine, period = 1024 units, output in [-512, 512]                    */
/* --------------------------------------------------------------------------- */
static int isin(int x) {
    static const int16_t q[65] = {
           0,   25,   50,   75,   99,  123,  147,  170,
         193,  215,  237,  258,  278,  298,  317,  335,
         353,  369,  385,  400,  414,  427,  439,  451,
         461,  470,  479,  486,  492,  497,  501,  504,
         506,  507,  507,  507,  505,  503,  500,  496,
         491,  486,  480,  474,  466,  459,  450,  441,
         432,  422,  411,  400,  389,  377,  365,  352,
         340,  327,  314,  301,  288,  275,  261,  248,
    };
    int neg = 0;
    x &= 1023;
    if (x >= 512) { x -= 512; neg = 1; }
    int idx, frac;
    if (x < 256) { idx = x / 4; frac = x % 4; }
    else {
        int m = 512 - x;
        idx = m / 4;
        frac = m % 4;
        if (idx >= 64) idx = 63;
    }
    int v = q[idx] + (q[idx + 1] - q[idx]) * frac / 4;
    return neg ? -v : v;
}

static uint32_t flow_palette(int t) {
    static const uint32_t stops[] = {
        0x000A5A6E, 0x00134A8C, 0x003D2E82,
        0x0090287A, 0x00C25A2E, 0x000A5A6E,
    };
    const int n = (int)(sizeof(stops) / sizeof(stops[0])) - 1;
    int pos = ((t % 1024) + 1024) % 1024;
    int seg = pos * n / 1024;
    if (seg >= n) seg = n - 1;
    int local = pos * n - seg * 1024;
    uint8_t f = (uint8_t)(local * 255 / 1024);
    return de_blend(stops[seg], stops[seg + 1], f);
}

static void render_flow(int w, int h) {
    for (int y = 0; y < h; y++) {
        uint32_t* row = cache + (uint32_t)y * w;
        int y1 = y * 11 / 91;
        int y2 = y * 22 / 47;
        for (int x = 0; x < w; x++) {
            int v1 = isin(x * 16 / 64 + y1);
            int v2 = isin(x *  8 / 123 - y2 + 512);
            int v3 = isin((x + y) * 5 / 191);
            int v  = (v1 + v2 + v3) / 3;
            int idx = 512 + v;
            if (idx < 0) idx = 0;
            if (idx > 1023) idx = 1023;
            row[x] = flow_palette(idx);
        }
    }
    for (int y = 0; y < h; y += 2) {
        uint32_t* row = cache + (uint32_t)y * w;
        for (int x = 0; x < w; x++) {
            uint32_t c = row[x];
            uint32_t r = (c >> 16) & 0xFF, g = (c >> 8) & 0xFF, b = c & 0xFF;
            r = (r + 12 > 255) ? 255 : r + 12;
            g = (g + 12 > 255) ? 255 : g + 12;
            b = (b + 12 > 255) ? 255 : b + 12;
            row[x] = (r << 16) | (g << 8) | b;
        }
    }
}

static void render_dark_gradient(int w, int h) {
    for (int y = 0; y < h; y++) {
        uint8_t t = (uint8_t)(y * 255 / (h > 1 ? h - 1 : 1));
        uint32_t c = de_blend(0x00101828, 0x00243850, t);
        uint32_t* row = cache + (uint32_t)y * w;
        for (int x = 0; x < w; x++) row[x] = c;
    }
}

static void render_light_gradient(int w, int h) {
    for (int y = 0; y < h; y++) {
        uint8_t t = (uint8_t)(y * 255 / (h > 1 ? h - 1 : 1));
        uint32_t c = de_blend(0x00D8E0EC, 0x00A8BCD8, t);
        uint32_t* row = cache + (uint32_t)y * w;
        for (int x = 0; x < w; x++) row[x] = c;
    }
}

static void render_solid(int w, int h, uint32_t color) {
    for (int y = 0; y < h; y++) {
        uint32_t* row = cache + (uint32_t)y * w;
        for (int x = 0; x < w; x++) row[x] = color;
    }
}

/* --------------------------------------------------------------------------- */
/* Ensure the cache is up to date for the current wallpaper id.                */
/* --------------------------------------------------------------------------- */
static void ensure_cache(void) {
    if (!cache) return;
    if (cache_wall_id == (uint32_t)g_gui_config.wallpaper) return;

    cache_wall_id = (uint32_t)g_gui_config.wallpaper;
    switch (g_gui_config.wallpaper) {
        case DE_WALL_FLOW:    render_flow(cache_w, cache_h);          break;
        case DE_WALL_DARK:    render_dark_gradient(cache_w, cache_h); break;
        case DE_WALL_LIGHT:   render_light_gradient(cache_w, cache_h);break;
        case DE_WALL_SOLID_A: render_solid(cache_w, cache_h, 0x00254A80); break;
        case DE_WALL_SOLID_B: render_solid(cache_w, cache_h, 0x001C2A3A); break;
        default:              render_flow(cache_w, cache_h);          break;
    }
}

/* --------------------------------------------------------------------------- */
void de_wallpaper_invalidate(void) {
    cache_wall_id = 0xFFFFFFFFu;
    de_damage_add_all();
}

void de_wallpaper_init(void) {
    cache_w = de_fb_w();
    cache_h = de_fb_h();
    if (cache_w <= 0 || cache_h <= 0) return;

    if (!cache) {
        uint32_t bytes = (uint32_t)cache_w * (uint32_t)cache_h * sizeof(uint32_t);
        cache = (uint32_t*)kmalloc(bytes);
        if (!cache) {
            kprintf("[DE] WARN: wallpaper cache alloc failed\n");
            return;
        }
        kprintf("[DE] Wallpaper cache: %u bytes (%dx%d)\n",
                bytes, cache_w, cache_h);
    }
    de_wallpaper_invalidate();
}

/* Full-screen blit. Kept for compatibility with old code paths. */
void de_wallpaper_draw(void) {
    ensure_cache();
    uint32_t* dst = de_bb_ptr();
    if (!dst || !cache) return;
    int bb_w = de_bb_w();
    int bb_h = de_bb_h();
    int rows = (cache_h < bb_h) ? cache_h : bb_h;
    int cols = (cache_w < bb_w) ? cache_w : bb_w;
    for (int y = 0; y < rows; y++) {
        kmemcpy(dst + (uint32_t)y * bb_w,
                cache + (uint32_t)y * cache_w,
                (size_t)cols * sizeof(uint32_t));
    }
}

/* Region blit, used by the damage-based compositor. */
void de_wallpaper_erase_region(int x, int y, int w, int h) {
    ensure_cache();

    uint32_t* dst = de_bb_ptr();
    if (!dst || !cache) return;

    int bb_w = de_bb_w();
    int bb_h = de_bb_h();
    int x0 = x, y0 = y, x1 = x + w, y1 = y + h;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > bb_w) x1 = bb_w;
    if (y1 > bb_h) y1 = bb_h;
    if (x0 >= x1 || y0 >= y1) return;
    if (x0 >= cache_w || y0 >= cache_h) return;
    if (x1 > cache_w) x1 = cache_w;
    if (y1 > cache_h) y1 = cache_h;

    for (int yy = y0; yy < y1; yy++) {
        kmemcpy(dst + (uint32_t)yy * bb_w + x0,
                cache + (uint32_t)yy * cache_w + x0,
                (size_t)(x1 - x0) * sizeof(uint32_t));
    }
}