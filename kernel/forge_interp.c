/*
 * kernel/forge_interp.c — Forge_High interpreter for Quark-OS-3 shell
 * Run Forge programs from ramfs:  forge script.fg
 */

#include <quark/kernel.h>
#include <quark/forge.h>
#include <quark/de.h>
#include <quark/de_apps.h>
#include <quark/de_font.h>

#define FG_MAX_VARS    64
#define FG_MAX_LINE    256
#define FG_MAX_VAL     512
#define FG_MAX_FUNCS   16
#define FG_MAX_BODY    64
#define FG_MAX_PARAMS  8
#define FG_MAX_LOOP    1024

typedef struct {
    char name[32];
    char val[FG_MAX_VAL];
    int  is_const;
} fg_var_t;

typedef struct {
    char name[32];
    char params[FG_MAX_PARAMS][32];
    int  param_count;
    char body[FG_MAX_BODY][FG_MAX_LINE];
    int  body_len;
} fg_func_t;

typedef struct {
    fg_var_t  vars[FG_MAX_VARS];
    int       var_count;
    fg_func_t funcs[FG_MAX_FUNCS];
    int       func_count;
    char      return_val[FG_MAX_VAL];
    int       return_active;
    int       break_active;
    int       call_depth;
} fg_engine_t;

typedef struct {
    int x, y, w, h;
    char label[64];
    char callback[32];
    char variable[32];
    int kind;
} fg_ui_widget_t;

typedef struct {
    fg_engine_t engine;
    app_t app;
    char title[64];
    int width, height;
    int drawing;
    int draw_x, draw_y;
    int widget_count;
    fg_ui_widget_t widgets[32];
} fg_ui_state_t;

static fg_engine_t g_fg_default;
static fg_engine_t *g_fg_current = &g_fg_default;
#define g_fg (*g_fg_current)
static fg_ui_state_t *g_ui_current;
static fg_ui_state_t *g_ui_pending;
static forge_output_fn g_output;
static void *g_output_context;

static void fg_ui_draw(void *st, window_t *w);
static void fg_ui_click(void *st, window_t *w, int mx, int my);
static void fg_ui_close(void *st);
static void *fg_ui_open(void);

void forge_set_output(forge_output_fn output, void *context) {
    g_output = output;
    g_output_context = context;
}

static void fg_write(const char *text) {
    if (g_output) {
        while (*text) g_output(g_output_context, *text++);
    } else {
        kprintf("%s", text);
    }
}

static fg_var_t *fg_var_find(const char *name) {
    for (int i = 0; i < g_fg.var_count; i++)
        if (kstrcmp(g_fg.vars[i].name, name) == 0)
            return &g_fg.vars[i];
    return NULL;
}

static void fg_var_set(const char *name, const char *val) {
    fg_var_t *v = fg_var_find(name);
    if (v && v->is_const) return;
    if (!v) {
        if (g_fg.var_count >= FG_MAX_VARS) return;
        v = &g_fg.vars[g_fg.var_count++];
        kstrcpy(v->name, name);
    }
    kstrncpy(v->val, val, FG_MAX_VAL - 1);
    v->val[FG_MAX_VAL - 1] = '\0';
}

static const char *fg_var_get(const char *name) {
    fg_var_t *v = fg_var_find(name);
    return v ? v->val : "";
}

static void fg_trim(char *s) {
    char *p = s;
    while (*p == ' ' || *p == '\t') p++;
    if (p != s) {
        size_t n = kstrlen(p);
        kmemcpy(s, p, n + 1);
    }
    int n = (int)kstrlen(s);
    while (n > 0 && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\r' || s[n-1] == '\n'))
        s[--n] = '\0';
}

static void fg_interpolate(const char *src, char *dst, size_t sz) {
    size_t di = 0;
    const char *p = src;
    while (*p && di < sz - 1) {
        if (*p == '@') {
            char name[32];
            int ni = 0;
            p++;
            while (*p && ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
                   (*p >= '0' && *p <= '9') || *p == '_') && ni < 31)
                name[ni++] = *p++;
            name[ni] = '\0';
            const char *val = fg_var_get(name);
            while (*val && di < sz - 1) dst[di++] = *val++;
            continue;
        }
        if (p[0] == '(' && p[1] == '@') {
            const char *close = kstrchr(p, ')');
            if (close) {
                char name[32];
                int ni = 0;
                const char *q = p + 2;
                while (q < close && ni < 31) name[ni++] = *q++;
                name[ni] = '\0';
                const char *val = fg_var_get(name);
                while (*val && di < sz - 1) dst[di++] = *val++;
                p = close + 1;
                continue;
            }
        }
        dst[di++] = *p++;
    }
    dst[di] = '\0';
}

static int fg_is_num(const char *s) {
    if (!s || !*s) return 0;
    if (*s == '-') s++;
    if (!*s) return 0;
    while (*s) {
        if (*s < '0' || *s > '9') return 0;
        s++;
    }
    return 1;
}

