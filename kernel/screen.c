/* screen.c — VGA text mode (0xb8000), tracks cursor position */
#include "screen.h"
#include "io.h"

/* volatile: these stores are the visible output of the kernel, but to the
 * compiler they look like writes to ordinary memory that is never read
 * back - with optimization on it would be free to drop or reorder them */
#define VIDMEM ((volatile char *) 0xb8000)
#define COLS 80
#define ROWS 25

static int cursor_row = 0;
static int cursor_col = 0;

static void update_hw_cursor(void) {
    unsigned short pos = cursor_row * COLS + cursor_col;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (unsigned char)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (unsigned char)((pos >> 8) & 0xFF));
}

static void scroll_if_needed(void) {
    if (cursor_row < ROWS) return;

    for (int i = 0; i < (ROWS - 1) * COLS * 2; i++) {
        VIDMEM[i] = VIDMEM[i + COLS * 2];
    }
    for (int i = (ROWS - 1) * COLS * 2; i < ROWS * COLS * 2; i += 2) {
        VIDMEM[i] = ' ';
        VIDMEM[i + 1] = 0x0f;
    }
    cursor_row = ROWS - 1;
}

void clear_screen(void) {
    for (int i = 0; i < COLS * ROWS * 2; i += 2) {
        VIDMEM[i] = ' ';
        VIDMEM[i + 1] = 0x0f;
    }
    cursor_row = 0;
    cursor_col = 0;
    update_hw_cursor();
}

void print_char(char c, unsigned char color) {
    if (c == '\n') {
        cursor_row++;
        cursor_col = 0;
    } else if (c == '\r') {
        cursor_col = 0;
    } else {
        int offset = (cursor_row * COLS + cursor_col) * 2;
        VIDMEM[offset] = c;
        VIDMEM[offset + 1] = color;
        cursor_col++;
        if (cursor_col >= COLS) {
            cursor_col = 0;
            cursor_row++;
        }
    }
    scroll_if_needed();
    update_hw_cursor();
}

void print_string(const char *str, unsigned char color) {
    int i = 0;
    while (str[i] != 0) {
        print_char(str[i], color);
        i++;
    }
}

void backspace(void) {
    if (cursor_col == 0 && cursor_row == 0) return;
    if (cursor_col == 0) {
        cursor_row--;
        cursor_col = COLS - 1;
    } else {
        cursor_col--;
    }
    int offset = (cursor_row * COLS + cursor_col) * 2;
    VIDMEM[offset] = ' ';
    VIDMEM[offset + 1] = 0x0f;
    update_hw_cursor();
}
