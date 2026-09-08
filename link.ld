/* link.ld — flat binary layout for the kernel, loaded at 0x1000 by
 * boot/disk_load.asm.
 *
 * Two things matter here beyond the obvious ordering:
 *
 * 1. Every ALLOC section lands in the flat binary, and the bootloader
 *    only copies a fixed number of sectors into RAM (see boot/boot.asm's
 *    `mov dh, N` and the Makefile's KERNEL_MAX). Anything past that is
 *    silently missing at runtime, so dead weight here is not free - it
 *    pushes real code and data off the end. `.eh_frame` (DWARF unwind
 *    tables) is exactly that: ~2.6KB a freestanding C kernel never
 *    reads, so it gets discarded.
 *
 * 2. The wildcards use `*(.foo*)`, not `*(.foo)`. GCC emits string
 *    literals into `.rodata.str1.1`, functions into `.text.*` with
 *    -ffunction-sections, etc. A bare `*(.rodata)` doesn't match those,
 *    leaving them to the linker's orphan placement - which works, but
 *    puts them wherever it likes rather than where this file says.
 */
ENTRY(start)
SECTIONS
{
    . = 0x1000;
    .text   : { *(.text*) }
    .rodata : { *(.rodata*) }
    .data   : { *(.data*) }
    .bss    : { *(.bss*) *(COMMON) }

    /DISCARD/ : {
        *(.eh_frame*)
        *(.comment*)
        *(.note*)
    }
}
