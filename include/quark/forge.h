#ifndef QUARK_FORGE_H
#define QUARK_FORGE_H

#include <quark/kernel.h>

typedef void (*forge_output_fn)(void *context, char character);

int forge_run_file(ram_file_t *file);
int forge_run_source(const char *src, uint32_t size);
int forge_repl(void);
int forge_is_ui_file(const ram_file_t *file);
int forge_ui_launch(ram_file_t *file);
void forge_set_output(forge_output_fn output, void *context);

/* Simple atoi helper used by forge_interp */
static inline int32_t kstrtol_simple(const char *s) {
    int32_t n = 0;
    int neg = 0;
    if (!s) return 0;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (*s - '0');
        s++;
    }
    return neg ? -n : n;
}

#endif
