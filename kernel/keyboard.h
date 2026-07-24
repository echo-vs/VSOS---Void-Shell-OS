#ifndef KEYBOARD_H
#define KEYBOARD_H

void keyboard_install(void);

/* returns 1 and fills *out if a full line (Enter pressed) is ready, else 0 */
int keyboard_read_line(char *out, int max_len);

#endif
