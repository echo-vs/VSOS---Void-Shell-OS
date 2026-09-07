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
