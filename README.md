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

Expected result in QEMU: screen clears, kernel greeting, `motd` from `vsos.conf', prompt `void:/ #'.

Your files live on a second disk, `fs.img`, created automatically on first build. `make clean` leaves it alone so files survive rebuilds; `make reset-fs` wipes it and re-seeds the defaults.

### The kernel size ceiling (read this before adding code)

`boot/disk_load.asm` loads a **fixed 48 sectors = 24576 bytes** into RAM. Anything past that is simply never in memory, so an oversized kernel doesn't fail loudly - it runs with its tail missing and misbehaves in baffling ways. This actually happened: the kernel reached 19452 bytes against a then-15360-byte limit, and `command_count` (24 bytes past the cutoff) came up garbage, which silently broke only the *last* commands in the shell's table.

Three things keep this in check, and none should be removed casually:
- the build **fails** if `kernel.bin` exceeds `KERNEL_MAX` (Makefile), and prints the overflow
- `-Os` in `CFLAGS`: unoptimized builds are ~40% bigger, which is the difference between fitting and not
- `link.ld` discards `.eh_frame` (~2.6KB of unwind tables a C kernel never reads)

The limit used to be 30 sectors, and it was a *disk* limit: the loader read via CHS with head and cylinder pinned to 0, so it could only ever reach the first track, and asking for 60 sectors hung it. It now prefers `int 13h ah=42h` (LBA), which addresses the disk as a flat run of sectors and has no such ceiling.

What caps it at 48 now is **memory**, not the disk. The kernel lands at `0x1000` and the loader's own code and stack live at `0x7C00`, so the image must stop short of that: 48 sectors fills `0x1000..0x7000` and leaves 3KB for the stack. Raising it further means loading the kernel above the 1MB line instead, which needs unreal mode or a copy after the switch to protected mode.

Check the commands: `help`, `ls', `cat vsos.conf`, `cat motd.txt `, `cat hostname`, `echo hello`, `uname`, `whoami`, `mkdir`, `touch`, `rm`, `cd`, `vedit`, `sync`, `clear`, `reboot`. Try `mkdir docs && cd docs && touch notes.txt && vedit notes.txt`, then `reboot` and `cat docs/notes.txt` again — it's still there.

## Debugging (if something is hanging on loading)

There are checkpoints in `disk_load.asm`, `boot.asm` and `switch_to_pm.asm`. They print in this order, so the last letter you see is the step it died on:
- `D` — entered `disk_load`
- `L` or `C` — which read path was chosen: `L` is `int 13h ah=42h` (LBA, the normal one), `C` is the old CHS fallback, which can only reach the first ~30 sectors and will likely then fail at `E`
- `A` — all sectors read, or `E` — the BIOS reported a read error
- text `"kernel read OK, entering PM"` — the loader finished and is about to switch modes
- the letters `PM` in the upper-left corner of the screen (yellow) — if you are in protected mode
- the letter `K` next to it is right before the jump into the core

If one of these things does not appear, it means that it is hanging at this step. It is also useful to run with `-no-reboot -no-shutdown` to distinguish a real freeze from a QEMU silently restarting VM with a triple fold:
``
qemu-system-i386 -drive format=raw,file=os-image.bin -drive format=raw,file=fs.img -no-reboot -no-shutdown
```

## Roadmap

- [x] **Stage 1** — bootloader (real mode, BIOS `int 0x10`)
- [x] **Stage 2** — booting kernel.bin from disk (LBA, `int 0x13 ah=42h'), switching to 32-bit protected mode (GDT), transfer of control to the C-core
- [x] **Stage 3** — IDT/interrupts, PIC remap, PS/2 keyboard
- [x] **Stage 4** — unix-style shell (`ls`, `cat`, `echo`, `uname`, `whoami`, `reboot`, `help`, `clear`) + declarative `vsos.conf`, embedded in the kernel via `objcopy' and applied at boot
- [x] **1.2 refresh** — `version` now lives in `vsos.conf` (declarative, not hardcoded), revamped `fetch` (dir/file counts, hostname, cwd), categorized `help`, multi-file `cat`, friendlier prompt (`host:cwd # `) and boot banner
- [x] **Stage 5** — a real filesystem: `fs.c` is now one unified tree where files carry actual content (no more separate `vedit`/`vfs` stubs), backed by a hand-written ATA PIO driver (`ata.c`, primary bus, LBA28). `mkdir`/`touch`/`rm`/`cat`/`cd` all take multi-level paths (`cd docs/notes`, `rm a/b/c.txt`). Every mutation auto-saves to disk (`sync` forces it manually); on boot, `fs_load()` restores the saved tree, or seeds default files on a blank disk. The tree lives on `fs.img`, a *second* virtual disk dedicated to the filesystem (primary IDE slave) - kept separate from the boot disk (`os-image.bin`) on purpose, since the real-mode loader depends on that disk's exact CHS geometry and growing/reusing it for filesystem data broke boot in testing. `make`/`make run` create `fs.img` once and `make clean` leaves it alone, so your files survive rebuilds
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
    ├── ata.h / ata.c        (ATA PIO disk driver)
    ├── fs.h / fs.c          (the filesystem: tree + disk persistence)
    ├── vfs.h / vfs.c        (seeds default files into fs.c on first boot)
    ├── editor.h / editor.c  (vedit, works on fs.c files)
    ├── shell.h / shell.c
    └── string.h / string.c
```

## How everything is connected (download order)
1. The BIOS loads `boot.asm' (sector 1, `0x7C00`), prints a greeting.
2. 'disk_load.asm` reads 60 sectors of kernel.bin into memory `0x1000` via LBA (`int 13h, ah=42h`).
3. `switch_to_pm.asm': `lgdt', PE bit in `cr0', far jump → 32-bit protected mode.
4. `kernel_entry.asm` calls `kernel_main()`(C).
5. `kernel_main`: `config_load()` (parses `vsos.conf`) → `idt_install()' → `pic_remap()` → `keyboard_install()` → `sti` → `fs_init()` → `fs_load()` (restores the saved tree from disk via `ata.c`, or on a blank disk falls through to `vfs_seed()` + `fs_save()` to create the default files) → `shell_init()` (prints motd) → main loop on `keyboard_read_line()' + `shell_execute()'.
# VSOS---Void-Shell-OS
