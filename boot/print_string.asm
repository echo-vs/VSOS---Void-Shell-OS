; print_string.asm — 16-bit real mode teletype print, SI = pointer to null-terminated string
print_string:
    pusha
    mov ah, 0x0E
.loop:
    lodsb
    or al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    popa
    ret
