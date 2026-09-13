/*
 * kernel/forge_interp.c — Forge_High interpreter for Quark-OS-3 shell
 * Run Forge programs from ramfs:  forge script.fg
 */

#include <quark/kernel.h>
#include <quark/forge.h>

#define FG_MAX_VARS    64
#define FG_MAX_LINE    256
#define FG_MAX_VAL     512
#define FG_MAX_FUNCS   16
#define FG_MAX_BODY    64
#define FG_MAX_PARAMS  8

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
    int       call_depth;
} fg_engine_t;

static fg_engine_t g_fg;

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
    while (*p && *p != '+' && *p != '-' && *p != '*' && *p != '/') p++;
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

static int fg_collect_block(const char *src, uint32_t size, uint32_t *pos,
                            char body[][FG_MAX_LINE], int max_body) {
    int count = 0;
    char line[FG_MAX_LINE];
    int li = 0;
    while (*pos < size && count < max_body) {
        char c = src[*pos];
        (*pos)++;
        if (c == '\n' || li >= FG_MAX_LINE - 1) {
            line[li] = '\0';
            li = 0;
            if (line[0] == '\0' || line[0] == '#') continue;
            if (line[0] != ' ' && line[0] != '\t') {
                (*pos)--;
                break;
            }
            const char *p = line;
            while (*p == ' ' || *p == '\t') p++;
            kstrncpy(body[count++], p, FG_MAX_LINE - 1);
        } else {
            line[li++] = c;
        }
    }
    return count;
}

static fg_func_t *fg_func_find(const char *name) {
    for (int i = 0; i < g_fg.func_count; i++)
        if (kstrcmp(g_fg.funcs[i].name, name) == 0)
            return &g_fg.funcs[i];
    return NULL;
}

static int fg_run_lines(char lines[][FG_MAX_LINE], int n);
static int fg_exec_line(const char *line);

static int fg_call_func(const char *fname, const char *argstr) {
    fg_func_t *fn = fg_func_find(fname);
    if (!fn) {
        kprintf("forge: undefined function '%s'\n", fname);
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
        char buf[FG_MAX_LINE];
        kstrcpy(buf, lines[i]);
        fg_trim(buf);
        if (!buf[0] || buf[0] == '#') { i++; continue; }

        if (kstrncmp(buf, "if ", 3) == 0) {
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
            if (fg_eval_cond(buf + 3))
                fg_run_lines(body, blen);
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
                fg_run_lines(body, blen);
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
        kprintf("%s\n", val);
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
            kprintf("%s", prompt);
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

    return 0;
}

int forge_run_file(ram_file_t *file) {
    if (!file || file->is_dir) return 1;
    kmemset(&g_fg, 0, sizeof(g_fg));

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
