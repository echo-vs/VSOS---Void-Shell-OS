/* editor.c — vedit: minimal in-memory line editor.
 * No disk filesystem exists yet (that's Stage 5), so "files" here live in a
 * small fixed set of RAM buffers. Once real disk I/O exists, vedit_run can
 * flush to disk instead without the shell-facing API changing.
 */
#include "editor.h"
#include "screen.h"
#include "keyboard.h"
#include "string.h"

vedit_file_t vedit_files[EDITOR_MAX_FILES];

static vedit_file_t *find_file(const char *name) {
    for (int i = 0; i < EDITOR_MAX_FILES; i++) {
        if (vedit_files[i].used && vs_strcmp(vedit_files[i].name, name) == 0) {
            return &vedit_files[i];
        }
    }
    return 0;
}

static vedit_file_t *find_or_create(const char *name) {
    vedit_file_t *existing = find_file(name);
    if (existing) return existing;

    for (int i = 0; i < EDITOR_MAX_FILES; i++) {
        if (!vedit_files[i].used) {
            vedit_files[i].used = 1;
            int j = 0;
            while (name[j] && j < 15) { vedit_files[i].name[j] = name[j]; j++; }
            vedit_files[i].name[j] = 0;
            vedit_files[i].content[0] = 0;
            return &vedit_files[i];
        }
    }
    return 0;   /* out of slots */
}

const char *vedit_read(const char *name) {
    vedit_file_t *f = find_file(name);
    return f ? f->content : 0;
}

int vedit_touch(const char *name) {
    return find_or_create(name) ? 1 : 0;
}

int vedit_remove(const char *name) {
    vedit_file_t *f = find_file(name);
    if (!f) return 0;
    f->used = 0;
    f->name[0] = 0;
    f->content[0] = 0;
    return 1;
}

static int wait_for_line(char *line, int max_len) {
    int got = 0;
    do {
        got = keyboard_read_line(line, max_len);
        if (!got) { __asm__ volatile ("hlt"); }
    } while (!got);
    return got;
}

void vedit_run(const char *filename) {
    vedit_file_t *f = find_or_create(filename);
    if (!f) {
        print_string("vedit: no free file slots\n", 0x0c);
        return;
    }

    print_string("vedit: editing '", 0x0f);
    print_string(filename, 0x0f);
    print_string("' - type a line with just -- to save and exit\n", 0x0f);

    int len = vs_strlen(f->content);

    char line[128];
    while (1) {
        print_string("vedit> ", 0x0b);
        wait_for_line(line, sizeof(line));

        if (vs_strcmp(line, "--") == 0) break;

        int line_len = vs_strlen(line);
        if (len + line_len + 2 < EDITOR_FILE_SIZE) {
            for (int i = 0; i < line_len; i++) f->content[len++] = line[i];
            f->content[len++] = '\n';
            f->content[len] = 0;
        } else {
            print_string("vedit: file full, line dropped\n", 0x0c);
        }
    }

    print_string("vedit: saved\n", 0x0a);
}
