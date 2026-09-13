/* =============================================================================
 * Quark-OS kernel/desktop/apps/editor.c
 * Simple text editor with RamFS save/load, arrow keys, and scrolling.
 * ============================================================================= */

#include <quark/de_apps.h>

#define ED_MAX     4096
#define ED_LINE    128
#define LINE_H     12

typedef struct {
    char buf[ED_MAX];
    int  len;
    int  cursor;
    int  scroll;
    char filename[32];
    int  dirty;
} editor_state_t;

static void ed_load(editor_state_t* s);
static void ed_save(editor_state_t* s);

/* Preload hook for Files. */
char g_editor_preload_name[32];

void editor_preload(const char* name) {
    int i = 0;
    while (name[i] && i < 31) { g_editor_preload_name[i] = name[i]; i++; }
    g_editor_preload_name[i] = '\0';
}

/* --------------------------------------------------------------------------- */
static void ed_new_blank(editor_state_t* s) {
    s->buf[0] = '\0';
    s->len = 0;
    s->cursor = 0;
    s->scroll = 0;
    kstrcpy(s->filename, "untitled.txt");
    s->dirty = 0;
}

static void* ed_open(void) {
    editor_state_t* s = (editor_state_t*)kmalloc(sizeof(editor_state_t));
    if (!s) return NULL;
    kmemset(s, 0, sizeof(*s));
    ed_new_blank(s);

    if (g_editor_preload_name[0]) {
        int i = 0;
        while (g_editor_preload_name[i] && i < 31) {
            s->filename[i] = g_editor_preload_name[i];
            i++;
        }
        s->filename[i] = '\0';
        g_editor_preload_name[0] = '\0';
        ed_load(s);
    }
    return s;
}

static void ed_close(void* st) { if (st) kfree(st); }

/* --------------------------------------------------------------------------- */
static int line_start(const editor_state_t* s, int pos) {
    while (pos > 0 && s->buf[pos - 1] != '\n') pos--;
    return pos;
}

static int line_end(const editor_state_t* s, int pos) {
    while (pos < s->len && s->buf[pos] != '\n') pos++;
    return pos;
}

static int cursor_line(const editor_state_t* s) {
    int line = 0;
    for (int i = 0; i < s->cursor && i < s->len; i++)
        if (s->buf[i] == '\n') line++;
    return line;
}

static void ensure_visible(editor_state_t* s, int max_rows) {
    if (max_rows < 1) max_rows = 1;
    int cl = cursor_line(s);
    if (cl < s->scroll) s->scroll = cl;
    if (cl >= s->scroll + max_rows) s->scroll = cl - max_rows + 1;
    if (s->scroll < 0) s->scroll = 0;
}

/* --------------------------------------------------------------------------- */
static void ed_draw(void* st, window_t* w) {
    editor_state_t* s = (editor_state_t*)st;
    if (!s) return;
    const de_theme_t* t = g_theme;

    int cx = w->x + 4;
    int cy = w->y + DE_TITLEBAR_H + 4;
    int cw = w->width - 8;
    int ch = w->height - DE_TITLEBAR_H - 8;

    de_fill(cx, cy, cw, ch, t->app_bg);
    de_border(cx, cy, cw, ch, t->win_border);

    int content_h = ch - 16;
    int max_rows = content_h / LINE_H;
    if (max_rows < 1) max_rows = 1;

    int cur_line = cursor_line(s);
    int total = s->len;
    if (total < 0) total = 0;
    if (total > ED_MAX) total = ED_MAX;

    int line = 0;
    int i = 0;
    int row = 0;
    int cursor_x = cx + 4;
    int cursor_y = cy + 3;

    while (i <= total && row <= max_rows + 1) {
        if (line >= s->scroll) {
            int y = cy + 3 + row * LINE_H;
            if (y + LINE_H > cy + content_h) break;

            int x = cx + 4;
            int col = 0;
            while (i + col < total && s->buf[i + col] != '\n' &&
                   col < ED_LINE) {
                char ch_str[2] = { s->buf[i + col], 0 };
                de_text(x + col * 8, y, ch_str, t->app_fg);
                col++;
            }
            if (line == cur_line) {
                cursor_x = x + col * 8;
                cursor_y = y;
            }
            row++;
        }

        int advanced = 0;
        while (i < total && s->buf[i] != '\n') { i++; advanced = 1; }
        if (i < total) { i++; advanced = 1; }
        if (!advanced) break;

        line++;
    }

    /* Blinking cursor */
    uint32_t blink = (timer_get_ticks() / 25) & 1;
    if (blink) de_fill(cursor_x, cursor_y, 2, 8, t->accent);

    /* Status bar */
    int sb_y = cy + ch - 14;
    de_fill(cx + 1, sb_y, cw - 2, 13, t->win_title_a);

    char status[64];
    int n = 0;
    const char* fn = s->filename[0] ? s->filename : "(unsaved)";
    while (fn[n] && n < 22) { status[n] = fn[n]; n++; }
    status[n++] = ' ';
    if (s->dirty) status[n++] = '*';
    else          status[n++] = ' ';
    status[n++] = ' ';
    status[n++] = ' ';
    const char* pf = "Ln ";
    int p = 0;
    while (pf[p]) status[n++] = pf[p++];
    int v = cur_line + 1;
    char tmp[8];
    int tt = 0;
    if (v == 0) tmp[tt++] = '0';
    while (v > 0) { tmp[tt++] = '0' + v % 10; v /= 10; }
    while (tt > 0) status[n++] = tmp[--tt];
    status[n] = '\0';
    de_text(cx + 4, sb_y + 3, status, t->txt_dark);
}

