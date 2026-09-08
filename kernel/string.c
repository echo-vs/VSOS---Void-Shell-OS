#include "string.h"

int vs_strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return (unsigned char)(*a) - (unsigned char)(*b);
}

int vs_strncmp(const char *a, const char *b, int n) {
    for (int i = 0; i < n; i++) {
        if (a[i] != b[i]) return (unsigned char)a[i] - (unsigned char)b[i];
        if (a[i] == 0) return 0;
    }
    return 0;
}

unsigned int vs_strlen(const char *s) {
    unsigned int n = 0;
    while (s[n]) n++;
    return n;
}

void vs_itoa(unsigned int value, char *out) {
    if (value == 0) {
        out[0] = '0';
        out[1] = 0;
        return;
    }

    char tmp[11];
    int i = 0;
    while (value > 0 && i < 11) {
        tmp[i++] = '0' + (value % 10);
        value /= 10;
    }

    int j = 0;
    while (i > 0) {
        out[j++] = tmp[--i];
    }
    out[j] = 0;
}

unsigned int vs_atoi(const char *s) {
    while (*s == ' ') s++;
    unsigned int n = 0;
    while (*s >= '0' && *s <= '9') {
        n = n * 10 + (unsigned int) (*s - '0');
        s++;
    }
    return n;
}

/* --- freestanding libc shims ---
 * The compiler emits calls to these on its own (see string.h), so they
 * have to exist even where nothing in this codebase calls them by name.
 */

void *memset(void *dst, int c, __SIZE_TYPE__ n) {
    unsigned char *d = (unsigned char *) dst;
    for (__SIZE_TYPE__ i = 0; i < n; i++) d[i] = (unsigned char) c;
    return dst;
}

void *memcpy(void *dst, const void *src, __SIZE_TYPE__ n) {
    unsigned char *d = (unsigned char *) dst;
    const unsigned char *s = (const unsigned char *) src;
    for (__SIZE_TYPE__ i = 0; i < n; i++) d[i] = s[i];
    return dst;
}

void *memmove(void *dst, const void *src, __SIZE_TYPE__ n) {
    unsigned char *d = (unsigned char *) dst;
    const unsigned char *s = (const unsigned char *) src;
    if (d == s || n == 0) return dst;
    if (d < s) {
        for (__SIZE_TYPE__ i = 0; i < n; i++) d[i] = s[i];
    } else {
        /* overlapping with dst after src: copy back to front */
        for (__SIZE_TYPE__ i = n; i > 0; i--) d[i - 1] = s[i - 1];
    }
    return dst;
}

int memcmp(const void *a, const void *b, __SIZE_TYPE__ n) {
    const unsigned char *x = (const unsigned char *) a;
    const unsigned char *y = (const unsigned char *) b;
    for (__SIZE_TYPE__ i = 0; i < n; i++) {
        if (x[i] != y[i]) return (int) x[i] - (int) y[i];
    }
    return 0;
}
