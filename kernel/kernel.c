/* kernel.c — VSOS kernel core (Stage 5: unix-style shell, declarative
 * config, a real filesystem tree persisted to disk via ata.c) */
#include "screen.h"
#include "idt.h"
#include "pic.h"
#include "keyboard.h"
#include "config.h"
#include "shell.h"
#include "readline.h"
#include "fs.h"
#include "vfs.h"

/* The VSOS wordmark, one glyph per string literal so the letters stay
 * readable here and the spacing can't be miscounted. '#' is drawn as
 * CP437 0xDB, the full block: in VGA text mode that gives solid strokes
 * instead of a hash pattern. Two columns per pixel, because text cells
 * are much taller than they are wide. */
static const char *const logo[] = {
    "\xDB\xDB      \xDB\xDB" "  " "\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB" "  " "\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB" "  " "\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB",
    "\xDB\xDB      \xDB\xDB" "  " "\xDB\xDB        " "  " "\xDB\xDB      \xDB\xDB" "  " "\xDB\xDB        ",
    "\xDB\xDB      \xDB\xDB" "  " "\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB" "  " "\xDB\xDB      \xDB\xDB" "  " "\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB",
    "  \xDB\xDB  \xDB\xDB  " "  " "        \xDB\xDB" "  " "\xDB\xDB      \xDB\xDB" "  " "        \xDB\xDB",
    "    \xDB\xDB    " "  " "\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB" "  " "\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB" "  " "\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB\xDB",
};
#define LOGO_ROWS 5
#define LOGO_WIDTH 46

static void print_banner(void) {
    for (int r = 0; r < LOGO_ROWS; r++) {
        print_string("   ", 0x0f);
        print_string(logo[r], 0x0f);
        print_string("\n", 0x0f);
    }

    print_string("\n   ", 0x0f);
    print_string("Void Shell OS", 0x0b);
    print_string("   v", 0x08);
    print_string(vsos_config.version, 0x0a);
    print_string("   i386", 0x08);
    print_string("\n   ", 0x0f);

    /* CP437 0xC4: a single horizontal rule, cleaner than a row of dashes */
    for (int i = 0; i < LOGO_WIDTH; i++) print_char((char) 0xC4, 0x08);
    print_string("\n\n", 0x0f);
}

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

    print_banner();

    idt_install();
    pic_remap();
    keyboard_install();
    __asm__ volatile ("sti");

    fs_init();
    if (!fs_load()) {
        vfs_seed();
        fs_save();
    }
    shell_init();

    /* completion redraws the prompt after listing its options */
    readline_set_prompt(print_prompt);

    char line[256];
    while (1) {
        print_prompt();
        readline(line, sizeof(line), RL_HISTORY | RL_COMPLETE);
        shell_execute(line);
    }
}
