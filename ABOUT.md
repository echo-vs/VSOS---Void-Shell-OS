# VSOS — Void Shell OS

**An independent operating system for x86.**
**Open-source.** Its own kernel, its own bootloader, its own shell — without Linux under the hood.

---

## What is it

VSOS is an operating system written from scratch in **C and x86 Assembly (NASM)**.
Without a ready-made kernel, without POSIX wrappers on top of someone else's code — bootloader, switching to protected mode, interrupt handling, keyboard driver and shell are hand-written and tested in QEMU.

## Philosophy

- **Unix style on the surface** — familiar commands (`ls`, `cat`, `echo`, `uname`), familiar logic, nothing needs to be reinvented in your head
- **Declarativeness under the hood** — the state of the system is described in the config (`vsos.conf`), and not hardcoded in the code. It's like Nix: you describe what should be, and the system applies it.
- **Without magic** — every line at a low level is written consciously and understandable to the author. Nothing "just works" because someone else did it 20 years ago.

## Status

, Active development. The kernel is loading, the shell is running, interrupts and the keyboard are.
Then there's the real file system, the batch manager, and the network.

## Stack
`x86 Assembly (NASM)` · `C` · `QEMU' (test environment) · without external runtime dependencies

## Authors
Created by echo

## License
Open-source — **MIT**
