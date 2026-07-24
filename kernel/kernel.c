/* kernel.c — VSOS kernel core (Stage 4: unix-style shell + declarative config) */
#include "screen.h"
#include "idt.h"
#include "pic.h"
#include "keyboard.h"
#include "config.h"
#include "shell.h"

static void print_prompt(void) {
    print_string(vsos_config.hostname, 0x0b);
    print_string("> ", 0x0b);
}

void kernel_main(void) {
    clear_screen();
    print_string("VSOS - Void Shell OS\n", 0x0f);
    print_string("kernel loaded : stage 4 (shell + declarative config)\n\n", 0x0a);

    idt_install();
    pic_remap();
    keyboard_install();
    __asm__ volatile ("sti");

    config_load();
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
