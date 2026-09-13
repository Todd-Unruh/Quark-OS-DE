/* =============================================================================
 * Quark-OS kernel/desktop/apps/files.c
 * File manager for RamFS: navigate, create, rename, delete, edit, forge.
 * Keyboard-driven, no right-click menu (yet).
 * ============================================================================= */

#include <quark/de_apps.h>
#include <quark/forge.h>

#define FILES_ROW_H     18
#define FILES_MAX_ENT   64
#define FILES_NAME_MAX  32
#define INPUT_MAX       48

/* --------------------------------------------------------------------------- */
/* State                                                                       */
/* --------------------------------------------------------------------------- */
typedef struct {
    char name[FILES_NAME_MAX];
    uint8_t is_dir;
    uint32_t size;
} file_entry_t;

typedef enum {
    MODAL_NONE = 0,
    MODAL_NEW_FILE,
    MODAL_NEW_FOLDER,
    MODAL_RENAME,
    MODAL_CONFIRM_DELETE,
    MODAL_MESSAGE
} modal_kind_t;

typedef struct {
    int  scroll_y;
    int  selected;
    int  count;

    file_entry_t entries[FILES_MAX_ENT];

    char cwd[FILES_NAME_MAX];   /* "/" or "subdir" (single level) */

    /* Modal state */
    modal_kind_t modal;
    char  input[INPUT_MAX];
    int   input_len;
    char  prompt[64];
    char  target_name[FILES_NAME_MAX];   /* for rename / delete */
} files_state_t;

extern void editor_preload(const char* name);

/* --------------------------------------------------------------------------- */
/* Refresh                                                                     */
/* --------------------------------------------------------------------------- */

/* Compare: dirs sort before files; within a type, alphabetical. */
static int entry_cmp(const file_entry_t* a, const file_entry_t* b) {
    if (a->is_dir != b->is_dir) return a->is_dir ? -1 : 1;
    return kstrcmp(a->name, b->name);
}

static void refresh(files_state_t* s) {
    /* Preserve selection by name if possible. */
    char keep_name[FILES_NAME_MAX];
    int  have_keep = 0;
    if (s->selected >= 0 && s->selected < s->count) {
        kstrcpy(keep_name, s->entries[s->selected].name);
        have_keep = 1;
    }

    s->count = 0;

    /* Read every child of cwd. */
    for (int i = 0; i < MAX_FILES && s->count < FILES_MAX_ENT; i++) {
        ram_file_t* f = ramfs_get_child(s->cwd, i);
        if (!f) break;
        int n = 0;
        while (f->name[n] && n < FILES_NAME_MAX - 1) {
            s->entries[s->count].name[n] = f->name[n];
            n++;
        }
        s->entries[s->count].name[n] = '\0';
        s->entries[s->count].is_dir = f->is_dir;
        s->entries[s->count].size   = f->size;
        s->count++;
    }

    /* Sort: dirs first, then alphabetical. Insertion sort — small arrays. */
    for (int i = 1; i < s->count; i++) {
        file_entry_t key = s->entries[i];
        int j = i - 1;
        while (j >= 0 && entry_cmp(&s->entries[j], &key) > 0) {
            s->entries[j + 1] = s->entries[j];
            j--;
        }
        s->entries[j + 1] = key;
    }

    /* Restore selection. */
    s->selected = -1;
    if (have_keep) {
        for (int i = 0; i < s->count; i++) {
            if (kstrcmp(s->entries[i].name, keep_name) == 0) {
                s->selected = i;
                break;
            }
        }
    }
    if (s->selected < 0 && s->count > 0) s->selected = 0;
}

/* --------------------------------------------------------------------------- */
/* Open / close                                                                */
/* --------------------------------------------------------------------------- */
static void* files_open(void) {
    files_state_t* s = (files_state_t*)kmalloc(sizeof(files_state_t));
    if (!s) return NULL;
    kmemset(s, 0, sizeof(*s));
    kstrcpy(s->cwd, "/");
    s->selected = -1;
    refresh(s);
    return s;
}

static void files_close(void* st) {
    if (st) kfree(st);
}

