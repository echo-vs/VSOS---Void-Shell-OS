/* editor.c — vedit: a small line editor over fs.c files. Content is kept
 * as one flat, '\n'-separated buffer (the same shape it's stored on disk
 * in); "lines" aren't a separate data structure, just byte ranges found
 * by scanning for '\n', addressed 1-indexed to match how they're shown.
 */
#include "editor.h"
#include "screen.h"
#include "readline.h"
#include "fs.h"
#include "string.h"

/* plain input on purpose: no completion (you're typing prose, not
 * commands) and no history (it belongs to the shell, not to a file) */
static void wait_for_line(char *line, int max_len) {
    readline(line, max_len, 0);
}

/* finds the [start,end) byte range of 1-indexed line n; end sits just
 * past the line's trailing '\n' if it has one, else at len. returns 0 if
 * n is out of range. */
static int find_line(const char *buf, int len, int n, int *start, int *end) {
    if (n < 1) return 0;
    int line = 1;
    int i = 0;
    while (i < len) {
        int s = i;
        while (i < len && buf[i] != '\n') i++;
        int e = (i < len) ? i + 1 : i;
        if (line == n) { *start = s; *end = e; return 1; }
        line++;
        i = e;
    }
    return 0;
}

/* replaces buf[s..e) with ins[0..ins_len), shifting the tail as needed;
 * 0 if the result wouldn't fit in FS_FILE_SIZE */
static int splice(char *buf, int *len, int s, int e, const char *ins, int ins_len) {
    int old_span = e - s;
    int new_len = *len - old_span + ins_len;
    if (new_len >= FS_FILE_SIZE) return 0;

    if (ins_len != old_span) {
        int tail_len = *len - e;
        if (ins_len < old_span) {
            for (int i = 0; i < tail_len; i++) buf[s + ins_len + i] = buf[e + i];
        } else {
            for (int i = tail_len - 1; i >= 0; i--) buf[s + ins_len + i] = buf[e + i];
        }
    }
    for (int i = 0; i < ins_len; i++) buf[s + i] = ins[i];
    *len = new_len;
    buf[*len] = 0;
    return 1;
}

static void print_lines(const char *buf, int len) {
    if (len == 0) {
        print_string("  (empty)\n", 0x08);
        return;
    }
    int n = 1;
    int i = 0;
    while (i < len) {
        int s = i;
        while (i < len && buf[i] != '\n') i++;
        int line_len = i - s;
        if (i < len) i++;

        char numbuf[8];
        vs_itoa((unsigned int) n, numbuf);
        print_string(numbuf, 0x08);
        print_string(": ", 0x08);
        for (int k = 0; k < line_len; k++) print_char(buf[s + k], 0x07);
        print_string("\n", 0x07);
        n++;
    }
}

/* how many lines, and how close the file is to the one-sector ceiling -
 * without this the only warning you get is a line being dropped */
static void print_status(const char *buf, int len) {
    char numbuf[12];
    int lines = 0;
    for (int i = 0; i < len; i++) {
        if (buf[i] == '\n') lines++;
    }
    if (len > 0 && buf[len - 1] != '\n') lines++;

    print_string("  ", 0x08);
    vs_itoa((unsigned int) lines, numbuf);
    print_string(numbuf, 0x08);
    print_string(lines == 1 ? " line, " : " lines, ", 0x08);
    vs_itoa((unsigned int) len, numbuf);
    print_string(numbuf, 0x08);
    print_string("/", 0x08);
    vs_itoa((unsigned int) (FS_FILE_SIZE - 1), numbuf);
    print_string(numbuf, 0x08);
    print_string(" bytes\n", 0x08);
}

static void print_help(void) {
    print_string("  <text>   append a line at the end\n", 0x07);
    print_string("  :p       print the file with line numbers\n", 0x07);
    print_string("  :i N     insert a new line before line N\n", 0x07);
    print_string("  :e N     edit line N (its text is offered back to you)\n", 0x07);
    print_string("  :d N     delete line N\n", 0x07);
    print_string("  :c       clear the whole file\n", 0x07);
    print_string("  :s       save and keep editing\n", 0x07);
    print_string("  :w / --  save and exit\n", 0x07);
    print_string("  :q       exit without saving\n", 0x07);
    print_string("  :help    this message\n", 0x07);
}

