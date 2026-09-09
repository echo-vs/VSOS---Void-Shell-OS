/* readline.c — the line editor behind the shell prompt: echo, backspace,
 * command history on the arrow keys, and Tab completion.
 *
 * This deliberately runs in normal context rather than in the keyboard
 * interrupt handler (which now only queues keys). Completion has to read
 * the shell's command table and walk the filesystem, and the filesystem
 * talks to the disk - none of which belongs in an ISR.
 */
#include "readline.h"
#include "screen.h"
#include "keyboard.h"
#include "shell.h"
#include "fs.h"
#include "string.h"

#define HIST_MAX 8
#define HIST_LEN 128

static char hist[HIST_MAX][HIST_LEN];
static int hist_count = 0;

static void (*prompt_hook)(void) = 0;

void readline_set_prompt(void (*fn)(void)) {
    prompt_hook = fn;
}

static void history_add(const char *line) {
    if (line[0] == 0) return;
    if (hist_count > 0 && vs_strcmp(hist[hist_count - 1], line) == 0) return;

    if (hist_count == HIST_MAX) {
        for (int i = 0; i < HIST_MAX - 1; i++) {
            for (int j = 0; j < HIST_LEN; j++) hist[i][j] = hist[i + 1][j];
        }
        hist_count--;
    }

    int i = 0;
    while (line[i] && i < HIST_LEN - 1) { hist[hist_count][i] = line[i]; i++; }
    hist[hist_count][i] = 0;
    hist_count++;
}

/* wipes what's on screen and types `text` in its place; returns new length */
static int set_line(char *buf, int len, int max_len, const char *text) {
    while (len > 0) { backspace(); len--; }

    int i = 0;
    while (text[i] && i < max_len - 1) {
        buf[i] = text[i];
        print_char(text[i], 0x0f);
        i++;
    }
    buf[i] = 0;
    return i;
}

/* the i-th thing the token could complete to, or 0 past the end:
 * command names for the first word, names in the current directory after */
static const char *candidate(int i, int commands) {
    if (commands) return shell_command_name(i);

    int children[FS_MAX_NODES];
    int n = fs_list_children(children, FS_MAX_NODES);
    if (i < 0 || i >= n) return 0;
    return fs_nodes[children[i]].name;
}

static int complete(char *buf, int len, int max_len) {
    /* the token under completion runs from the last space to the end */
    int start = len;
    while (start > 0 && buf[start - 1] != ' ') start--;

    int commands = 1;
    for (int i = 0; i < start; i++) {
        if (buf[i] != ' ') { commands = 0; break; }
    }
    int tok_len = len - start;

    /* count the matches and reduce them to their common prefix */
    int matches = 0;
    char common[32];
    int common_len = 0;

    for (int i = 0; ; i++) {
        const char *c = candidate(i, commands);
        if (!c) break;
        if (vs_strncmp(c, buf + start, tok_len) != 0) continue;

        if (matches == 0) {
            while (c[common_len] && common_len < (int) sizeof(common) - 1) {
                common[common_len] = c[common_len];
                common_len++;
            }
            common[common_len] = 0;
        } else {
            int j = 0;
            while (j < common_len && c[j] && common[j] == c[j]) j++;
            common_len = j;
            common[common_len] = 0;
        }
        matches++;
    }

    if (matches == 0) return len;

    int added = 0;
    for (int i = tok_len; i < common_len && len < max_len - 1; i++) {
        buf[len++] = common[i];
        print_char(common[i], 0x0f);
        added++;
    }
    buf[len] = 0;

    if (matches == 1) {
        if (len < max_len - 1) {
            buf[len++] = ' ';
            buf[len] = 0;
            print_char(' ', 0x0f);
        }
        return len;
    }

    /* several matches and nothing left to fill in: show the options,
     * then put the prompt and the half-typed line back */
    if (added == 0 && prompt_hook) {
        print_char('\n', 0x0f);
        for (int i = 0; ; i++) {
            const char *c = candidate(i, commands);
            if (!c) break;
            if (vs_strncmp(c, buf + start, tok_len) != 0) continue;
            print_string(c, 0x0b);
            print_string("  ", 0x0f);
        }
        print_char('\n', 0x0f);
        prompt_hook();
        print_string(buf, 0x0f);
    }

    return len;
}

void readline(char *out, int max_len, int flags) {
    readline_edit(out, max_len, flags, 0);
}

void readline_edit(char *out, int max_len, int flags, const char *initial) {
    int len = 0;
    out[0] = 0;

    if (initial) {
        while (initial[len] && len < max_len - 1) {
            out[len] = initial[len];
            print_char(out[len], 0x0f);
            len++;
        }
        out[len] = 0;
    }

    int browse = -1;            /* -1 means "the line I'm typing" */
    char saved[HIST_LEN];
    saved[0] = 0;

    while (1) {
        int k = keyboard_getch();

        if (k == '\n') {
            print_char('\n', 0x0f);
            out[len] = 0;
            if (flags & RL_HISTORY) history_add(out);
            return;
        }

        if (k == '\b') {
            if (len > 0) {
                len--;
                out[len] = 0;
                backspace();
            }
            continue;
        }

        if (k == '\t') {
            if (flags & RL_COMPLETE) len = complete(out, len, max_len);
            continue;
        }

        if (k == KEY_UP || k == KEY_DOWN) {
            if (!(flags & RL_HISTORY) || hist_count == 0) continue;

            if (k == KEY_UP) {
                if (browse + 1 >= hist_count) continue;   /* at the oldest */
                if (browse == -1) {
                    /* keep the unfinished line so DOWN can come back to it */
                    int i = 0;
                    while (out[i] && i < HIST_LEN - 1) { saved[i] = out[i]; i++; }
                    saved[i] = 0;
                }
                browse++;
            } else {
                if (browse < 0) continue;                 /* already back at it */
                browse--;
            }

            const char *text = (browse < 0) ? saved : hist[hist_count - 1 - browse];
            len = set_line(out, len, max_len, text);
            continue;
        }

        if (k == KEY_LEFT || k == KEY_RIGHT) continue;   /* no mid-line editing yet */

        if (k >= 32 && k < 127 && len < max_len - 1) {
            out[len++] = (char) k;
            out[len] = 0;
            print_char((char) k, 0x0f);
        }
    }
}
