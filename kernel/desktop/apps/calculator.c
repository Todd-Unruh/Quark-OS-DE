/* =============================================================================
 * Quark-OS kernel/desktop/apps/calculator.c
 * Simple integer calculator with clickable buttons and keyboard input.
 * ============================================================================= */

#include <quark/de_apps.h>

#define CALC_BTN_W 52
#define CALC_BTN_H 32
#define CALC_PAD   4
#define CALC_COLS  4
#define CALC_ROWS  6

typedef struct {
    char display[32];
    int  display_len;
    int  result;
    char op;
    int  cur;
    int  fresh;
} calc_state_t;

static void set_display_str(calc_state_t* s, const char* str) {
    int i = 0;
    while (str[i] && i < 30) { s->display[i] = str[i]; i++; }
    s->display[i] = '\0';
    s->display_len = i;
}

static void update_display(calc_state_t* s) {
    char buf[32];
    int i = 0;
    int v = s->cur;
    int neg = 0;
    if (v < 0) { neg = 1; v = -v; }
    char tmp[16];
    int t = 0;
    if (v == 0) tmp[t++] = '0';
    while (v > 0) { tmp[t++] = '0' + v % 10; v /= 10; }
    if (neg) buf[i++] = '-';
    while (t > 0) buf[i++] = tmp[--t];
    buf[i] = '\0';
    set_display_str(s, buf);
}

static void apply_op(calc_state_t* s) {
    if (s->op == '+') s->result += s->cur;
    else if (s->op == '-') s->result -= s->cur;
    else if (s->op == '*') s->result *= s->cur;
    else if (s->op == '/' && s->cur != 0) s->result /= s->cur;
    else if (s->op == 0) s->result = s->cur;
    s->cur = s->result;
    update_display(s);
}

static void digit(calc_state_t* s, int d) {
    if (s->fresh) { s->cur = 0; s->fresh = 0; }
    s->cur = s->cur * 10 + d;
    if (s->cur > 99999999) s->cur = 99999999;
    update_display(s);
}

static void op(calc_state_t* s, char o) {
    apply_op(s);
    s->op = o;
    s->fresh = 1;
}

static void clear_all(calc_state_t* s) {
    s->result = 0; s->cur = 0; s->op = 0; s->fresh = 1;
    update_display(s);
}

static void* calc_open(void) {
    calc_state_t* s = (calc_state_t*)kmalloc(sizeof(calc_state_t));
    if (!s) return NULL;
    kmemset(s, 0, sizeof(*s));
    s->fresh = 1;
    update_display(s);
    return s;
}

static void calc_close(void* st) { if (st) kfree(st); }

/* Flat, 24-element array. Indexed as button_labels[r * CALC_COLS + c]. */
static const char* button_labels[CALC_ROWS * CALC_COLS] = {
    "C", "+/-", "%", "/",
    "7", "8", "9", "*",
    "4", "5", "6", "-",
    "1", "2", "3", "+",
    "0", ".", "=", "",
    "", "", "", "",
};

static void calc_btn_rect(window_t* w, int row, int col, int* bx, int* by) {
    int grid_w = CALC_COLS * (CALC_BTN_W + CALC_PAD) + CALC_PAD;
    int grid_h = CALC_ROWS * (CALC_BTN_H + CALC_PAD) + CALC_PAD;
    int x0 = w->x + (w->width - grid_w) / 2;
    int y0 = w->y + w->height - grid_h - CALC_PAD - 4;
    *bx = x0 + CALC_PAD + col * (CALC_BTN_W + CALC_PAD);
    *by = y0 + CALC_PAD + row * (CALC_BTN_H + CALC_PAD);
}