static int fg_eval_math(const char *expr, char *out, size_t outsz) {
    char buf[FG_MAX_VAL];
    kstrncpy(buf, expr, sizeof(buf) - 1);
    fg_trim(buf);
    int a = 0, b = 0;
    char op = 0;
    char *p = buf;
        while (*p && *p != '+' && *p != '-' && *p != '*' && *p != '/' &&
            *p != '%') p++;
    if (*p) {
        op = *p;
        *p = '\0';
        fg_trim(buf);
        char rhs[FG_MAX_VAL];
        kstrcpy(rhs, p + 1);
        fg_trim(rhs);
        const char *aval = fg_is_num(buf) ? buf : fg_var_get(buf);
        const char *bval = fg_is_num(rhs) ? rhs : fg_var_get(rhs);
        a = aval[0] ? (int)kstrtol_simple(aval) : 0;
        b = bval[0] ? (int)kstrtol_simple(bval) : 0;
        int res = 0;
        switch (op) {
            case '+': res = a + b; break;
            case '-': res = a - b; break;
            case '*': res = a * b; break;
            case '/': res = b ? a / b : 0; break;
            case '%': res = b ? a % b : 0; break;
        }
        kitoa(res, out, 10);
        return 1;
    }
    if (fg_is_num(buf)) {
        kstrncpy(out, buf, outsz - 1);
        return 1;
    }
    const char *v = fg_var_get(buf);
    kstrncpy(out, v, outsz - 1);
    return 1;
}

static void fg_eval_expr(const char *raw, char *out, size_t outsz) {
    size_t len = kstrlen(raw);
    if (len >= 2 && raw[0] == '"' && raw[len-1] == '"') {
        char inner[FG_MAX_VAL];
        kstrncpy(inner, raw + 1, len - 2);
        inner[len - 2] = '\0';
        fg_interpolate(inner, out, outsz);
        return;
    }
    char tmp[FG_MAX_VAL];
    fg_interpolate(raw, tmp, sizeof(tmp));
    fg_eval_math(tmp, out, outsz);
}

static int fg_eval_cond(const char *cond) {
    char cbuf[FG_MAX_LINE];
    kstrncpy(cbuf, cond, sizeof(cbuf) - 1);
    fg_trim(cbuf);
    size_t cl = kstrlen(cbuf);
    if (cl > 0 && cbuf[cl-1] == ':') cbuf[--cl] = '\0';
    fg_trim(cbuf);

    const char *ops[] = { "==", "!=", "<=", ">=", "<", ">", NULL };
    for (int oi = 0; ops[oi]; oi++) {
        const char *op = ops[oi];
        const char *found = kstrstr(cbuf, op);
        if (!found) continue;
        char left[FG_MAX_VAL], right[FG_MAX_VAL];
        size_t llen = (size_t)(found - cbuf);
        kmemcpy(left, cbuf, llen);
        left[llen] = '\0';
        kstrcpy(right, found + kstrlen(op));
        fg_trim(left);
        fg_trim(right);
        char lval[FG_MAX_VAL], rval[FG_MAX_VAL];
        fg_eval_expr(left, lval, sizeof(lval));
        fg_eval_expr(right, rval, sizeof(rval));
        int cmp = kstrcmp(lval, rval);
        if (kstrcmp(op, "==") == 0) return cmp == 0;
        if (kstrcmp(op, "!=") == 0) return cmp != 0;
        if (kstrcmp(op, "<") == 0)  return kstrtol_simple(lval) < kstrtol_simple(rval);
        if (kstrcmp(op, ">") == 0)  return kstrtol_simple(lval) > kstrtol_simple(rval);
        if (kstrcmp(op, "<=") == 0) return kstrtol_simple(lval) <= kstrtol_simple(rval);
        if (kstrcmp(op, ">=") == 0) return kstrtol_simple(lval) >= kstrtol_simple(rval);
    }
    char val[FG_MAX_VAL];
    fg_eval_expr(cbuf, val, sizeof(val));
    return val[0] != '\0' && kstrcmp(val, "0") != 0 && kstrcmp(val, "false") != 0;
}

static fg_func_t *fg_func_find(const char *name) {
    for (int i = 0; i < g_fg.func_count; i++)
        if (kstrcmp(g_fg.funcs[i].name, name) == 0)
            return &g_fg.funcs[i];
    return NULL;
}

static int fg_run_lines(char lines[][FG_MAX_LINE], int n);
static int fg_exec_line(const char *line);

static int fg_ui_truthy(const char *value) {
    return value && value[0] && kstrcmp(value, "0") != 0 &&
           kstrcmp(value, "false") != 0;
}

static int fg_next_token(const char **cursor, char *out, size_t outsz) {
    const char *p = *cursor;
    while (*p == ' ' || *p == '\t') p++;
    if (!*p) {
        *cursor = p;
        return 0;
    }
    size_t len = 0;
    if (*p == '"') {
        if (len + 1 < outsz) out[len++] = *p;
        p++;
        while (*p && *p != '"') {
            if (len + 1 < outsz) out[len++] = *p;
            p++;
        }
        if (*p == '"') {
            if (len + 1 < outsz) out[len++] = *p;
            p++;
        }
    } else {
        while (*p && *p != ' ' && *p != '\t') {
            if (len + 1 < outsz) out[len++] = *p;
            p++;
        }
    }
    out[len] = '\0';
    *cursor = p;
    return 1;
}

