OS_NAME = VSOS
CC = gcc
LD = ld
ASM = nasm
OBJCOPY = objcopy

CFLAGS = -m32 -ffreestanding -fno-pie -fno-stack-protector -mgeneral-regs-only -c
LDFLAGS = -m elf_i386 -T link.ld --oformat binary

KERNEL_C_OBJS = kernel/kernel.o kernel/screen.o kernel/idt.o kernel/pic.o kernel/keyboard.o \
                kernel/config.o kernel/vfs.o kernel/shell.o kernel/string.o kernel/editor.o
KERNEL_ASM_OBJS = kernel/kernel_entry.o kernel/idt_load.o
KERNEL_DATA_OBJS = kernel/config_blob.o

all: os-image.bin

os-image.bin: boot.bin kernel.bin
	cat boot.bin kernel.bin > os-image.bin

boot.bin: boot/boot.asm boot/print_string.asm boot/disk_load.asm boot/gdt.asm boot/switch_to_pm.asm
	$(ASM) -f bin boot/boot.asm -o boot.bin

kernel.bin: $(KERNEL_ASM_OBJS) $(KERNEL_C_OBJS) $(KERNEL_DATA_OBJS)
	$(LD) $(LDFLAGS) -o kernel.bin $(KERNEL_ASM_OBJS) $(KERNEL_C_OBJS) $(KERNEL_DATA_OBJS)
	truncate -s 15360 kernel.bin

kernel/config_blob.o: vsos.conf
	$(OBJCOPY) -I binary -O elf32-i386 -B i386 vsos.conf kernel/config_blob.o

kernel/%.o: kernel/%.asm
	$(ASM) -f elf32 $< -o $@

kernel/%.o: kernel/%.c
	$(CC) $(CFLAGS) $< -o $@

run: os-image.bin
	qemu-system-i386 -drive format=raw,file=os-image.bin

clean:
	rm -f boot.bin kernel.bin os-image.bin kernel/*.o

.PHONY: all run clean
