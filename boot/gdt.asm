; gdt.asm — minimal flat GDT: null, code, data segments
gdt_start:

gdt_null:
    dd 0x0
    dd 0x0

gdt_code:
    dw 0xFFFF        ; limit low
    dw 0x0           ; base low
    db 0x0           ; base middle
    db 10011010b     ; access: present, ring0, code, executable, readable
    db 11001111b     ; flags + limit high (4K granularity, 32-bit)
    db 0x0           ; base high

gdt_data:
    dw 0xFFFF
    dw 0x0
    db 0x0
    db 10010010b     ; access: present, ring0, data, writable
    db 11001111b
    db 0x0

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1   ; size
    dd gdt_start                  ; address

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start
