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