/* --------------------------------------------------------------------------- */
/* Helpers                                                                     */
/* --------------------------------------------------------------------------- */
static int selected_is_valid(files_state_t* s) {
    return s->selected >= 0 && s->selected < s->count;
}

static void begin_modal(files_state_t* s, modal_kind_t kind,
                        const char* prompt, const char* initial) {
    s->modal = kind;
    int n = 0;
    while (prompt[n] && n < (int)sizeof(s->prompt) - 1) {
        s->prompt[n] = prompt[n]; n++;
    }
    s->prompt[n] = '\0';
    s->input_len = 0;
    if (initial) {
        int i = 0;
        while (initial[i] && i < INPUT_MAX - 1) {
            s->input[i] = initial[i]; i++;
        }
        s->input_len = i;
    }
    s->input[s->input_len] = '\0';
}

static void end_modal(files_state_t* s) {
    s->modal = MODAL_NONE;
    s->input[0] = '\0';
    s->input_len = 0;
}

/* --------------------------------------------------------------------------- */
/* Actions                                                                     */
/* --------------------------------------------------------------------------- */
static void action_open(files_state_t* s) {
    if (!selected_is_valid(s)) return;
    file_entry_t* e = &s->entries[s->selected];

    if (e->is_dir) {
        /* Enter the directory. We use a flat cwd model: the subdir name
         * becomes the new cwd, and ".." takes us back to "/". This only
         * supports one nesting level, which matches the rest of the OS. */
        kstrcpy(s->cwd, e->name);
        s->selected = -1;
        s->scroll_y = 0;
        refresh(s);
        return;
    }

    /* Open file in editor. */
    editor_preload(e->name);
    de_app_open("editor");
}

static void action_edit(files_state_t* s) {
    if (!selected_is_valid(s)) return;
    file_entry_t* e = &s->entries[s->selected];
    if (e->is_dir) return;
    editor_preload(e->name);
    de_app_open("editor");
}

static void action_forge(files_state_t* s) {
    if (!selected_is_valid(s)) return;
    file_entry_t* e = &s->entries[s->selected];
    if (e->is_dir) return;

    /* Check .fg extension. */
    int len = (int)kstrlen(e->name);
    if (len < 3 ||
        e->name[len - 3] != '.' ||
        e->name[len - 2] != 'f' ||
        e->name[len - 1] != 'g') {
        begin_modal(s, MODAL_MESSAGE, "Not a .fg file", NULL);
        return;
    }

    ram_file_t* f = ramfs_get(e->name, s->cwd);
    if (!f) {
        begin_modal(s, MODAL_MESSAGE, "File disappeared", NULL);
        return;
    }

    /* Log start + end to the shared console so the user can find the output
     * in the terminal workspace. */
    char line[64];
    int p = 0;
    const char* pre = "forge: ";
    while (*pre && p < 60) line[p++] = *pre++;
    int q = 0;
    while (e->name[q] && p < 62) line[p++] = e->name[q++];
    line[p] = '\0';
    de_console_log(line);

    int rc = forge_run_file(f);

    char done[48];
    p = 0;
    const char* pre2 = "forge: exit ";
    while (*pre2 && p < 40) done[p++] = *pre2++;
    char num[8]; int n = 0;
    char tmp[8]; int m = 0;
    if (rc == 0) tmp[m++] = '0';
    while (rc > 0) { tmp[m++] = '0' + rc % 10; rc /= 10; }
    while (m > 0) num[n++] = tmp[--m];
    num[n] = '\0';
    q = 0;
    while (num[q] && p < 46) done[p++] = num[q++];
    done[p] = '\0';
    de_console_log(done);

    begin_modal(s, MODAL_MESSAGE, "Forge output in terminal", NULL);
}

static void action_delete(files_state_t* s) {
    if (!selected_is_valid(s)) return;
    kstrcpy(s->target_name, s->entries[s->selected].name);
    begin_modal(s, MODAL_CONFIRM_DELETE, "Delete?", s->target_name);
}

