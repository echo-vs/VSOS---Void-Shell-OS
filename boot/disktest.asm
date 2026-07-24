; disktest.asm — absolute minimal test: read ONE sector, nothing else.
; If this hangs, the problem is disk I/O itself in this QEMU environment.
; If it works, the bug is somewhere in our loop logic.
BITS 16
ORG 0x7C00

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov al, '1'
    mov ah, 0x0E
    int 0x10

    mov bx, 0x8000      ; read into a scratch buffer, well away from us
    mov ah, 0x02        ; read sector(s)
    mov al, 1            ; just 1 sector
    mov ch, 0             ; cylinder 0
    mov cl, 2              ; sector 2 (1-indexed, skip boot sector)
    mov dh, 0              ; head 0
    ; dl already holds the boot drive number from BIOS, don't touch it
    int 0x13

    mov al, '2'
    mov ah, 0x0E
    int 0x10

    jc .fail

    mov al, 'K'          ; success!
    mov ah, 0x0E
    int 0x10
    jmp $

.fail:
    mov al, 'F'          ; carry was set = BIOS reported an error
    mov ah, 0x0E
    int 0x10
    jmp $

times 510-($-$$) db 0
dw 0xAA55
