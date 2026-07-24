; disk_load.asm — loop of single-sector CHS reads (al=1 each time), proven
; to work on this system. A single call with al=60 hangs, so we just repeat
; the working single-sector call 60 times instead, incrementing the sector
; number and destination pointer each time. No geometry query needed since
; head=0/cylinder=0 stays fixed and sector numbers 2..61 fit fine.
; in:  dl = drive number, dh = sector count, es:bx = destination buffer
disk_load:
    mov al, 'D'          ; checkpoint: entered disk_load
    mov ah, 0x0E
    int 0x10

    mov [SECT_LEFT], dh
    mov [DRIVE], dl
    mov byte [SECTOR_NUM], 2   ; start right after the boot sector

.read_loop:
    mov al, [SECT_LEFT]
    cmp al, 0
    je .done

    mov ah, 0x02
    mov al, 1                     ; one sector per call — this is what works
    mov ch, 0
    mov cl, [SECTOR_NUM]
    mov dh, 0
    mov dl, [DRIVE]
    int 0x13
    jc disk_error

    add bx, 512
    inc byte [SECTOR_NUM]
    dec byte [SECT_LEFT]
    jmp .read_loop

.done:
    mov al, 'A'          ; checkpoint: all sectors read
    mov ah, 0x0E
    int 0x10
    ret

disk_error:
    mov al, 'E'          ; checkpoint: BIOS reported an error
    mov ah, 0x0E
    int 0x10
    jmp $

SECT_LEFT:  db 0
DRIVE:      db 0
SECTOR_NUM: db 0
