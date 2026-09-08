/* keyboard.c — PS/2 keyboard driver, US QWERTY, IRQ1 */
#include "keyboard.h"
#include "idt.h"
#include "pic.h"
#include "io.h"
#include "screen.h"

#define KBD_DATA_PORT 0x60
#define LINE_BUF_SIZE 256

static const char scancode_ascii[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']', '\n',
    0, 'a','s','d','f','g','h','j','k','l',';','\'','`',
    0, '\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0, ' ', 0,
};

/* all three are written by keyboard_handler (an IRQ that fires between
 * any two instructions) and read by normal kernel code, so none of them
 * may be cached in a register across the wait loops */
static volatile char line_buf[LINE_BUF_SIZE];
static volatile int line_len = 0;
static volatile int line_ready = 0;

struct interrupt_frame;

__attribute__((interrupt))
void keyboard_handler(struct interrupt_frame *frame) {
    (void) frame;
    unsigned char scancode = inb(KBD_DATA_PORT);

    if (!(scancode & 0x80)) {
        char c = (scancode < 128) ? scancode_ascii[scancode] : 0;

        if (c == '\n') {
            print_char('\n', 0x0f);
            line_buf[line_len] = 0;
            line_ready = 1;
        } else if (c == '\b') {
            if (line_len > 0) {
                line_len--;
                backspace();
            }
        } else if (c && line_len < LINE_BUF_SIZE - 1) {
            line_buf[line_len++] = c;
            print_char(c, 0x0f);
        }
    }

    outb(0x20, 0x20);
}

void keyboard_install(void) {
    set_idt_gate(0x21, (unsigned int) keyboard_handler);
    pic_unmask_irq(1);
}

int keyboard_read_line(char *out, int max_len) {
    if (!line_ready) return 0;

    int i = 0;
    while (line_buf[i] != 0 && i < max_len - 1) {
        out[i] = line_buf[i];
        i++;
    }
    out[i] = 0;

    line_len = 0;
    line_ready = 0;
    return 1;
}