static void fg_ui_eval_token(const char *raw, char *out, size_t outsz) {
    fg_eval_expr(raw, out, outsz);
}

static int fg_ui_int(const char *raw) {
    char value[32];
    fg_ui_eval_token(raw, value, sizeof(value));
    return (int)kstrtol_simple(value);
}

static uint32_t fg_ui_color(const char *raw) {
    char value[32];
    fg_ui_eval_token(raw, value, sizeof(value));
    const char *p = value;
    int base = 10;
    if (*p == '#') {
        p++;
        base = 16;
    } else if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
        base = 16;
    }
    uint32_t color = 0;
    while (*p) {
        int digit;
        if (*p >= '0' && *p <= '9') digit = *p - '0';
        else if (base == 16 && *p >= 'a' && *p <= 'f') digit = *p - 'a' + 10;
        else if (base == 16 && *p >= 'A' && *p <= 'F') digit = *p - 'A' + 10;
        else return 0;
        if (digit >= base) return 0;
        color = color * (uint32_t)base + (uint32_t)digit;
        p++;
    }
    return color;
}

static int fg_ui_exec(const char *line) {
    char command[32], args[6][FG_MAX_VAL];
    const char *p = line;
    if (!fg_next_token(&p, command, sizeof(command))) return 0;
    int argc = 0;
    while (argc < 6 && fg_next_token(&p, args[argc], sizeof(args[argc])))
        argc++;

    if (kstrcmp(command, "window") == 0) {
        if (!g_ui_current || argc < 3) return 1;
        char title[64];
        fg_ui_eval_token(args[0], title, sizeof(title));
        kstrncpy(g_ui_current->title, title, sizeof(g_ui_current->title) - 1);
        g_ui_current->title[sizeof(g_ui_current->title) - 1] = '\0';
        g_ui_current->width = fg_ui_int(args[1]);
        g_ui_current->height = fg_ui_int(args[2]);
        if (g_ui_current->width < 240) g_ui_current->width = 240;
        if (g_ui_current->height < 160) g_ui_current->height = 160;
        return 1;
    }

    if (!g_ui_current || !g_ui_current->drawing) return 0;
    fg_ui_state_t *ui = g_ui_current;

    int x, y, width, height;
    uint32_t color;
    if (kstrcmp(command, "fill") == 0 && argc >= 5) {
        x = fg_ui_int(args[0]); y = fg_ui_int(args[1]);
        width = fg_ui_int(args[2]); height = fg_ui_int(args[3]);
        color = fg_ui_color(args[4]);
        de_fill(ui->draw_x + x, ui->draw_y + y, width, height, color);
        return 1;
    }
    if (kstrcmp(command, "rect") == 0 && argc >= 5) {
        x = fg_ui_int(args[0]); y = fg_ui_int(args[1]);
        width = fg_ui_int(args[2]); height = fg_ui_int(args[3]);
        color = fg_ui_color(args[4]);
        de_border(ui->draw_x + x, ui->draw_y + y, width, height, color);
        return 1;
    }
    if (kstrcmp(command, "panel") == 0 && argc >= 5) {
        x = fg_ui_int(args[0]); y = fg_ui_int(args[1]);
        width = fg_ui_int(args[2]); height = fg_ui_int(args[3]);
        color = fg_ui_color(args[4]);
        de_fill_rounded(ui->draw_x + x, ui->draw_y + y, width, height, 9, color);
        de_border_rounded(ui->draw_x + x, ui->draw_y + y, width, height, 9,
                          g_theme->widget_edge);
        return 1;
    }
    if (kstrcmp(command, "line") == 0 && argc >= 5) {
        int x2 = fg_ui_int(args[2]), y2 = fg_ui_int(args[3]);
        color = fg_ui_color(args[4]);
        de_line(ui->draw_x + fg_ui_int(args[0]),
                ui->draw_y + fg_ui_int(args[1]),
                ui->draw_x + x2, ui->draw_y + y2, color);
        return 1;
    }
    if (kstrcmp(command, "circle") == 0 && argc >= 4) {
        x = fg_ui_int(args[0]); y = fg_ui_int(args[1]);
        int radius = fg_ui_int(args[2]);
        color = fg_ui_color(args[3]);
        de_fill_circle(ui->draw_x + x, ui->draw_y + y, radius, color);
        return 1;
    }
    if ((kstrcmp(command, "text") == 0 || kstrcmp(command, "label") == 0) &&
        argc >= 3) {
        char text[FG_MAX_VAL];
        fg_ui_eval_token(args[2], text, sizeof(text));
        color = argc >= 4 ? fg_ui_color(args[3]) : g_theme->widget_txt;
        de_text(ui->draw_x + fg_ui_int(args[0]),
                ui->draw_y + fg_ui_int(args[1]), text, color);
        return 1;
    }
    if (kstrcmp(command, "button") == 0 && argc >= 6 &&
        ui->widget_count < 32) {
        fg_ui_widget_t *widget = &ui->widgets[ui->widget_count++];
        widget->x = fg_ui_int(args[0]); widget->y = fg_ui_int(args[1]);
        widget->w = fg_ui_int(args[2]); widget->h = fg_ui_int(args[3]);
        fg_ui_eval_token(args[4], widget->label, sizeof(widget->label));
        kstrncpy(widget->callback, args[5], sizeof(widget->callback) - 1);
        widget->callback[sizeof(widget->callback) - 1] = '\0';
        widget->kind = 0;
        de_fill_rounded(ui->draw_x + widget->x, ui->draw_y + widget->y,
                        widget->w, widget->h, 5, g_theme->accent);
        de_border(ui->draw_x + widget->x, ui->draw_y + widget->y,
                  widget->w, widget->h, g_theme->panel_edge);
        de_text_centered(ui->draw_x + widget->x + widget->w / 2,
                         ui->draw_y + widget->y +
                         (widget->h - de_font_body.ascent) / 2,
                         widget->label, 0x00FFFFFF);
        return 1;
    }
    if (kstrcmp(command, "checkbox") == 0 && argc >= 5 &&
        ui->widget_count < 32) {
        fg_ui_widget_t *widget = &ui->widgets[ui->widget_count++];
        widget->x = fg_ui_int(args[0]); widget->y = fg_ui_int(args[1]);
        fg_ui_eval_token(args[2], widget->label, sizeof(widget->label));
        widget->w = 21 + de_text_width(widget->label);
        widget->h = 18;
        kstrncpy(widget->variable, args[3], sizeof(widget->variable) - 1);
        widget->variable[sizeof(widget->variable) - 1] = '\0';
        kstrncpy(widget->callback, args[4], sizeof(widget->callback) - 1);
        widget->callback[sizeof(widget->callback) - 1] = '\0';
        widget->kind = 1;
        de_fill_rounded(ui->draw_x + widget->x, ui->draw_y + widget->y,
                        14, 14, 3, g_theme->widget_bg);
        de_border(ui->draw_x + widget->x, ui->draw_y + widget->y,
                  14, 14, g_theme->widget_edge);
        if (fg_ui_truthy(fg_var_get(widget->variable))) {
            int box_x = ui->draw_x + widget->x;
            int box_y = ui->draw_y + widget->y;
            de_line(box_x + 4, box_y + 4, box_x + 9, box_y + 9,
                    g_theme->accent);
            de_line(box_x + 9, box_y + 4, box_x + 4, box_y + 9,
                    g_theme->accent);
        }
        de_text(ui->draw_x + widget->x + 21, ui->draw_y + widget->y + 2,
                widget->label, g_theme->widget_txt);
        return 1;
    }
    return 0;
}