/* --------------------------------------------------------------------------- */
/* Modal key handling                                                          */
/* --------------------------------------------------------------------------- */
static int modal_handle_key(files_state_t* s, de_key_event_t* k) {
    if (s->modal == MODAL_NONE) return 0;

    if (s->modal == MODAL_MESSAGE) {
        /* Any key dismisses. */
        end_modal(s);
        return 1;
    }

    if (s->modal == MODAL_CONFIRM_DELETE) {
        if (k->ascii == 'y' || k->ascii == 'Y') {
            int rc = ramfs_delete(s->target_name, s->cwd,
                                  s->entries[s->selected].is_dir ? 1 : 0);
            (void)rc;
            end_modal(s);
            refresh(s);
            return 1;
        }
        if (k->ascii == 'n' || k->ascii == 'N' ||
            k->ascii == '\n' || k->ascii == 27 /*esc*/) {
            end_modal(s);
            return 1;
        }
        return 1;
    }

    /* Text-input modals */
    if (k->ascii == 27) {           /* escape */
        end_modal(s);
        return 1;
    }
    if (k->ascii == '\n') {
        if (s->input_len > 0) {
            if (s->modal == MODAL_NEW_FILE) {
                ramfs_create(s->input, s->cwd, 0);
            } else if (s->modal == MODAL_NEW_FOLDER) {
                ramfs_create(s->input, s->cwd, 1);
            } else if (s->modal == MODAL_RENAME) {
                ramfs_rename(s->target_name, s->input, s->cwd);
            }
        }
        end_modal(s);
        refresh(s);
        return 1;
    }
    if (k->ascii == '\b') {
        if (s->input_len > 0) {
            s->input_len--;
            s->input[s->input_len] = '\0';
        }
        return 1;
    }
    if (k->ascii >= 32 && k->ascii < 127 && s->input_len < INPUT_MAX - 1) {
        s->input[s->input_len++] = k->ascii;
        s->input[s->input_len] = '\0';
        return 1;
    }
    return 1;
}

/* --------------------------------------------------------------------------- */
/* Keyboard                                                                    */
/* --------------------------------------------------------------------------- */
static void files_key(void* st, window_t* w, de_key_event_t* k) {
    (void)w;
    files_state_t* s = (files_state_t*)st;
    if (!s || !k) return;

    /* Modal swallows all keys. */
    if (modal_handle_key(s, k)) return;

    if (k->extended) {
        switch (k->keycode) {
            case DE_KEY_UP:
                if (s->selected > 0) s->selected--;
                break;
            case DE_KEY_DOWN:
                if (s->selected < s->count - 1) s->selected++;
                break;
            case DE_KEY_HOME:
                s->selected = 0;
                break;
            case DE_KEY_END:
                s->selected = s->count - 1;
                break;
            case DE_KEY_PGUP:
                s->selected -= 10;
                if (s->selected < 0) s->selected = 0;
                break;
            case DE_KEY_PGDN:
                s->selected += 10;
                if (s->selected >= s->count) s->selected = s->count - 1;
                if (s->selected < 0) s->selected = 0;
                break;
            default:
                break;
        }
        return;
    }

    switch (k->ascii) {
        case '\n':
            action_open(s);
            break;
        case 'n': case 'N':
            begin_modal(s, MODAL_NEW_FILE, "New file name", NULL);
            break;
        case 'f': case 'F':
            begin_modal(s, MODAL_NEW_FOLDER, "New folder name", NULL);
            break;
        case 'r': case 'R':
            if (selected_is_valid(s)) {
                kstrcpy(s->target_name, s->entries[s->selected].name);
                begin_modal(s, MODAL_RENAME, "Rename to", s->target_name);
            }
            break;
        case 'd': case 'D':
            action_delete(s);
            break;
        case 'e': case 'E':
            action_edit(s);
            break;
        case 'g': case 'G':
            action_forge(s);
            break;
        case 27:   /* escape */
            s->selected = -1;
            break;
        case 'u': case 'U':   /* "up" shortcut: parent dir */
            if (kstrcmp(s->cwd, "/") != 0) {
                kstrcpy(s->cwd, "/");
                s->selected = -1;
                s->scroll_y = 0;
                refresh(s);
            }
            break;
        default:
            break;
    }
}

