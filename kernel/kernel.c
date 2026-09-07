/* kernel.c — VSOS kernel core (Stage 4+: unix-style shell, declarative
 * config, in-memory directory navigation) */
#include "screen.h"
#include "idt.h"
#include "pic.h"
#include "keyboard.h"
#include "config.h"
#include "shell.h"
#include "fs.h"

static void print_prompt(void) {
    char path[64];
    fs_pwd(path, sizeof(path));
    print_string(vsos_config.hostname, 0x0b);
    print_string(":", 0x08);
    print_string(path, 0x0f);
    print_string(" # ", 0x0a);
}

void kernel_main(void) {
    clear_screen();
    config_load();

    print_string("========================================\n", 0x0b);
    print_string(" VSOS - Void Shell OS\n", 0x0f);
    print_string("========================================\n", 0x0b);
    print_string("kernel loaded : v", 0x0a);
    print_string(vsos_config.version, 0x0a);
    print_string(" (shell + declarative config + fs nav + refreshed utils)\n\n", 0x0a);

    idt_install();
    pic_remap();
    keyboard_install();
    __asm__ volatile ("sti");

    fs_init();
    shell_init();
    print_prompt();

    char line[256];
    while (1) {
        if (keyboard_read_line(line, sizeof(line))) {
            shell_execute(line);
            print_prompt();
        }
        __asm__ volatile ("hlt");
    }
}