static int fg_call_func(const char *fname, const char *argstr) {
    fg_func_t *fn = fg_func_find(fname);
    if (!fn) {
        fg_write("forge: undefined function '");
        fg_write(fname);
        fg_write("'\n");
        return 0;
    }
    if (g_fg.call_depth >= 64) {
        fg_write("forge: maximum function call depth exceeded\n");
        return 0;
    }
    char args[FG_MAX_PARAMS][FG_MAX_VAL];
    int argc = 0;
    char acopy[FG_MAX_VAL];
    kstrncpy(acopy, argstr ? argstr : "", sizeof(acopy) - 1);
    char *tok = acopy;
    char *start = tok;
    while (*tok && argc < FG_MAX_PARAMS) {
        if (*tok == ',') {
            *tok = '\0';
            fg_trim(start);
            fg_eval_expr(start, args[argc], FG_MAX_VAL);
            argc++;
            start = tok + 1;
        }
        tok++;
    }
    if (start && *start) {
        fg_trim(start);
        fg_eval_expr(start, args[argc], FG_MAX_VAL);
        argc++;
    }
    char saved[FG_MAX_PARAMS][FG_MAX_VAL];
    for (int i = 0; i < fn->param_count; i++) {
        kstrcpy(saved[i], fg_var_get(fn->params[i]));
        fg_var_set(fn->params[i], i < argc ? args[i] : "");
    }
    g_fg.return_active = 0;
    g_fg.return_val[0] = '\0';
    g_fg.call_depth++;
    fg_run_lines((char (*)[FG_MAX_LINE])fn->body, fn->body_len);
    g_fg.call_depth--;
    for (int i = 0; i < fn->param_count; i++)
        fg_var_set(fn->params[i], saved[i]);
    return 1;
}

