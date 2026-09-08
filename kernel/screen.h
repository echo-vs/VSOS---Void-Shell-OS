#ifndef SCREEN_H
#define SCREEN_H

void clear_screen(void);
void print_char(char c, unsigned char color);
void print_string(const char *str, unsigned char color);
void backspace(void);

/* writes one cell at an absolute position without touching the cursor or
 * scrolling, for drawing rather than printing. Out-of-range coordinates
 * are ignored. The screen is 80x25. */
void print_char_at(int row, int col, char c, unsigned char color);

#endif
