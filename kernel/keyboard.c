/* keyboard.c — PS/2 keyboard driver, US QWERTY, IRQ1.
 *
 * The handler does one job: translate a scancode and put the key in a
 * queue. It deliberately does not echo or edit a line, so that the code
 * which does (readline.c) can run in normal context and be free to touch
 * the screen, the shell's command table and the filesystem.
 */
#include "keyboard.h"
#include "idt.h"
#include "pic.h"
#include "io.h"

#define KBD_DATA_PORT 0x60
#define QUEUE_SIZE 32

static const char scancode_ascii[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']', '\n',
    0, 'a','s','d','f','g','h','j','k','l',';','\'','`',
    0, '\\','z','x','c','v','b','n','m',',','.','/', 0,
    '*', 0, ' ', 0,
};

/* single producer (the ISR) writes head, single consumer reads tail, so
 * no locking is needed between them */
static volatile int queue[QUEUE_SIZE];
static volatile int q_head = 0;
static volatile int q_tail = 0;

/* arrow keys and friends arrive as 0xE0 followed by the real code */
static volatile int ext_pending = 0;

static void queue_push(int key) {
    int next = (q_head + 1) % QUEUE_SIZE;
    if (next == q_tail) return;      /* full: drop the key rather than wrap */
    queue[q_head] = key;
    q_head = next;
}

struct interrupt_frame;

__attribute__((interrupt))
void keyboard_handler(struct interrupt_frame *frame) {
    (void) frame;
    unsigned char scancode = inb(KBD_DATA_PORT);

    if (scancode == 0xE0) {
        ext_pending = 1;
        outb(0x20, 0x20);
        return;
    }

    if (ext_pending) {
        ext_pending = 0;
        if (!(scancode & 0x80)) {        /* press, not release */
            if (scancode == 0x48) queue_push(KEY_UP);
            else if (scancode == 0x50) queue_push(KEY_DOWN);
            else if (scancode == 0x4B) queue_push(KEY_LEFT);
            else if (scancode == 0x4D) queue_push(KEY_RIGHT);
        }
        outb(0x20, 0x20);
        return;
    }

    if (!(scancode & 0x80)) {
        char c = (scancode < 128) ? scancode_ascii[scancode] : 0;
        if (c) queue_push((int) (unsigned char) c);
    }

    outb(0x20, 0x20);
}

void keyboard_install(void) {
    set_idt_gate(0x21, (unsigned int) keyboard_handler);
    pic_unmask_irq(1);
}

int keyboard_getch(void) {
    while (1) {
        __asm__ volatile ("cli");
        if (q_head != q_tail) {
            int k = queue[q_tail];
            q_tail = (q_tail + 1) % QUEUE_SIZE;
            __asm__ volatile ("sti");
            return k;
        }
        /* `sti; hlt` as one pair: the CPU guarantees an interrupt arriving
         * between them isn't lost, which a plain "check, then hlt" would
         * miss - and then we'd sleep until some later, unrelated key */
        __asm__ volatile ("sti; hlt");
    }
}

int keyboard_poll(void) {
    if (q_head == q_tail) return 0;
    int k = queue[q_tail];
    q_tail = (q_tail + 1) % QUEUE_SIZE;
    return k;
}