void vedit_run(const char *filename) {
    if (!fs_touch(filename)) {
        print_string("vedit: cannot open '", 0x0c);
        print_string(filename, 0x0c);
        print_string("' (it's a directory, bad path, or the tree is full)\n", 0x0c);
        return;
    }

    char buf[FS_FILE_SIZE];
    int len = 0;
    const char *existing = fs_read(filename);
    if (existing) {
        len = (int) vs_strlen(existing);
        if (len > FS_FILE_SIZE - 1) len = FS_FILE_SIZE - 1;
        for (int i = 0; i < len; i++) buf[i] = existing[i];
    }
    buf[len] = 0;

    print_string("vedit: editing '", 0x0f);
    print_string(filename, 0x0f);
    print_string("' - :help for commands\n", 0x0f);
    print_lines(buf, len);
    print_status(buf, len);

    char line[128];
    while (1) {
        print_string("vedit> ", 0x0b);
        wait_for_line(line, sizeof(line));

        if (vs_strcmp(line, "--") == 0 || vs_strcmp(line, ":w") == 0) {
            if (fs_write(filename, buf, (unsigned int) len)) {
                print_string("vedit: saved\n", 0x0a);
            } else {
                print_string("vedit: save failed\n", 0x0c);
            }
            return;
        }

        if (vs_strcmp(line, ":q") == 0) {
            print_string("vedit: quit without saving\n", 0x0e);
            return;
        }

        if (vs_strcmp(line, ":s") == 0) {
            if (fs_write(filename, buf, (unsigned int) len)) {
                print_string("vedit: saved\n", 0x0a);
            } else {
                print_string("vedit: save failed\n", 0x0c);
            }
            continue;
        }

        if (vs_strcmp(line, ":p") == 0) {
            print_lines(buf, len);
            print_status(buf, len);
            continue;
        }

        if (vs_strcmp(line, ":c") == 0) {
            len = 0;
            buf[0] = 0;
            print_string("vedit: cleared\n", 0x0e);
            continue;
        }

        if (vs_strcmp(line, ":help") == 0 || vs_strcmp(line, ":h") == 0) {
            print_help();
            continue;
        }

        if (vs_strncmp(line, ":d ", 3) == 0) {
            int n = (int) vs_atoi(line + 3);
            int s, e;
            if (find_line(buf, len, n, &s, &e)) {
                splice(buf, &len, s, e, "", 0);
                print_string("vedit: deleted line\n", 0x0e);
            } else {
                print_string("vedit: no such line\n", 0x0c);
            }
            continue;
        }

        if (vs_strncmp(line, ":e ", 3) == 0) {
            int n = (int) vs_atoi(line + 3);
            int s, e;
            if (!find_line(buf, len, n, &s, &e)) {
                print_string("vedit: no such line\n", 0x0c);
                continue;
            }

            /* hand the current text back so it can be corrected in place
             * rather than retyped from scratch */
            char current[128];
            int cur_len = e - s;
            if (cur_len > 0 && buf[s + cur_len - 1] == '\n') cur_len--;
            if (cur_len > (int) sizeof(current) - 1) cur_len = (int) sizeof(current) - 1;
            for (int i = 0; i < cur_len; i++) current[i] = buf[s + i];
            current[cur_len] = 0;

            print_string("line ", 0x0b);
            print_string(line + 3, 0x0b);
            print_string("> ", 0x0b);

            char newline[128];
            readline_edit(newline, sizeof(newline), 0, current);
            int nl_len = (int) vs_strlen(newline);

            char ins[130];
            for (int i = 0; i < nl_len; i++) ins[i] = newline[i];
            ins[nl_len] = '\n';

            if (splice(buf, &len, s, e, ins, nl_len + 1)) {
                print_string("vedit: line replaced\n", 0x0e);
            } else {
                print_string("vedit: replacement too big, line unchanged\n", 0x0c);
            }
            continue;
        }

        if (vs_strncmp(line, ":i ", 3) == 0) {
            int n = (int) vs_atoi(line + 3);
            int s, e;
            if (!find_line(buf, len, n, &s, &e)) {
                print_string("vedit: no such line (append with plain text)\n", 0x0c);
                continue;
            }

            print_string("insert> ", 0x0b);
            char newline[128];
            wait_for_line(newline, sizeof(newline));
            int nl_len = (int) vs_strlen(newline);

            char ins[130];
            for (int i = 0; i < nl_len; i++) ins[i] = newline[i];
            ins[nl_len] = '\n';

            /* an empty range at the start of line N: pure insertion */
            if (splice(buf, &len, s, s, ins, nl_len + 1)) {
                print_string("vedit: line inserted\n", 0x0e);
            } else {
                print_string("vedit: file full, nothing inserted\n", 0x0c);
            }
            continue;
        }

        int line_len = (int) vs_strlen(line);
        if (len + line_len + 2 < FS_FILE_SIZE) {
            for (int i = 0; i < line_len; i++) buf[len++] = line[i];
            buf[len++] = '\n';
            buf[len] = 0;
        } else {
            print_string("vedit: file full, line dropped\n", 0x0c);
        }
    }
}