static int fg_run_lines(char lines[][FG_MAX_LINE], int n) {
    int i = 0;
    while (i < n) {
        if (g_fg.return_active || g_fg.break_active) return 0;
        char buf[FG_MAX_LINE];
        kstrcpy(buf, lines[i]);
        fg_trim(buf);
        if (!buf[0] || buf[0] == '#') { i++; continue; }

        if (kstrncmp(buf, "if ", 3) == 0) {
            int condition = fg_eval_cond(buf + 3);
            char body[FG_MAX_BODY][FG_MAX_LINE];
            int blen = 0;
            i++;
            while (i < n && blen < FG_MAX_BODY) {
                char raw[FG_MAX_LINE];
                kstrcpy(raw, lines[i]);
                if (raw[0] != ' ' && raw[0] != '\t') break;
                const char *p = raw;
                while (*p == ' ' || *p == '\t') p++;
                kstrncpy(body[blen++], p, FG_MAX_LINE - 1);
                i++;
            }
            char else_body[FG_MAX_BODY][FG_MAX_LINE];
            int else_len = 0;
            if (i < n) {
                char else_line[FG_MAX_LINE];
                kstrcpy(else_line, lines[i]);
                fg_trim(else_line);
                if (kstrcmp(else_line, "else:") == 0) {
                    i++;
                    while (i < n && else_len < FG_MAX_BODY) {
                        char raw[FG_MAX_LINE];
                        kstrcpy(raw, lines[i]);
                        if (raw[0] != ' ' && raw[0] != '\t') break;
                        const char *p = raw;
                        while (*p == ' ' || *p == '\t') p++;
                        kstrncpy(else_body[else_len++], p, FG_MAX_LINE - 1);
                        i++;
                    }
                }
            }
            if (condition) fg_run_lines(body, blen);
            else if (else_len > 0) fg_run_lines(else_body, else_len);
            continue;
        }

        if (kstrncmp(buf, "while ", 6) == 0) {
            char condition[FG_MAX_LINE];
            kstrcpy(condition, buf + 6);
            char body[FG_MAX_BODY][FG_MAX_LINE];
            int body_len = 0;
            i++;
            while (i < n && body_len < FG_MAX_BODY) {
                char raw[FG_MAX_LINE];
                kstrcpy(raw, lines[i]);
                if (raw[0] != ' ' && raw[0] != '\t') break;
                const char *p = raw;
                while (*p == ' ' || *p == '\t') p++;
                kstrncpy(body[body_len++], p, FG_MAX_LINE - 1);
                i++;
            }
            for (int iteration = 0; iteration < FG_MAX_LOOP &&
                                    fg_eval_cond(condition); iteration++) {
                fg_run_lines(body, body_len);
                if (g_fg.return_active) break;
                if (g_fg.break_active) {
                    g_fg.break_active = 0;
                    break;
                }
            }
            continue;
        }

        if (kstrncmp(buf, "for ", 4) == 0) {
            char decl[FG_MAX_LINE];
            kstrcpy(decl, buf + 4);
            size_t decl_len = kstrlen(decl);
            if (decl_len && decl[decl_len - 1] == ':')
                decl[--decl_len] = '\0';
            char *from = kstrstr(decl, " from ");
            char *to = from ? kstrstr(from + 6, " to ") : NULL;
            if (!from || !to) { i++; continue; }

            *from = '\0';
            *to = '\0';
            char varname[32], start_expr[FG_MAX_LINE], end_expr[FG_MAX_LINE];
            kstrcpy(varname, decl);
            kstrcpy(start_expr, from + 6);
            kstrcpy(end_expr, to + 4);
            fg_trim(varname);
            fg_trim(start_expr);
            fg_trim(end_expr);

            char start_value[FG_MAX_VAL], end_value[FG_MAX_VAL];
            fg_eval_expr(start_expr, start_value, sizeof(start_value));
            fg_eval_expr(end_expr, end_value, sizeof(end_value));
            int start_num = (int)kstrtol_simple(start_value);
            int end_num = (int)kstrtol_simple(end_value);

            char body[FG_MAX_BODY][FG_MAX_LINE];
            int body_len = 0;
            i++;
            while (i < n && body_len < FG_MAX_BODY) {
                char raw[FG_MAX_LINE];
                kstrcpy(raw, lines[i]);
                if (raw[0] != ' ' && raw[0] != '\t') break;
                const char *p = raw;
                while (*p == ' ' || *p == '\t') p++;
                kstrncpy(body[body_len++], p, FG_MAX_LINE - 1);
                i++;
            }

            int step = start_num <= end_num ? 1 : -1;
            int value = start_num;
            for (int iteration = 0; iteration < FG_MAX_LOOP; iteration++) {
                if ((step > 0 && value > end_num) ||
                    (step < 0 && value < end_num)) break;
                char value_text[16];
                kitoa(value, value_text, 10);
                fg_var_set(varname, value_text);
                fg_run_lines(body, body_len);
                if (g_fg.return_active) break;
                if (g_fg.break_active) {
                    g_fg.break_active = 0;
                    break;
                }
                if (value == end_num) break;
                value += step;
            }
            continue;
        }

        if (kstrncmp(buf, "repeat ", 7) == 0) {
            char nstr[32];
            kstrcpy(nstr, buf + 7);
            fg_trim(nstr);
            size_t nl = kstrlen(nstr);
            if (nl > 0 && nstr[nl-1] == ':') nstr[--nl] = '\0';
            char nval[32];
            fg_eval_expr(nstr, nval, sizeof(nval));
            int count = (int)kstrtol_simple(nval);
            if (count < 0) count = 0;
            if (count > FG_MAX_LOOP) count = FG_MAX_LOOP;
            char body[FG_MAX_BODY][FG_MAX_LINE];
            int blen = 0;
            i++;
            while (i < n && blen < FG_MAX_BODY) {
                char raw[FG_MAX_LINE];
                kstrcpy(raw, lines[i]);
                if (raw[0] != ' ' && raw[0] != '\t') break;
                const char *p = raw;
                while (*p == ' ' || *p == '\t') p++;
                kstrncpy(body[blen++], p, FG_MAX_LINE - 1);
                i++;
            }
            for (int r = 0; r < count; r++)
            {
                fg_run_lines(body, blen);
                if (g_fg.return_active) break;
                if (g_fg.break_active) {
                    g_fg.break_active = 0;
                    break;
                }
            }
            continue;
        }

        if (kstrncmp(buf, "func ", 5) == 0) {
            char fdecl[FG_MAX_LINE];
            kstrcpy(fdecl, buf + 5);
            fg_trim(fdecl);
            size_t fd = kstrlen(fdecl);
            if (fd > 0 && fdecl[fd-1] == ':') fdecl[--fd] = '\0';
            char fname[32], plist[128];
            plist[0] = '\0';
            char *lp = kstrchr(fdecl, '(');
            if (lp) {
                *lp = '\0';
                kstrcpy(fname, fdecl);
                fg_trim(fname);
                char *rp = kstrrchr(lp + 1, ')');
                if (rp) *rp = '\0';
                kstrcpy(plist, lp + 1);
            } else {
                kstrcpy(fname, fdecl);
            }
            fg_func_t *fn = fg_func_find(fname);
            if (!fn && g_fg.func_count < FG_MAX_FUNCS)
                fn = &g_fg.funcs[g_fg.func_count++];
            if (fn) {
                kstrcpy(fn->name, fname);
                fn->param_count = 0;
                fn->body_len = 0;
                char *tok = plist;
                char *ps = tok;
                while (*tok && fn->param_count < FG_MAX_PARAMS) {
                    if (*tok == ',') {
                        *tok = '\0';
                        fg_trim(ps);
                        kstrcpy(fn->params[fn->param_count++], ps);
                        ps = tok + 1;
                    }
                    tok++;
                }
                if (ps && *ps) {
                    fg_trim(ps);
                    kstrcpy(fn->params[fn->param_count++], ps);
                }
                i++;
                while (i < n && fn->body_len < FG_MAX_BODY) {
                    char raw[FG_MAX_LINE];
                    kstrcpy(raw, lines[i]);
                    if (raw[0] != ' ' && raw[0] != '\t') break;
                    const char *p = raw;
                    while (*p == ' ' || *p == '\t') p++;
                    kstrncpy(fn->body[fn->body_len++], p, FG_MAX_LINE - 1);
                    i++;
                }
            } else i++;
            continue;
        }

        fg_exec_line(buf);
        i++;
    }
    return 0;
}