/* --------------------------------------------------------------------------- */
static void ed_insert(editor_state_t* s, char c) {
    if (s->len >= ED_MAX - 1) return;
    for (int i = s->len; i > s->cursor; i--) s->buf[i] = s->buf[i - 1];
    s->buf[s->cursor] = c;
    s->len++;
    s->cursor++;
    s->dirty = 1;
}

static void ed_backspace(editor_state_t* s) {
    if (s->cursor <= 0) return;
    for (int i = s->cursor - 1; i < s->len - 1; i++)
        s->buf[i] = s->buf[i + 1];
    s->len--;
    s->cursor--;
    s->dirty = 1;
}

static void ed_save(editor_state_t* s) {
    /* Make sure the file exists in RamFS before writing. */
    if (!ramfs_get(s->filename, "/")) {
        ramfs_create(s->filename, "/", 0);
    }
    int rc = ramfs_write(s->filename, "/", s->buf, s->len);
    kprintf("[ED] save '%s' len=%d rc=%d\n", s->filename, s->len, rc);
    if (rc == 0) {
        s->dirty = 0;
        de_notify("Editor", "Saved");
    } else {
        de_notify("Editor", "Save failed");
    }
}

static void ed_load(editor_state_t* s) {
    ram_file_t* f = ramfs_get(s->filename, "/");
    if (!f) {
        de_notify("Editor", "File not found");
        return;
    }
    int i = 0;
    while (i < (int)f->size && i < ED_MAX - 1) { s->buf[i] = f->data[i]; i++; }
    s->buf[i] = '\0';
    s->len = i;
    s->cursor = s->len;
    s->dirty = 0;
    de_notify("Editor", "Loaded");
}

/* --------------------------------------------------------------------------- */
static void ed_key(void* st, window_t* w, de_key_event_t* k) {
    editor_state_t* s = (editor_state_t*)st;
    if (!s || !k) return;

    int ch = w->height - DE_TITLEBAR_H - 8;
    int max_rows = (ch - 16) / LINE_H;
    if (max_rows < 1) max_rows = 1;

    if (k->ctrl) {
        if (k->ascii == 's' || k->ascii == 'S') { ed_save(s); return; }
        if (k->ascii == 'o' || k->ascii == 'O') { ed_load(s); return; }
    }

    if (k->extended) {
        switch (k->keycode) {
            case DE_KEY_LEFT:
                if (s->cursor > 0) s->cursor--;
                break;
            case DE_KEY_RIGHT:
                if (s->cursor < s->len) s->cursor++;
                break;
            case DE_KEY_UP: {
                int ls = line_start(s, s->cursor);
                if (ls == 0) break;
                int prev_e = ls - 1;
                int prev_s = line_start(s, prev_e);
                int col = s->cursor - ls;
                int prev_len = prev_e - prev_s;
                s->cursor = prev_s + (col < prev_len ? col : prev_len);
                break;
            }
            case DE_KEY_DOWN: {
                int le = line_end(s, s->cursor);
                if (le >= s->len) break;
                int next_s = le + 1;
                int next_e = line_end(s, next_s);
                int ls = line_start(s, s->cursor);
                int col = s->cursor - ls;
                int next_len = next_e - next_s;
                s->cursor = next_s + (col < next_len ? col : next_len);
                break;
            }
            case DE_KEY_HOME: s->cursor = line_start(s, s->cursor); break;
            case DE_KEY_END:  s->cursor = line_end(s, s->cursor);   break;
            case DE_KEY_DEL:
                if (s->cursor < s->len) {
                    for (int i = s->cursor; i < s->len - 1; i++)
                        s->buf[i] = s->buf[i + 1];
                    s->len--;
                    s->dirty = 1;
                }
                break;
            default:
                break;
        }
        ensure_visible(s, max_rows);
        return;
    }

    if (k->ascii == '\b')  { ed_backspace(s); ensure_visible(s, max_rows); return; }
    if (k->ascii == '\n')  { ed_insert(s, '\n'); ensure_visible(s, max_rows); return; }
    if (k->ascii >= 32 && k->ascii < 127) {
        ed_insert(s, k->ascii);
        ensure_visible(s, max_rows);
    }
}

const app_t app_editor = {
    .id = "editor",
    .name = "Text Editor",
    .icon_char = "E",
    .default_w = 500,
    .default_h = 340,
    .on_open = ed_open,
    .on_close = ed_close,
    .on_draw = ed_draw,
    .on_click = NULL,
    .on_key = ed_key,
    .on_scroll = NULL,
};