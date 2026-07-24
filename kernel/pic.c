/* pic.c — remap the 8259 PIC off the CPU exception vectors */
#include "pic.h"
#include "io.h"

#define PIC1 0x20
#define PIC2 0xA0
#define PIC1_DATA 0x21
#define PIC2_DATA 0xA1

void pic_remap(void) {
    unsigned char mask1 = inb(PIC1_DATA);
    unsigned char mask2 = inb(PIC2_DATA);

    outb(PIC1, 0x11);
    outb(PIC2, 0x11);

    outb(PIC1_DATA, 0x20);
    outb(PIC2_DATA, 0x28);

    outb(PIC1_DATA, 0x04);
    outb(PIC2_DATA, 0x02);

    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);

    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

void pic_unmask_irq(unsigned char irq) {
    unsigned short port = irq < 8 ? PIC1_DATA : PIC2_DATA;
    unsigned char irq_line = irq < 8 ? irq : irq - 8;
    unsigned char mask = inb(port);
    mask &= ~(1 << irq_line);
    outb(port, mask);
}