static int fg_exec_line(const char *line) {
    char buf[FG_MAX_LINE];
    kstrcpy(buf, line);
    fg_trim(buf);

    if (kstrncmp(buf, "print ", 6) == 0) {
        char val[FG_MAX_VAL];
        fg_eval_expr(buf + 6, val, sizeof(val));
        fg_write(val);
        fg_write("\n");
        return 0;
    }

    if (kstrcmp(buf, "print") == 0) {
        fg_write("\n");
        return 0;
    }

    if (kstrcmp(buf, "break") == 0) {
        g_fg.break_active = 1;
        return 0;
    }

    if (kstrncmp(buf, "const ", 6) == 0 || kstrncmp(buf, "set ", 4) == 0) {
        int is_const = (buf[0] == 'c');
        const char *body = is_const ? buf + 6 : buf + 4;
        char *eq = kstrchr(body, '=');
        if (!eq) return 0;
        *eq = '\0';
        char name[32];
        kstrcpy(name, body);
        fg_trim(name);
        char val[FG_MAX_VAL];
        fg_eval_expr(eq + 1, val, sizeof(val));
        fg_var_set(name, val);
        if (is_const) {
            fg_var_t *v = fg_var_find(name);
            if (v) v->is_const = 1;
        }
        return 0;
    }

    if (kstrncmp(buf, "input ", 6) == 0) {
        char prompt[FG_MAX_VAL], varname[32];
        const char *p = buf + 6;
        while (*p == ' ') p++;
        if (*p == '"') {
            p++;
            int pi = 0;
            char pr[FG_MAX_VAL];
            while (*p && *p != '"' && pi < FG_MAX_VAL - 1) pr[pi++] = *p++;
            pr[pi] = '\0';
            if (*p == '"') p++;
            while (*p == ' ') p++;
            kstrcpy(varname, p);
            fg_trim(varname);
            fg_interpolate(pr, prompt, sizeof(prompt));
            fg_write(prompt);
            char inbuf[FG_MAX_VAL];
            int ii = 0;
            char c;
            while (ii < FG_MAX_VAL - 1 && (c = keyboard_getchar()) != '\n') {
                terminal_putchar(c);
                inbuf[ii++] = c;
            }
            inbuf[ii] = '\0';
            terminal_putchar('\n');
            fg_var_set(varname, inbuf);
        }
        return 0;
    }

    if (kstrncmp(buf, "call ", 5) == 0) {
        char fname[32], argstr[FG_MAX_VAL];
        argstr[0] = '\0';
        const char *lp = kstrchr(buf + 5, '(');
        if (lp) {
            size_t fnl = (size_t)(lp - (buf + 5));
            kmemcpy(fname, buf + 5, fnl);
            fname[fnl] = '\0';
            fg_trim(fname);
            const char *rp = kstrrchr(lp, ')');
            if (rp) {
                size_t al = (size_t)(rp - lp - 1);
                if (al < sizeof(argstr)) {
                    kmemcpy(argstr, lp + 1, al);
                    argstr[al] = '\0';
                }
            }
        } else {
            kstrcpy(fname, buf + 5);
            fg_trim(fname);
        }
        fg_call_func(fname, argstr);
        return 0;
    }

    if (kstrcmp(buf, "return") == 0 || kstrncmp(buf, "return ", 7) == 0) {
        g_fg.return_val[0] = '\0';
        const char *rv = buf + 6;
        while (*rv == ' ') rv++;
        if (*rv) fg_eval_expr(rv, g_fg.return_val, sizeof(g_fg.return_val));
        g_fg.return_active = 1;
        return 0;
    }

    if (fg_ui_exec(buf)) return 0;
    return 0;
}

