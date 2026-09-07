/* shell.c — command parser + unix-style builtins */
#include "shell.h"
#include "screen.h"
#include "config.h"
#include "vfs.h"
#include "editor.h"
#include "fs.h"
#include "string.h"
#include "io.h"

static void skip_spaces(const char **s) {
    while (**s == ' ') (*s)++;
}

static void cmd_help(const char *arg) {
    (void) arg;
    print_string("VSOS shell - available commands:\n\n", 0x0f);

    print_string("files & fs\n", 0x0e);
    print_string("  ls               list files and directories\n", 0x07);
    print_string("  cat FILE...      print contents of one or more files\n", 0x07);
    print_string("  mkdir DIR        create a directory (in-memory)\n", 0x07);
    print_string("  cd DIR           change directory (.. or / work too)\n", 0x07);
    print_string("  vedit FILE       minimal in-memory text editor\n\n", 0x07);

    print_string("system\n", 0x0e);
    print_string("  uname            kernel info\n", 0x07);
    print_string("  whoami           current user\n", 0x07);
    print_string("  fetch            show system info\n", 0x07);
    print_string("  clear            clear the screen\n", 0x07);
    print_string("  reboot           restart the machine\n\n", 0x07);

    print_string("shell\n", 0x0e);
    print_string("  echo TEXT        print text\n", 0x07);
    print_string("  help             this message\n", 0x07);
}

static void cmd_ls(const char *arg) {
    (void) arg;
    int printed = 0;

    int children[FS_MAX_NODES];
    int n = fs_list_children(children, FS_MAX_NODES);
    for (int i = 0; i < n; i++) {
        print_string(fs_nodes[children[i]].name, 0x09);
        print_string("/  ", 0x0f);
        printed = 1;
    }

    if (fs_current == 0) {
        for (int i = 0; i < vfs_list_count; i++) {
            print_string(vfs_list[i], 0x0f);
            print_string("  ", 0x0f);
            printed = 1;
        }
        for (int i = 0; i < EDITOR_MAX_FILES; i++) {
            if (vedit_files[i].used) {
                print_string(vedit_files[i].name, 0x0e);
                print_string("  ", 0x0f);
                printed = 1;
            }
        }
    }

    if (!printed) {
        print_string("(empty)", 0x08);
    }
    print_string("\n", 0x0f);
}

static void cmd_mkdir(const char *arg) {
    skip_spaces(&arg);
    if (*arg == 0) {
        print_string("mkdir: missing operand\n", 0x0c);
        return;
    }
    if (!fs_mkdir(arg)) {
        print_string("mkdir: failed (already exists or out of space)\n", 0x0c);
    }
}

static void cmd_cd(const char *arg) {
    skip_spaces(&arg);
    if (*arg == 0) {
        fs_cd("/");
        return;
    }
    if (!fs_cd(arg)) {
        print_string("cd: no such directory\n", 0x0c);
    }
}

static void cmd_fetch(const char *arg) {
    (void) arg;
    char path[64];
    fs_pwd(path, sizeof(path));

    int dirs = 0;
    for (int i = 0; i < FS_MAX_NODES; i++) {
        if (fs_nodes[i].used) dirs++;
    }
    int files = vfs_list_count;
    for (int i = 0; i < EDITOR_MAX_FILES; i++) {
        if (vedit_files[i].used) files++;
    }
    char numbuf[11];

    print_string("   +------------------+\n", 0x0b);
    print_string("   |       VSOS       |\n", 0x0f);
    print_string("   |   Void Shell OS  |\n", 0x0b);
    print_string("   +------------------+\n\n", 0x0b);

    print_string("os     ", 0x09); print_string(": ", 0x08);
    print_string("VSOS ", 0x07); print_string(vsos_config.version, 0x07); print_string(" (Void Shell OS)\n", 0x07);

    print_string("kernel ", 0x0a); print_string(": ", 0x08);
    print_string("custom, i386\n", 0x07);

    print_string("shell  ", 0x0b); print_string(": ", 0x08);
    print_string("vsh (built-in)\n", 0x07);

    print_string("host   ", 0x0d); print_string(": ", 0x08);
    print_string(vsos_config.hostname, 0x07); print_string("\n", 0x07);

    print_string("cwd    ", 0x0e); print_string(": ", 0x08);
    print_string(path, 0x07); print_string("\n", 0x07);

    print_string("dirs   ", 0x0c); print_string(": ", 0x08);
    vs_itoa((unsigned int) dirs, numbuf); print_string(numbuf, 0x07); print_string("\n", 0x07);

    print_string("files  ", 0x09); print_string(": ", 0x08);
    vs_itoa((unsigned int) files, numbuf); print_string(numbuf, 0x07); print_string("\n", 0x07);
}

