#ifndef PIC_H
#define PIC_H

/* remaps master PIC to interrupts 0x20-0x27, slave to 0x28-0x2F */
void pic_remap(void);

/* unmask a specific IRQ line (0-15) so it can fire */
void pic_unmask_irq(unsigned char irq);

#endif
