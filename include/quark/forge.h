#ifndef QUARK_FORGE_H
#define QUARK_FORGE_H

#include <quark/kernel.h>

int forge_run_file(ram_file_t *file);
int forge_run_source(const char *src, uint32_t size);

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