static void cmd_cat(const char *arg) {
    skip_spaces(&arg);
    if (*arg == 0) {
        print_string("cat: missing file operand\n", 0x0c);
        return;
    }

    while (*arg != 0) {
        int len = 0;
        while (arg[len] && arg[len] != ' ') len++;

        char name[32];
        int n = len < (int) sizeof(name) - 1 ? len : (int) sizeof(name) - 1;
        for (int i = 0; i < n; i++) name[i] = arg[i];
        name[n] = 0;

        const char *content = vedit_read(name);
        if (!content) content = vfs_read(name);
        if (!content) {
            print_string("cat: ", 0x0c);
            print_string(name, 0x0c);
            print_string(": no such file\n", 0x0c);
        } else {
            print_string(content, 0x07);
            print_string("\n", 0x07);
        }

        arg += len;
        skip_spaces(&arg);
    }
}

static void cmd_echo(const char *arg) {
    skip_spaces(&arg);
    print_string(arg, 0x0f);
    print_string("\n", 0x0f);
}

static void cmd_uname(const char *arg) {
    (void) arg;
    print_string("VSOS ", 0x0f);
    print_string(vsos_config.version, 0x0f);
    print_string(" (Void Shell OS) i386\n", 0x0f);
}

static void cmd_whoami(const char *arg) {
    (void) arg;
    print_string("root\n", 0x0f);
}

static void cmd_reboot(const char *arg) {
    (void) arg;
    print_string("rebooting...\n", 0x0c);
    unsigned char status;
    do {
        status = inb(0x64);
    } while (status & 0x02);
    outb(0x64, 0xFE);
    for (;;) { __asm__ volatile ("hlt"); }
}

static void cmd_clear(const char *arg) {
    (void) arg;
    clear_screen();
}

static void cmd_vedit(const char *arg) {
    skip_spaces(&arg);
    if (*arg == 0) {
        print_string("vedit: usage: vedit <filename>\n", 0x0c);
        return;
    }
    vedit_run(arg);
}

typedef struct {
    const char *name;
    void (*fn)(const char *arg);
} shell_cmd_t;

static const shell_cmd_t commands[] = {
    { "help",   cmd_help },
    { "ls",     cmd_ls },
    { "cat",    cmd_cat },
    { "echo",   cmd_echo },
    { "clear",  cmd_clear },
    { "uname",  cmd_uname },
    { "whoami", cmd_whoami },
    { "reboot", cmd_reboot },
    { "vedit",  cmd_vedit },
    { "mkdir",  cmd_mkdir },
    { "cd",     cmd_cd },
    { "fetch",  cmd_fetch },
};
static const int command_count = sizeof(commands) / sizeof(commands[0]);

void shell_init(void) {
    print_string(vsos_config.motd, 0x0a);
    print_string("\n\n", 0x0f);
}

void shell_execute(const char *line) {
    skip_spaces(&line);
    if (*line == 0) return;

    int name_len = 0;
    while (line[name_len] && line[name_len] != ' ') name_len++;

    for (int c = 0; c < command_count; c++) {
        if ((int) vs_strlen(commands[c].name) == name_len &&
            vs_strncmp(line, commands[c].name, name_len) == 0) {
            commands[c].fn(line + name_len);
            return;
        }
    }

    print_string(line, 0x0c);
    print_string(": command not found\n", 0x0c);
}