/* --------------------------------------------------------------------------- */
/* Mouse                                                                       */
/* --------------------------------------------------------------------------- */
static void files_click(void* st, window_t* w, int mx, int my) {
    files_state_t* s = (files_state_t*)st;
    if (!s) return;

    /* If a modal is up, a click dismisses it (or hits the confirm). */
    if (s->modal != MODAL_NONE) {
        if (s->modal == MODAL_CONFIRM_DELETE) {
            end_modal(s);
        } else {
            end_modal(s);
        }
        return;
    }

    int cx = w->x + 4;
    int cy = w->y + DE_TITLEBAR_H + 4;
    int cw = w->width - 8;
    int ch = w->height - DE_TITLEBAR_H - 8;
    if (mx < cx || mx >= cx + cw || my < cy || my >= cy + ch) return;

    /* Click on the first row ("..") goes up. */
    int row = (my - cy - 4 + s->scroll_y) / FILES_ROW_H;
    if (row < 0 || row >= s->count) return;
    s->selected = row;

    static int last_idx = -1;
    static uint32_t last_t = 0;
    uint32_t now = timer_get_ticks();
    if (last_idx == row && now - last_t < 50) {
        action_open(s);
        last_idx = -1;
    } else {
        last_idx = row;
        last_t = now;
    }
}

static void files_scroll(void* st, window_t* w, int dy) {
    (void)w;
    files_state_t* s = (files_state_t*)st;
    if (!s || s->modal != MODAL_NONE) return;
    s->scroll_y -= dy * 24;
    if (s->scroll_y < 0) s->scroll_y = 0;
}

/* --------------------------------------------------------------------------- */
/* Drawing                                                                     */
/* --------------------------------------------------------------------------- */
static void draw_modal(files_state_t* s, window_t* w) {
    const de_theme_t* t = g_theme;
    int mw = 320;
    int mh = 90;
    int mx = w->x + (w->width  - mw) / 2;
    int my = w->y + (w->height - mh) / 2;

    de_fill(mx + 3, my + 3, mw, mh, t->widget_shadow);
    de_fill_rounded(mx, my, mw, mh, 6, t->widget_bg);
    de_border(mx, my, mw, mh, t->widget_edge);

    de_text(mx + 12, my + 12, s->prompt, t->widget_txt);

    if (s->modal == MODAL_MESSAGE) {
        de_text(mx + 12, my + 40, "(press any key)", t->txt_dim);
        return;
    }

    if (s->modal == MODAL_CONFIRM_DELETE) {
        de_text(mx + 12, my + 40, "Y = yes, N = no", t->txt_dim);
        return;
    }

    /* Text input box */
    int bx = mx + 12;
    int by = my + 36;
    int bw = mw - 24;
    int bh = 22;
    de_fill(bx, by, bw, bh, t->app_bg);
    de_border(bx, by, bw, bh, t->win_border);

    de_text(bx + 4, by + (bh - 8) / 2, s->input, t->app_fg);

    uint32_t blink = (timer_get_ticks() / 25) & 1;
    if (blink) {
        int cx = bx + 4 + s->input_len * 8;
        de_fill(cx, by + (bh - 8) / 2, 2, 8, t->accent);
    }

    de_text(mx + 12, my + 66, "Enter = OK, Esc = Cancel", t->txt_dim);
}

