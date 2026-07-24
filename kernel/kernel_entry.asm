; kernel_entry.asm — 32-bit entry point, hands off to C kernel_main
[BITS 32]
[GLOBAL start]
[EXTERN kernel_main]

start:
    call kernel_main
    jmp $
