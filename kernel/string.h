#ifndef STRING_H
#define STRING_H

int vs_strcmp(const char *a, const char *b);
int vs_strncmp(const char *a, const char *b, int n);
unsigned int vs_strlen(const char *s);

/* writes the decimal representation of value into out (out must have
 * room for at least 11 bytes: 10 digits + NUL, for a 32-bit value) */
void vs_itoa(unsigned int value, char *out);

/* parses a leading run of decimal digits (skipping leading spaces) into
 * an unsigned int; stops at the first non-digit. "" or no digits -> 0 */
unsigned int vs_atoi(const char *s);

/* The compiler is allowed to turn plain C loops (array init, struct
 * copy, buffer clear) into calls to these, and does so as soon as
 * optimization is on - there's no libc here to supply them, so the
 * kernel provides them itself or the link fails. __SIZE_TYPE__ is what
 * GCC itself uses for size_t, so these match its builtin prototypes. */
void *memset(void *dst, int c, __SIZE_TYPE__ n);
void *memcpy(void *dst, const void *src, __SIZE_TYPE__ n);
void *memmove(void *dst, const void *src, __SIZE_TYPE__ n);
int memcmp(const void *a, const void *b, __SIZE_TYPE__ n);

#endif