static void files_draw(void* st, window_t* w) {
    files_state_t* s = (files_state_t*)st;
    if (!s) return;

    /* Auto-refresh once per second, but never while a modal is up. */
    static uint32_t last_refresh = 0;
    uint32_t now = timer_get_ticks();
    if (s->modal == MODAL_NONE && now - last_refresh > 100) {
        refresh(s);
        last_refresh = now;
    }

    const de_theme_t* t = g_theme;
    int cx = w->x + 4;
    int cy = w->y + DE_TITLEBAR_H + 4;
    int cw = w->width - 8;
    int ch = w->height - DE_TITLEBAR_H - 8;

    de_fill(cx, cy, cw, ch, t->app_bg);
    de_border(cx, cy, cw, ch, t->win_border);

    /* Path bar at the top */
    de_fill(cx + 1, cy + 1, cw - 2, 14, t->win_title_a);
    char path[64];
    int p = 0;
    const char* pre = "path: ";
    while (*pre && p < 50) path[p++] = *pre++;
    const char* c = s->cwd;
    while (*c && p < 62) path[p++] = *c++;
    path[p] = '\0';
    de_text(cx + 4, cy + 4, path, t->txt_dark);

    /* List area */
    int list_y = cy + 16;
    int list_h = ch - 16;
    int view_h = list_h - 4;

    int total_h = s->count * FILES_ROW_H + 8;
    if (total_h < view_h) total_h = view_h;

    int max_scroll = total_h - view_h;
    if (max_scroll < 0) max_scroll = 0;

    /* Auto-scroll to keep selection visible */
    int sel_top = s->selected * FILES_ROW_H;
    int sel_bot = sel_top + FILES_ROW_H;
    if (sel_top < s->scroll_y) s->scroll_y = sel_top;
    if (sel_bot > s->scroll_y + view_h)
        s->scroll_y = sel_bot - view_h;
    if (s->scroll_y < 0) s->scroll_y = 0;
    if (s->scroll_y > max_scroll) s->scroll_y = max_scroll;

    for (int i = 0; i < s->count; i++) {
        int ry = list_y + 4 + i * FILES_ROW_H - s->scroll_y;
        if (ry + FILES_ROW_H < list_y + 4) continue;
        if (ry > list_y + list_h - 4) break;

        int selected = (i == s->selected);
        if (selected)
            de_fill(cx + 2, ry, cw - 12, FILES_ROW_H - 1, t->panel_sel);

        uint32_t fg = selected ? t->panel_sel_txt : t->app_fg;
        uint32_t dim = selected ? t->panel_sel_txt : t->txt_dim;

        /* Type indicator */
        de_text(cx + 8, ry + 5, s->entries[i].is_dir ? "[D]" : "   ", dim);
        de_text(cx + 32, ry + 5, s->entries[i].name, fg);

        /* Size on the right */
        if (!s->entries[i].is_dir) {
            char num[16]; int n = 0;
            char tmp[16]; int m = 0;
            uint32_t v = s->entries[i].size;
            if (v == 0) tmp[m++] = '0';
            while (v > 0) { tmp[m++] = '0' + v % 10; v /= 10; }
            while (m > 0) num[n++] = tmp[--m];
            num[n] = '\0';
            int tw = n * 8;
            de_text(cx + cw - tw - 18, ry + 5, num, dim);
        }
    }

    if (s->count == 0) {
        de_text(cx + 10, list_y + 10, "(empty)", t->txt_dim);
    }

    /* Scrollbar */
    if (max_scroll > 0) {
        int bar_x = cx + cw - 8;
        de_fill(bar_x, list_y + 2, 6, list_h - 4, t->app_alt_bg);
        int thumb_h = (list_h - 4) * view_h / total_h;
        if (thumb_h < 20) thumb_h = 20;
        int thumb_y = list_y + 2 +
            (list_h - 4 - thumb_h) * s->scroll_y / max_scroll;
        de_fill_rounded(bar_x, thumb_y, 6, thumb_h, 3, t->accent);
    }

    /* Footer: hint text */
    de_fill(cx + 1, cy + ch - 12, cw - 2, 11, t->win_title_a);
    de_text(cx + 4, cy + ch - 11,
            "N=new  F=folder  R=rename  D=delete  E=edit  G=forge  U=up",
            t->txt_dark);

    /* Modal overlay (draw last, on top of everything) */
    if (s->modal != MODAL_NONE) {
        /* Slight dim */
        de_fill(cx + 1, cy + 1, cw - 2, ch - 2, 0x80000000);
        draw_modal(s, w);
    }
}

/* --------------------------------------------------------------------------- */
const app_t app_files = {
    .id = "files",
    .name = "Files",
    .icon_char = "F",
    .default_w = 420,
    .default_h = 320,
    .on_open = files_open,
    .on_close = files_close,
    .on_draw = files_draw,
    .on_click = files_click,
    .on_key = files_key,
    .on_scroll = files_scroll,
};