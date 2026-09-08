OS_NAME = VSOS
CC = gcc
LD = ld
ASM = nasm
OBJCOPY = objcopy

# -Os: the bootloader only loads KERNEL_MAX bytes, so code size is a hard
#      correctness constraint here, not a preference. Unoptimized builds
#      overflowed it and got silently truncated.
# -fno-asynchronous-unwind-tables: stops ~2.6KB of .eh_frame being emitted
#      (link.ld discards it too, this just avoids generating it).
# -fno-strict-aliasing: the disk code reinterprets sector buffers as
#      structs, which is exactly what strict aliasing assumes never happens.
CFLAGS = -m32 -ffreestanding -fno-pie -fno-stack-protector -mgeneral-regs-only \
         -Os -fno-asynchronous-unwind-tables -fno-strict-aliasing -c
LDFLAGS = -m elf_i386 -T link.ld --oformat binary

# Bytes of kernel the bootloader actually copies into RAM: must stay in
# sync with `mov dh, N` in boot/boot.asm (N sectors * 512).
KERNEL_MAX = 15360

KERNEL_C_OBJS = kernel/kernel.o kernel/screen.o kernel/idt.o kernel/pic.o kernel/keyboard.o \
                kernel/config.o kernel/vfs.o kernel/shell.o kernel/string.o kernel/editor.o kernel/fs.o \
                kernel/ata.o
KERNEL_ASM_OBJS = kernel/kernel_entry.o kernel/idt_load.o
KERNEL_DATA_OBJS = kernel/config_blob.o

all: os-image.bin fs.img

os-image.bin: boot.bin kernel.bin
	cat boot.bin kernel.bin > os-image.bin

# fs.img is a second, separate virtual disk dedicated to the filesystem
# (kernel/fs.c via kernel/ata.c, which addresses it as the primary IDE
# slave). It is NOT the boot disk, so growing it can't perturb the BIOS's
# CHS geometry translation for os-image.bin - that translation is what the
# real-mode loader (boot/disk_load.asm) depends on, and is fragile (see
# its comments). fs.img also has no prerequisites, so `make` only creates
# it once: it's persistent state (whatever files you've made in VSOS),
# not a disposable build artifact - `clean` deliberately leaves it alone.
fs.img:
	truncate -s 131072 fs.img

boot.bin: boot/boot.asm boot/print_string.asm boot/disk_load.asm boot/gdt.asm boot/switch_to_pm.asm
	$(ASM) -f bin boot/boot.asm -o boot.bin

kernel.bin: $(KERNEL_ASM_OBJS) $(KERNEL_C_OBJS) $(KERNEL_DATA_OBJS)
	$(LD) $(LDFLAGS) -o kernel.bin $(KERNEL_ASM_OBJS) $(KERNEL_C_OBJS) $(KERNEL_DATA_OBJS)
	@actual=$$(stat -c %s kernel.bin); \
	if [ $$actual -gt $(KERNEL_MAX) ]; then \
		echo ""; \
		echo "BUILD FAILED: kernel.bin is $$actual bytes, but the bootloader only"; \
		echo "loads $(KERNEL_MAX) ($$(( $(KERNEL_MAX) / 512 )) sectors, see 'mov dh' in boot/boot.asm)."; \
		echo "Truncating it here would silently drop $$(( actual - $(KERNEL_MAX) )) bytes of real code"; \
		echo "and data, and the kernel would misbehave in confusing ways at runtime."; \
		echo "Shrink the kernel, or raise both the sector count and KERNEL_MAX."; \
		echo ""; \
		rm -f kernel.bin; \
		exit 1; \
	fi; \
	echo "kernel.bin: $$actual / $(KERNEL_MAX) bytes ($$(( $(KERNEL_MAX) - actual )) free)"
	@# pad to the full load size: the loader reads KERNEL_MAX bytes no
	@# matter what, and reading past the end of the image fails
	truncate -s $(KERNEL_MAX) kernel.bin

kernel/config_blob.o: vsos.conf
	$(OBJCOPY) -I binary -O elf32-i386 -B i386 vsos.conf kernel/config_blob.o

kernel/%.o: kernel/%.asm
	$(ASM) -f elf32 $< -o $@

kernel/%.o: kernel/%.c
	$(CC) $(CFLAGS) $< -o $@

run: os-image.bin fs.img
	qemu-system-i386 -drive format=raw,file=os-image.bin -drive format=raw,file=fs.img

clean:
	rm -f boot.bin kernel.bin os-image.bin kernel/*.o

# wipes the filesystem disk: every file and directory made inside VSOS is
# lost, and the next boot re-seeds the defaults from vsos.conf. Separate
# from `clean` on purpose - `clean` rebuilds the OS, this throws away data.
reset-fs:
	rm -f fs.img
	$(MAKE) fs.img

.PHONY: all run clean reset-fs