static void calc_draw(void* st, window_t* w) {
    calc_state_t* s = (calc_state_t*)st;
    if (!s) return;
    const de_theme_t* t = g_theme;

    /* Display */
    int dx = w->x + 8;
    int dy = w->y + DE_TITLEBAR_H + 8;
    int dw = w->width - 16;
    int dh = 44;
    de_fill_rounded(dx, dy, dw, dh, 4, t->app_alt_bg);
    de_border(dx, dy, dw, dh, t->win_border);

    int tw = de_text_width(s->display);
    de_text(dx + dw - tw - 8, dy + (dh - 8) / 2, s->display, t->app_fg);

    /* Buttons */
    for (int r = 0; r < CALC_ROWS; r++) {
        for (int c = 0; c < CALC_COLS; c++) {
            const char* lbl = button_labels[r * CALC_COLS + c];
            if (!lbl || !lbl[0]) continue;

            if (r == 4 && c == 0) {
                /* "0" button spans two columns */
                int bx, by;
                calc_btn_rect(w, r, c, &bx, &by);
                int bw = CALC_BTN_W * 2 + CALC_PAD;
                de_fill_rounded(bx, by, bw, CALC_BTN_H, 4, t->panel_btn);
                de_border(bx, by, bw, CALC_BTN_H, t->panel_edge);
                de_text_centered(bx + bw / 2 - 4, by + (CALC_BTN_H - 8) / 2,
                                 lbl, t->panel_txt);
                c++;   /* skip next column */
                continue;
            }

            int bx, by;
            calc_btn_rect(w, r, c, &bx, &by);

            uint32_t bg = t->panel_btn;
            if (r == 0 && c == 0) bg = t->btn_close;
            else if (lbl[0] == '+' || lbl[0] == '-' ||
                     lbl[0] == '*' || lbl[0] == '/' || lbl[0] == '=')
                bg = t->accent;

            de_fill_rounded(bx, by, CALC_BTN_W, CALC_BTN_H, 4, bg);
            de_border(bx, by, CALC_BTN_W, CALC_BTN_H, t->panel_edge);
            de_text_centered(bx + CALC_BTN_W / 2 - 4,
                             by + (CALC_BTN_H - 8) / 2,
                             lbl,
                             (bg == t->accent) ? 0x00FFFFFF : t->panel_txt);
        }
    }
}

static void calc_click(void* st, window_t* w, int mx, int my) {
    calc_state_t* s = (calc_state_t*)st;
    if (!s) return;

    for (int r = 0; r < CALC_ROWS; r++) {
        for (int c = 0; c < CALC_COLS; c++) {
            int bx, by;
            calc_btn_rect(w, r, c, &bx, &by);
            int bw = CALC_BTN_W;
            if (r == 4 && c == 0) bw = CALC_BTN_W * 2 + CALC_PAD;
            if (mx >= bx && mx < bx + bw &&
                my >= by && my < by + CALC_BTN_H) {
                const char* lbl = button_labels[r * CALC_COLS + c];
                if (!lbl || !lbl[0]) return;
                if (lbl[0] >= '0' && lbl[0] <= '9') digit(s, lbl[0] - '0');
                else if (lbl[0] == '+' || lbl[0] == '-' ||
                         lbl[0] == '*' || lbl[0] == '/') op(s, lbl[0]);
                else if (lbl[0] == '=') { apply_op(s); s->op = 0; s->fresh = 1; }
                else if (lbl[0] == 'C') clear_all(s);
                else if (lbl[0] == '+' && lbl[1] == '/') {
                    s->cur = -s->cur; update_display(s);
                }
                else if (lbl[0] == '%') { s->cur = s->cur / 100; update_display(s); }
                return;
            }
        }
    }
}

static void calc_key(void* st, window_t* w, de_key_event_t* k) {
    (void)w;
    calc_state_t* s = (calc_state_t*)st;
    if (!s || !k) return;
    if (k->ascii >= '0' && k->ascii <= '9') digit(s, k->ascii - '0');
    else if (k->ascii == '+') op(s, '+');
    else if (k->ascii == '-') op(s, '-');
    else if (k->ascii == '*') op(s, '*');
    else if (k->ascii == '/') op(s, '/');
    else if (k->ascii == '=' || k->ascii == '\n') {
        apply_op(s); s->op = 0; s->fresh = 1;
    }
    else if (k->ascii == 'c' || k->ascii == 'C') clear_all(s);
}

const app_t app_calculator = {
    .id = "calc",
    .name = "Calculator",
    .icon_char = "C",
    .default_w = 260,
    .default_h = 340,
    .on_open = calc_open,
    .on_close = calc_close,
    .on_draw = calc_draw,
    .on_click = calc_click,
    .on_key = calc_key,
    .on_scroll = NULL,
};