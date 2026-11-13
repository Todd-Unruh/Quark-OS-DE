/* =============================================================================
 * Quark-OS kernel/desktop/apps/monitor.c
 * Live desktop app and memory monitor.
 * ============================================================================= */

#include <quark/de_apps.h>
#include <quark/de_sprites.h>
#include <quark/de_font.h>

#define MONITOR_MAX_APPS DE_MAX_WINDOWS
#define MONITOR_ROW_H     16

typedef struct {
    const char* name;
    uint8_t minimized;
} monitor_app_entry_t;

typedef struct {
    uint32_t refresh_tick;
    uint32_t idle_tick;
    uint32_t cpu_percent;
    uint32_t heap_used;
    uint32_t heap_free;
    uint32_t pmm_total;
    uint32_t pmm_free;
    uint32_t ramfs_used;
    uint32_t app_count;
    monitor_app_entry_t apps[MONITOR_MAX_APPS];
    int scroll;
} monitor_state_t;

extern uint32_t timer_get_ticks(void);

static void append_u32(char* out, int* at, uint32_t value) {
    char digits[11];
    int count = 0;
    do {
        digits[count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value && count < (int)sizeof(digits));
    while (count > 0) out[(*at)++] = digits[--count];
}

static void append_text(char* out, int* at, const char* text) {
    while (*text) out[(*at)++] = *text++;
}

static void format_mb(char* out, uint32_t bytes) {
    uint32_t tenths = (bytes / (1024u * 1024u)) * 10u
                    + ((bytes % (1024u * 1024u)) * 10u)
                      / (1024u * 1024u);
    int at = 0;
    append_u32(out, &at, tenths / 10u);
    out[at++] = '.';
    out[at++] = (char)('0' + tenths % 10u);
    out[at++] = ' ';
    out[at++] = 'M';
    out[at++] = 'B';
    out[at] = '\0';
}

static void format_u32(char* out, uint32_t value) {
    int at = 0;
    append_u32(out, &at, value);
    out[at] = '\0';
}

static uint32_t pages_to_bytes(uint32_t pages) {
    return pages * PAGE_SIZE;
}

static void format_storage(char* out, uint32_t bytes) {
    int at = 0;
    if (bytes < 10u * 1024u) {
        append_u32(out, &at, bytes);
        append_text(out, &at, " B");
    } else {
        uint32_t tenths = (bytes / 1024u) * 10u
                        + (bytes % 1024u) * 10u / 1024u;
        append_u32(out, &at, tenths / 10u);
        out[at++] = '.';
        out[at++] = (char)('0' + tenths % 10u);
        append_text(out, &at, " KB");
    }
    out[at] = '\0';
}

static uint32_t percent_of(uint32_t value, uint32_t total) {
    if (total == 0) return 0;
    if (value >= total) return 100;

    uint32_t whole = total / 100u;
    uint32_t remainder = total % 100u;
    for (uint32_t percent = 99; percent > 0; percent--) {
        uint32_t threshold = whole * percent
                           + (remainder * percent + 99u) / 100u;
        if (value >= threshold) return percent;
    }
    return 0;
}

static void monitor_refresh(monitor_state_t* s, uint32_t now) {
    uint32_t heap_used = 0, heap_free = 0;
    heap_get_stats(&heap_used, &heap_free);
    s->heap_used = heap_used;
    s->heap_free = heap_free;
    pmm_get_stats(&s->pmm_total, &s->pmm_free);
    s->ramfs_used = ramfs_total_bytes();

    uint32_t elapsed = now - s->refresh_tick;
    uint32_t idle_now = timer_get_idle_ticks();
    if (elapsed >= TIMER_HZ) {
        uint32_t idle_delta = idle_now - s->idle_tick;
        if (idle_delta > elapsed) idle_delta = elapsed;
        s->cpu_percent = (elapsed - idle_delta) * 100u / elapsed;
        s->idle_tick = idle_now;
    }

    s->app_count = 0;
    for (int i = 0; i < DE_MAX_WINDOWS &&
                    s->app_count < MONITOR_MAX_APPS; i++) {
        const window_t* win = &g_windows[i];
        if (!win->is_active || !win->app) continue;
        monitor_app_entry_t* entry = &s->apps[s->app_count++];
        entry->name = win->app->name;
        entry->minimized = win->is_minimized;
    }
    s->refresh_tick = now;
}

static void* monitor_open(void) {
    monitor_state_t* s = (monitor_state_t*)kmalloc(sizeof(*s));
    if (!s) return NULL;
    kmemset(s, 0, sizeof(*s));
    s->refresh_tick = timer_get_ticks();
    s->idle_tick = timer_get_idle_ticks();
    monitor_refresh(s, s->refresh_tick);
    return s;
}

static void monitor_close(void* st) {
    if (st) kfree(st);
}

static int monitor_tick(void* st, window_t* w, uint32_t now) {
    (void)w;
    monitor_state_t* s = (monitor_state_t*)st;
    if (!s || now - s->refresh_tick < TIMER_HZ) return 0;
    monitor_refresh(s, now);
    return 1;
}

static void draw_bar(int x, int y, int width, uint32_t percent,
                     uint32_t color, const de_theme_t* t) {
    if (percent > 100) percent = 100;
    de_fill_rounded(x, y, width, 6, 3, t->app_alt_bg);
    int fill = (int)((uint32_t)width * percent / 100u);
    if (fill > 0) de_fill_rounded(x, y, fill, 6, 3, color);
}

static void draw_memory_row(int x, int y, int width, const char* label,
                            uint32_t used, uint32_t total,
                            const de_theme_t* t) {
    char used_text[20], total_text[20];
    format_mb(used_text, used);
    format_mb(total_text, total);

    de_text(x, y, label, t->app_fg);
    int right = x + width - de_text_width(total_text);
    de_text(right, y, total_text, t->txt_dim);
    y += 15;

    de_text(x, y, used_text, t->txt_dim);
    uint32_t pct = percent_of(used, total);
    draw_bar(x + 76, y + 4, width - 76, pct, t->accent, t);
}

static void monitor_draw(void* st, window_t* w) {
    monitor_state_t* s = (monitor_state_t*)st;
    if (!s) return;
    const de_theme_t* t = g_theme;

    int x = w->x + 4;
    int y = w->y + DE_TITLEBAR_H + 4;
    int width = w->width - 8;
    int height = w->height - DE_TITLEBAR_H - 8;
    de_fill(x, y, width, height, t->app_bg);
    de_border(x, y, width, height, t->win_border);

    int pad = 12;
    int left = x + pad;
    int content_w = width - pad * 2;
    int top = y + 10;

    uint32_t uptime = timer_get_ticks() / TIMER_HZ;
    uint32_t hours = uptime / 3600u;
    uint32_t minutes = (uptime / 60u) % 60u;
    uint32_t seconds = uptime % 60u;
    char uptime_text[32];
    int up_at = 0;
    append_text(uptime_text, &up_at, "Uptime  ");
    append_u32(uptime_text, &up_at, hours);
    uptime_text[up_at++] = ':';
    uptime_text[up_at++] = (char)('0' + minutes / 10u);
    uptime_text[up_at++] = (char)('0' + minutes % 10u);
    uptime_text[up_at++] = ':';
    uptime_text[up_at++] = (char)('0' + seconds / 10u);
    uptime_text[up_at++] = (char)('0' + seconds % 10u);
    uptime_text[up_at] = '\0';
    de_text(left, top, "SYSTEM", t->accent);
    de_text(x + width - pad - de_text_width(uptime_text), top,
            uptime_text, t->txt_dim);

    char cpu_text[20];
    format_u32(cpu_text, s->cpu_percent);
    int cpu_x = x + width - pad - de_text_width(cpu_text) - 18;
    de_text(left, top + 16, "CPU activity", t->txt_dim);
    de_text(cpu_x, top + 16, cpu_text, t->app_fg);
    de_text(cpu_x + de_text_width(cpu_text) + 3, top + 16, "%", t->txt_dim);
    draw_bar(left, top + 32, content_w, s->cpu_percent, t->accent, t);

    int section_y = top + 46;
    de_hline(left, section_y, content_w, t->win_border);
    draw_memory_row(left, section_y + 8, content_w, "Kernel heap",
                    s->heap_used, s->heap_used + s->heap_free, t);

    uint32_t pmm_used = s->pmm_total - s->pmm_free;
    draw_memory_row(left, section_y + 42, content_w, "Physical memory",
                    pages_to_bytes(pmm_used), pages_to_bytes(s->pmm_total), t);

    char storage_text[32];
    format_storage(storage_text, s->ramfs_used);
    de_text(left, section_y + 76, "RamFS storage", t->app_fg);
    int storage_x = left + content_w - de_text_width(storage_text);
    de_text(storage_x, section_y + 76, storage_text, t->txt_dim);
    uint32_t storage_capacity = MAX_FILES * MAX_FILE_SIZE;
    uint32_t storage_percent = percent_of(s->ramfs_used, storage_capacity);
    draw_bar(left, section_y + 92, content_w, storage_percent,
             t->accent, t);

    int table_y = section_y + 111;
    de_hline(left, table_y, content_w, t->win_border);
    table_y += 8;
    de_text(left, table_y, "OPEN APPS", t->accent);
    char app_count[12];
    format_u32(app_count, s->app_count);
    int count_x = left + content_w - de_text_width(app_count);
    de_text(count_x, table_y, app_count, t->txt_dim);

    int header_y = table_y + 18;
    de_hline(left, header_y, content_w, t->win_border);
    de_text(left, header_y + 3, "APPLICATION", t->txt_dim);
    de_text(left + content_w - 68, header_y + 3, "STATUS", t->txt_dim);

    int rows_y = header_y + 19;
    int bottom = y + height - 8;
    int visible = (bottom - rows_y) / MONITOR_ROW_H;
    if (visible < 0) visible = 0;
    int max_scroll = (int)s->app_count - visible;
    if (max_scroll < 0) max_scroll = 0;
    if (s->scroll > max_scroll) s->scroll = max_scroll;

    for (int row = 0; row < visible; row++) {
        int index = s->scroll + row;
        if (index >= (int)s->app_count) break;
        int row_y = rows_y + row * MONITOR_ROW_H;
        if (row & 1)
            de_fill_alpha(left, row_y, content_w, MONITOR_ROW_H,
                          t->app_alt_bg, 100);

        monitor_app_entry_t* app = &s->apps[index];
        de_text_clipped(left, row_y + 3, app->name,
                content_w - 76, t->app_fg);
        const char* status = app->minimized ? "MINIMIZED" : "RUNNING";
        int status_x = left + content_w - de_text_width(status);
        de_text(status_x, row_y + 3, status, t->txt_dim);
    }
}

static void monitor_scroll(void* st, window_t* w, int dy) {
    (void)w;
    monitor_state_t* s = (monitor_state_t*)st;
    if (!s) return;
    s->scroll -= dy;
    if (s->scroll < 0) s->scroll = 0;
    if (s->scroll >= (int)s->app_count)
        s->scroll = s->app_count ? (int)s->app_count - 1 : 0;
}

const app_t app_monitor = {
    .id = "monitor",
    .name = "System Monitor",
    .icon_char = "M",
    .icon_image = &img_monitor_icon,
    .hide_from_dock = 1,
    .default_w = 360,
    .default_h = 390,
    .on_open = monitor_open,
    .on_close = monitor_close,
    .on_draw = monitor_draw,
    .on_click = NULL,
    .on_key = NULL,
    .on_scroll = monitor_scroll,
    .on_tick = monitor_tick,
};