static int fg_execute_file(ram_file_t *file) {
    char lines[128][FG_MAX_LINE];
    int line_count = 0;
    char line[FG_MAX_LINE];
    int li = 0;

    for (uint32_t i = 0; i <= file->size && line_count < 128; i++) {
        char c = (i < file->size) ? (char)file->data[i] : '\n';
        if (c == '\n' || li >= FG_MAX_LINE - 1) {
            line[li] = '\0';
            li = 0;
            if (line[0]) {
                kstrncpy(lines[line_count], line, FG_MAX_LINE - 1);
                line_count++;
            }
        } else {
            line[li++] = c;
        }
    }
    fg_run_lines(lines, line_count);
    return 0;
}

int forge_is_ui_file(const ram_file_t *file) {
    if (!file || file->is_dir) return 0;
    int has_window = 0;
    int has_draw = 0;
    char line[FG_MAX_LINE];
    int length = 0;

    for (uint32_t i = 0; i <= file->size; i++) {
        char c = i < file->size ? (char)file->data[i] : '\n';
        if (c != '\n' && length < FG_MAX_LINE - 1) {
            line[length++] = c;
            continue;
        }
        line[length] = '\0';
        fg_trim(line);
        if (line[0] && line[0] != '#') {
            if (kstrncmp(line, "window ", 7) == 0)
                has_window = 1;
            if (kstrncmp(line, "func draw(", 10) == 0 ||
                kstrcmp(line, "func draw:") == 0)
                has_draw = 1;
        }
        length = 0;
        if (has_window && has_draw) return 1;
    }
    return 0;
}

static void *fg_ui_open(void) {
    return g_ui_pending;
}

static void fg_ui_close(void *st) {
    if (st) kfree(st);
}

static void fg_ui_activate(fg_ui_state_t *ui, fg_engine_t **previous_engine,
                           fg_ui_state_t **previous_ui) {
    *previous_engine = g_fg_current;
    *previous_ui = g_ui_current;
    g_fg_current = &ui->engine;
    g_ui_current = ui;
}

static void fg_ui_restore(fg_engine_t *previous_engine,
                          fg_ui_state_t *previous_ui) {
    g_fg_current = previous_engine;
    g_ui_current = previous_ui;
}

static void fg_ui_draw(void *st, window_t *w) {
    fg_ui_state_t *ui = (fg_ui_state_t *)st;
    if (!ui || !w) return;
    fg_engine_t *previous_engine;
    fg_ui_state_t *previous_ui;
    fg_ui_activate(ui, &previous_engine, &previous_ui);

    ui->draw_x = w->x + 8;
    ui->draw_y = w->y + DE_TITLEBAR_H + 8;
    ui->widget_count = 0;
    ui->drawing = 1;
    g_fg.return_active = 0;
    fg_func_t *draw = fg_func_find("draw");
    if (draw) fg_call_func("draw", "");
    else de_text(ui->draw_x, ui->draw_y, "Missing func draw():", g_theme->widget_txt);
    ui->drawing = 0;

    fg_ui_restore(previous_engine, previous_ui);
}

