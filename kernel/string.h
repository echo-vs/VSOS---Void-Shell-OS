#ifndef STRING_H
#define STRING_H

int vs_strcmp(const char *a, const char *b);
int vs_strncmp(const char *a, const char *b, int n);
unsigned int vs_strlen(const char *s);

/* writes the decimal representation of value into out (out must have
 * room for at least 11 bytes: 10 digits + NUL, for a 32-bit value) */
void vs_itoa(unsigned int value, char *out);

#endif
