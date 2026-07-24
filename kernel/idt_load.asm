; idt_load.asm — load the IDT register
[BITS 32]
[GLOBAL idt_load]

idt_load:
    mov eax, [esp+4]
    lidt [eax]
    ret
