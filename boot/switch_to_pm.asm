; switch_to_pm.asm — enable A20-agnostic PE bit, load GDT, far jump into 32-bit code
[BITS 16]
switch_to_pm:
    cli                     ; interrupts off, no IVT in protected mode yet
    lgdt [gdt_descriptor]

    mov eax, cr0
    or eax, 0x1
    mov cr0, eax

    jmp CODE_SEG:init_pm    ; far jump forces CPU to flush pipeline / enter 32-bit

[BITS 32]
init_pm:
    mov ax, DATA_SEG
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov ebp, 0x90000
    mov esp, ebp

    ; debug checkpoint: write "PM" directly to VGA text memory (top-left)
    mov byte [0xb8000], 'P'
    mov byte [0xb8001], 0x4F
    mov byte [0xb8002], 'M'
    mov byte [0xb8003], 0x4F

    call BEGIN_PM

[BITS 32]
BEGIN_PM:
    ; debug checkpoint: write "K" right before jumping into the kernel
    mov byte [0xb8004], 'K'
    mov byte [0xb8005], 0x4F

    call KERNEL_OFFSET      ; hand off to the loaded kernel
    jmp $
