# VSOS — Void Shell OS

A C+ Assembly mini OS (NASM), a real bootloader, is being tested in QEMU.
Unix-style commands + declarative config (in the spirit of Nix: you describe the state, the kernel applies it).

## Installing the tulchain

**Fedora:**
```
sudo dnf install nasm qemu gcc make gdb binutils
```
**Debian/Ubuntu:**
```
sudo apt install nasm qemu-system-x86 gcc make gdb binutils
```

## Build and launch
```
cd VSOS
make clean && make run
```

Expected result in QEMU: screen clears, kernel greeting, `motd` from `vsos.conf', prompt `void>'.

Check the commands: `help`, `ls', `cat vsos.conf`, `cat motd.txt `, `cat hostname`, `echo hello`, `uname`, `whoami`, `clear`, `reboot`.

## Debugging (if something is hanging on loading)

There are checkpoints in `boot.asm` and `switch_to_pm.asm`:
- text `"kernel read OK, entering protected mode..."` — is printed if the disk has been read successfully
- the letters `PM` in the upper-left corner of the screen (yellow) — if you are in protected mode
- the letter `K` next to it is right before the jump into the core

If one of these things does not appear, it means that it is hanging at this step. It is also useful to run with `-no-reboot -no-shutdown` to distinguish a real freeze from a QEMU silently restarting VM with a triple fold:
``
qemu-system-i386 -drive format=raw,file=os-image.bin -no-reboot -no-shutdown
```

## Roadmap

- [x] **Stage 1** — bootloader (real mode, BIOS `int 0x10`)
- [x] **Stage 2** — booting kernel.bin from disk (LBA, `int 0x13 ah=42h'), switching to 32-bit protected mode (GDT), transfer of control to the C-core
- [x] **Stage 3** — IDT/interrupts, PIC remap, PS/2 keyboard
- [x] **Stage 4** — unix-style shell (`ls`, `cat`, `echo`, `uname`, `whoami`, `reboot`, `help`, `clear`) + declarative `vsos.conf`, embedded in the kernel via `objcopy' and applied at boot
- [ ] **Stage 5** — the real file system on disk (now VFS is a stub in memory)
— [ ] **Stage 6** - processes / memory (malloc, padding)
- [ ] **Package Manager** — offline via the second disk (`.vpkg`), see the ideas in the project notes
- [ ] **Network** — PCI enumeration → RTL8139 driver → ARP/IP/UDP → (someday) TCP

## Complete project structure
```
VSOS/
├── Makefile
├── link.ld
├── vsos.conf
├── README.md
├── ABOUT.md
├── LICENSE
├── boot/
│   ├── boot.asm
│   ├── print_string.asm
│   ├── disk_load.asm
│   ├── gdt.asm
│   └── switch_to_pm.asm
└── kernel/
    ├── kernel_entry.asm
    ├── idt_load.asm
    ├── kernel.c
    ├── io.h
    ├── screen.h / screen.c
    ├── idt.h / idt.c
    ├── pic.h / pic.c
    ├── keyboard.h / keyboard.c
    ├── config.h / config.c
    ├── vfs.h / vfs.c
    ├── shell.h / shell.c
    └── string.h / string.c
```

## How everything is connected (download order)
1. The BIOS loads `boot.asm' (sector 1, `0x7C00`), prints a greeting.
2. 'disk_load.asm` reads 60 sectors of kernel.bin into memory `0x1000` via LBA (`int 13h, ah=42h`).
3. `switch_to_pm.asm': `lgdt', PE bit in `cr0', far jump → 32-bit protected mode.
4. `kernel_entry.asm` calls `kernel_main()`(C).
5. `kernel_main`: `idt_install()' → `pic_remap()` → `keyboard_install()` → `sti` → `config_load()` (parses `vsos.conf`) → `shell_init()` (prints motd) → main loop on `keyboard_read_line()' + `shell_execute()'.
# VSOS---Void-Shell-OS
