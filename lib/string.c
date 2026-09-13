/* =============================================================================
 * Quark-OS lib/string.c
 * Freestanding kernel standard library helper routines
 * ============================================================================= */

#include <quark/kernel.h>

size_t kstrlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

char *kstrcpy(char *dst, const char *src) {
    char *orig = dst;
    while ((*dst++ = *src++));
    return orig;
}

char *kstrcat(char *dst, const char *src) {
    char *orig = dst;
    while (*dst) dst++;
    while ((*dst++ = *src++));
    return orig;
}

int kstrcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) {
        a++;
        b++;
    }
    return *(unsigned char *)a - *(unsigned char *)b;
}

int kstrncmp(const char *a, const char *b, size_t n) {
    if (n == 0) return 0;
    while (n-- > 0) {
        if (*a != *b) return *(unsigned char *)a - *(unsigned char *)b;
        if (*a == 0) break;
        a++;
        b++;
    }
    return 0;
}

void *kmemset(void *dst, int c, size_t n) {
    unsigned char *p = dst;
    while (n--) *p++ = (unsigned char)c;
    return dst;
}

void *kmemcpy(void *dst, const void *src, size_t n) {
    char *d = dst;
    const char *s = src;
    while (n--) *d++ = *s++;
    return dst;
}

char *kitoa(int32_t n, char *buf, int base) {
    char *p = buf;
    char *p1, *p2;
    uint32_t ud = n;

    if (base == 10 && n < 0) {
        *p++ = '-';
        buf++;
        ud = -n;
    }

    int remainder;
    do {
        remainder = ud % base;
        *p++ = (remainder < 10) ? remainder + '0' : remainder - 10 + 'a';
    } while (ud /= base);

    *p = '\0';

    /* Reverse the digits inside the buffer */
    p1 = buf;
    p2 = p - 1;
    while (p1 < p2) {
        char tmp = *p1;
        *p1++ = *p2;
        *p2-- = tmp;
    }
    return buf;
}

char *kstrchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return (c == '\0') ? (char *)s : NULL;
}

char *kstrstr(const char *haystack, const char *needle) {
    if (!needle || !*needle) return (char *)haystack;
    for (; *haystack; haystack++) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && *h == *n) {
            h++;
            n++;
        }
        if (*n == '\0') return (char *)haystack;
    }
    return NULL;
}

char *kstrncpy(char *dst, const char *src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i]; i++) dst[i] = src[i];
    for (; i < n; i++) dst[i] = '\0';
    return dst;
}

char *kstrncat(char *dst, const char *src, size_t n) {
    char *orig = dst;
    while (*dst) dst++;
    while (n-- > 0 && *src) *dst++ = *src++;
    *dst = '\0';
    return orig;
}

char *kstrrchr(const char *s, int c) {
    const char *last = NULL;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    return (char *)last;
}

char *kuitoa(uint32_t n, char *buf, int base) {
    char *p = buf;
    char *p1, *p2;

    int remainder;
    do {
        remainder = n % base;
        *p++ = (remainder < 10) ? remainder + '0' : remainder - 10 + 'a';
    } while (n /= base);

    *p = '\0';

    p1 = buf;
    p2 = p - 1;
    while (p1 < p2) {
        char tmp = *p1;
        *p1++ = *p2;
        *p2-- = tmp;
    }
    return buf;
}
