/* idt.c — Interrupt Descriptor Table setup */
#include "idt.h"
#include "io.h"

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  type_attr;
    uint16_t offset_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

#define IDT_SIZE 256

static struct idt_entry idt[IDT_SIZE];
static struct idt_ptr idtp;

extern void idt_load(uint32_t idtp_addr);

struct interrupt_frame;

/* catches any interrupt/exception we haven't installed a real handler for
 * (e.g. the timer IRQ0, which BIOS leaves unmasked). Without this, such an
 * interrupt hits an empty/not-present gate and triple-faults the CPU. */
__attribute__((interrupt))
static void default_isr(struct interrupt_frame *frame) {
    (void) frame;
    outb(0x20, 0x20);   /* harmless EOI to both PICs even if this wasn't one */
    outb(0xA0, 0x20);
}

void set_idt_gate(int n, uint32_t handler) {
    idt[n].offset_low  = handler & 0xFFFF;
    idt[n].selector    = 0x08;      /* kernel code segment, matches gdt.asm */
    idt[n].zero        = 0;
    idt[n].type_attr   = 0x8E;      /* present, ring0, 32-bit interrupt gate */
    idt[n].offset_high = (handler >> 16) & 0xFFFF;
}

void idt_install(void) {
    for (int i = 0; i < IDT_SIZE; i++) {
        set_idt_gate(i, (uint32_t) default_isr);
    }

    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint32_t) &idt;
    idt_load((uint32_t) &idtp);
}
