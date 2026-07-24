; boot.asm — VSOS (Void Shell OS) Stage 1+2 bootloader
BITS 16
ORG 0x7C00

KERNEL_OFFSET equ 0x1000   ; where we load the kernel in memory

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [BOOT_DRIVE], dl    ; BIOS passes boot drive number in dl

    mov si, MSG_REAL_MODE
    call print_string

    call load_kernel

    mov si, MSG_KERNEL_OK
    call print_string

    call switch_to_pm       ; never returns

    jmp $

%include "boot/print_string.asm"
%include "boot/disk_load.asm"
%include "boot/gdt.asm"
%include "boot/switch_to_pm.asm"

[BITS 16]
load_kernel:
    mov bx, KERNEL_OFFSET
    mov dh, 30                ; number of sectors to read (confirmed stable; kernel is ~6-7KB)
    mov dl, [BOOT_DRIVE]
    call disk_load
    ret

BOOT_DRIVE: db 0
MSG_REAL_MODE: db "VSOS - Void Shell OS", 13, 10, "loading kernel...", 13, 10, 0
MSG_KERNEL_OK: db "kernel read OK, entering protected mode...", 13, 10, 0

; pad to 510 bytes, then boot signature
times 510-($-$$) db 0
dw 0xAA55
