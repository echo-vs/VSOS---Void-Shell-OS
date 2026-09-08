; disk_load.asm — loads the kernel from disk into es:bx.
;
; Two paths, and the choice is printed as a checkpoint so a failure says
; which one was taken:
;
;   'D' entered disk_load   'L' using LBA   'C' using CHS
;   'A' all sectors read    'E' BIOS reported an error
;
; Preferred path is INT 13h extensions (AH=42h) with a Disk Address
; Packet. Those address the disk as a flat run of sectors, so they have
; no sectors-per-track ceiling. The old CHS path below does: it holds
; head and cylinder at 0 and only varies the sector number, so it can
; only ever reach the first track. Raising the sector count to 60 on that
; path hung the loader right after the 'D' checkpoint, which is what
; capped the kernel at 15KB until now.
;
; The CHS loop is kept as a fallback for a BIOS without extensions. Note
; it can only deliver about the first 30 sectors, so if it ever actually
; runs with today's sector count it will likely fail at 'E' - that is a
; deliberate trade: extensions are universal on anything this OS runs on
; (SeaBIOS included), and a loud failure beats silently loading half a
; kernel.
;
; Both paths read ONE sector per BIOS call. A single multi-sector call
; was found to hang on this setup, and boot time is irrelevant here.
;
; in:  dl = drive number, dh = sector count, es:bx = destination buffer

disk_load:
    mov al, 'D'
    mov ah, 0x0E
    int 0x10

    mov [SECT_LEFT], dh
    mov [DRIVE], dl
    mov [DEST_OFF], bx
    mov byte [SECTOR_NUM], 2      ; CHS: sector numbers are 1-based, 1 = boot sector

    mov [DAP_OFF], bx             ; LBA: same destination, filled into the packet
    mov ax, es
    mov [DAP_SEG], ax
    mov word [DAP_LBA_LO], 1      ; LBA is 0-based, so CHS sector 2 == LBA 1

    ; --- does this BIOS support INT 13h extensions? ---
    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, [DRIVE]
    int 0x13
    jc .use_chs                   ; CF set: no extension support
    cmp bx, 0xAA55                ; BX must come back byte-swapped
    jne .use_chs
    test cl, 1                    ; bit 0: extended read/write available
    jz .use_chs

.use_lba:
    mov al, 'L'
    mov ah, 0x0E
    int 0x10

.lba_loop:
    cmp byte [SECT_LEFT], 0
    je .done

    mov word [DAP_COUNT], 1       ; AH=42h writes the transferred count back
                                  ; into this field, so re-arm it every pass
    mov ah, 0x42
    mov dl, [DRIVE]
    mov si, DAP
    int 0x13
    jc disk_error

    add word [DAP_OFF], 512
    inc word [DAP_LBA_LO]         ; 16 bits is plenty: we read tens of sectors
    dec byte [SECT_LEFT]
    jmp .lba_loop

.use_chs:
    mov al, 'C'
    mov ah, 0x0E
    int 0x10

    mov bx, [DEST_OFF]            ; the AH=41h probe clobbered bx

.chs_loop:
    cmp byte [SECT_LEFT], 0
    je .done

    mov ah, 0x02
    mov al, 1
    mov ch, 0
    mov cl, [SECTOR_NUM]
    mov dh, 0
    mov dl, [DRIVE]
    int 0x13
    jc disk_error

    add bx, 512
    inc byte [SECTOR_NUM]
    dec byte [SECT_LEFT]
    jmp .chs_loop

.done:
    mov al, 'A'
    mov ah, 0x0E
    int 0x10
    ret

disk_error:
    mov al, 'E'
    mov ah, 0x0E
    int 0x10
    jmp $

SECT_LEFT:  db 0
DRIVE:      db 0
SECTOR_NUM: db 0
DEST_OFF:   dw 0

; Disk Address Packet for INT 13h AH=42h — exactly 16 bytes, and the
; field order is fixed by the BIOS interface.
DAP:
            db 0x10               ; packet size
            db 0                  ; reserved
DAP_COUNT:  dw 1                  ; sectors per call
DAP_OFF:    dw 0                  ; destination offset
DAP_SEG:    dw 0                  ; destination segment
DAP_LBA_LO: dd 0                  ; starting LBA, low 32 bits
DAP_LBA_HI: dd 0                  ; starting LBA, high 32 bits
