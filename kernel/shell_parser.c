/* =============================================================================
 * Quark-OS kernel/shell_parser.c
 * Bash-style line parser: tokenization, quoting, redirection, semicolons
 * ============================================================================= */

#include <quark/shell.h>

static int is_space(char c) {
    return c == ' ' || c == '\t';
}

static void skip_spaces(const char **p) {
    while (**p && is_space(**p)) (*p)++;
}

static int read_token(const char **p, char *dst, int dst_size) {
    int i = 0;
    skip_spaces(p);

    if (**p == '"') {
        (*p)++;
        while (**p && **p != '"') {
            if (i < dst_size - 1) dst[i++] = *(*p)++;
        }
        if (**p == '"') (*p)++;
    } else if (**p == '\'') {
        (*p)++;
        while (**p && **p != '\'') {
            if (i < dst_size - 1) dst[i++] = *(*p)++;
        }
        if (**p == '\'') (*p)++;
    } else {
        while (**p && !is_space(**p) && **p != '>' && **p != ';') {
            if (i < dst_size - 1) dst[i++] = *(*p)++;
        }
    }

    dst[i] = '\0';
    return i;
}

int shell_parse_line(const char *line, shell_parsed_t *out) {
    const char *p = line;
    char token[SHELL_MAX_ARG_LEN];
    int storage_idx = 0;

    kmemset(out, 0, sizeof(*out));
    out->redirect_append = 0;
    out->redirect_out[0] = '\0';

    skip_spaces(&p);
    if (*p == '\0') return 0;

    while (*p && out->argc < SHELL_MAX_ARGS) {
        if (*p == ';') {
            p++;
            break;
        }

        if (*p == '>' && *(p + 1) == '>') {
            p += 2;
            read_token(&p, token, sizeof(token));
            if (kstrlen(token) > 0) {
                kstrcpy(out->redirect_out, token);
                out->redirect_append = 1;
            }
            break;
        }

        if (*p == '>') {
            p++;
            read_token(&p, token, sizeof(token));
            if (kstrlen(token) > 0) {
                kstrcpy(out->redirect_out, token);
                out->redirect_append = 0;
            }
            break;
        }

        read_token(&p, token, sizeof(token));
        if (kstrlen(token) == 0) {
            skip_spaces(&p);
            continue;
        }

        if (storage_idx + (int)kstrlen(token) + 1 >= SHELL_MAX_LINE) break;

        out->argv[out->argc] = &out->arg_storage[storage_idx];
        kstrcpy(out->argv[out->argc], token);
        storage_idx += (int)kstrlen(token) + 1;
        out->argc++;
        skip_spaces(&p);
    }

    return out->argc;
}
