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
    ; Sectors of kernel to load (must match KERNEL_MAX in the Makefile:
    ; N * 512). 30 was the old ceiling, imposed by CHS reads that could
    ; only reach the first track; disk_load.asm now prefers LBA, which has
    ; no such limit.
    ;
    ; What limits it now is memory, not the disk: the kernel lands at
    ; KERNEL_OFFSET (0x1000) and this loader plus its stack live at
    ; 0x7C00. 48 sectors fills 0x1000..0x7000, leaving 3KB of headroom
    ; below 0x7C00 for the stack the BIOS calls use. Going much past this
    ; would have the loader overwrite itself mid-read. To grow further the
    ; kernel has to be loaded above the 1MB line instead.
    mov dh, 48
    mov dl, [BOOT_DRIVE]
    call disk_load
    ret

BOOT_DRIVE: db 0
; kept short on purpose: the whole boot sector is 512 bytes, and the
; kernel prints its own banner a moment later anyway
MSG_REAL_MODE: db "VSOS loading...", 13, 10, 0
MSG_KERNEL_OK: db "kernel read OK, entering PM", 13, 10, 0

; pad to 510 bytes, then boot signature
times 510-($-$$) db 0
dw 0xAA55