static void fg_ui_click(void *st, window_t *w, int mx, int my) {
    fg_ui_state_t *ui = (fg_ui_state_t *)st;
    if (!ui || !w) return;
    fg_engine_t *previous_engine;
    fg_ui_state_t *previous_ui;
    fg_ui_activate(ui, &previous_engine, &previous_ui);
    int local_x = mx - (w->x + 8);
    int local_y = my - (w->y + DE_TITLEBAR_H + 8);

    for (int i = ui->widget_count - 1; i >= 0; i--) {
        fg_ui_widget_t *widget = &ui->widgets[i];
        if (local_x < widget->x || local_x >= widget->x + widget->w ||
            local_y < widget->y || local_y >= widget->y + widget->h)
            continue;
        if (widget->kind == 1) {
            fg_ui_widget_t *checkbox = widget;
            const char *old_value = fg_var_get(checkbox->variable);
            fg_var_set(checkbox->variable, fg_ui_truthy(old_value) ? "0" : "1");
        }
        if (widget->callback[0]) fg_call_func(widget->callback, "");
        g_fg.return_active = 0;
        break;
    }
    fg_ui_restore(previous_engine, previous_ui);
}

int forge_ui_launch(ram_file_t *file) {
    if (!file || file->is_dir) return 1;
    fg_ui_state_t *ui = (fg_ui_state_t *)kmalloc(sizeof(fg_ui_state_t));
    if (!ui) return 1;
    kmemset(ui, 0, sizeof(*ui));
    kstrcpy(ui->title, "Forge App");
    ui->width = 420;
    ui->height = 300;

    fg_engine_t *previous_engine;
    fg_ui_state_t *previous_ui;
    fg_ui_activate(ui, &previous_engine, &previous_ui);
    int rc = fg_execute_file(file);
    int has_draw = fg_func_find("draw") != NULL;
    fg_ui_restore(previous_engine, previous_ui);
    if (rc != 0 || !has_draw) {
        kfree(ui);
        return 1;
    }

    int max_w = de_fb_w();
    int max_h = de_screen_bottom();
    if (max_w > 0 && ui->width > max_w) ui->width = max_w;
    if (max_h > DE_TITLEBAR_H && ui->height > max_h) ui->height = max_h;
    ui->app.id = "forge";
    ui->app.name = ui->title;
    ui->app.icon_char = "F";
    ui->app.default_w = ui->width;
    ui->app.default_h = ui->height;
    ui->app.on_open = fg_ui_open;
    ui->app.on_close = fg_ui_close;
    ui->app.on_draw = fg_ui_draw;
    ui->app.on_click = fg_ui_click;

    int x = (de_fb_w() - ui->width) / 2;
    int y = (de_screen_bottom() - ui->height) / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    g_ui_pending = ui;
    int window_index = de_wm_create_app(&ui->app, x, y, ui->width, ui->height);
    g_ui_pending = NULL;
    if (window_index < 0) {
        kfree(ui);
        return 1;
    }
    return 0;
}

int forge_run_file(ram_file_t *file) {
    if (!file || file->is_dir) return 1;
    fg_engine_t *previous_engine = g_fg_current;
    fg_ui_state_t *previous_ui = g_ui_current;
    g_fg_current = &g_fg_default;
    g_ui_current = NULL;
    kmemset(&g_fg, 0, sizeof(g_fg));
    int rc = fg_execute_file(file);
    g_fg_current = previous_engine;
    g_ui_current = previous_ui;
    return rc;
}

int forge_run_source(const char *src, uint32_t size) {
    ram_file_t tmp;
    kmemset(&tmp, 0, sizeof(tmp));
    kstrcpy(tmp.name, "__forge_inline__");
    if (size > MAX_FILE_SIZE) size = MAX_FILE_SIZE;
    kmemcpy(tmp.data, src, size);
    tmp.size = size;
    tmp.used = 1;
    return forge_run_file(&tmp);
}

int forge_repl(void) {
    g_fg_current = &g_fg_default;
    g_ui_current = NULL;
    kmemset(&g_fg, 0, sizeof(g_fg));
    fg_write("Forge REPL. Type 'exit' or 'quit' to return.\n");

    char line[FG_MAX_LINE];
    for (;;) {
        fg_write("forge> ");
        int length = 0;
        for (;;) {
            char c = keyboard_getchar();
            if (c == '\r' || c == '\n') {
                terminal_putchar('\n');
                break;
            }
            if (c == '\b' || c == 127) {
                if (length > 0) {
                    length--;
                    terminal_putchar('\b');
                }
                continue;
            }
            if (c >= 32 && c < 127 && length < FG_MAX_LINE - 1) {
                line[length++] = c;
                terminal_putchar(c);
            }
        }
        line[length] = '\0';
        fg_trim(line);
        if (kstrcmp(line, "exit") == 0 || kstrcmp(line, "quit") == 0)
            break;
        if (!line[0]) continue;

        char lines[1][FG_MAX_LINE];
        kstrncpy(lines[0], line, FG_MAX_LINE - 1);
        lines[0][FG_MAX_LINE - 1] = '\0';
        fg_run_lines(lines, 1);
        g_fg.return_active = 0;
        g_fg.break_active = 0;
    }

    forge_set_output(NULL, NULL);
    return 0;
}
