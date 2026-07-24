#ifndef SCREEN_H
#define SCREEN_H

void clear_screen(void);
void print_char(char c, unsigned char color);
void print_string(const char *str, unsigned char color);
void backspace(void);

#endif
