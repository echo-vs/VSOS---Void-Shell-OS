/* vspl.c — the VSPL interpreter. See vspl.h for what the language is.
 *
 * Execution model: the script is copied into a local buffer and split
 * into NUL-terminated lines, then walked with a program counter. Blocks
 * are tracked on a small stack: `end` either falls through (if) or jumps
 * back to re-test the condition (while). Nothing recurses, so a deeply
 * nested script costs a stack slot rather than a C stack frame.
 *
 * Every line is variable-expanded before it is looked at, so `$x` works
 * uniformly in conditions, in `set`, and in the arguments of any shell
 * command - one rule instead of three.
 */
#include "vspl.h"
#include "shell.h"
#include "screen.h"
#include "keyboard.h"
#include "fs.h"
#include "string.h"

#define MAX_VARS 16
#define NAME_LEN 12
#define VAL_LEN 48
#define MAX_LINES 64
#define MAX_DEPTH 8
#define LINE_LEN 256    /* same as the shell's own line buffer */
#define MAX_NESTED_RUNS 4

#define BLK_IF 0
#define BLK_WHILE 1

typedef struct {
    char name[NAME_LEN];
    char value[VAL_LEN];
    int used;
} var_t;

/* variables outlive a single script on purpose, so one script can leave
 * state for the next, the way exported shell variables do. They are not
 * reachable from the interactive prompt: `$` expansion happens here, not
 * in shell_execute. */
static var_t vars[MAX_VARS];
static int run_depth = 0;
static char bad_name[NAME_LEN];

/* --- small helpers --- */

static int is_space(char c) { return c == ' ' || c == '\t'; }

static int is_name_start(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static int is_name_char(char c) {
    return is_name_start(c) || (c >= '0' && c <= '9');
}

static void skip_ws(const char **p) { while (is_space(**p)) (*p)++; }

static int token(const char **p, char *out, int max) {
    skip_ws(p);
    if (**p == 0) { out[0] = 0; return 0; }
    int i = 0;
    while (**p && !is_space(**p)) {
        if (i < max - 1) out[i++] = **p;
        (*p)++;
    }
    out[i] = 0;
    return 1;
}

/* strict: only a token that is entirely a number counts as one */
static int parse_int(const char *s, int *out) {
    int i = 0, sign = 1;
    if (s[0] == '-') { sign = -1; i = 1; }
    if (!s[i]) return 0;
    int v = 0;
    for (; s[i]; i++) {
        if (s[i] < '0' || s[i] > '9') return 0;
        v = v * 10 + (s[i] - '0');
    }
    *out = v * sign;
    return 1;
}

static void int_to_str(int v, char *out) {
    if (v < 0) {
        out[0] = '-';
        vs_itoa((unsigned int) (-v), out + 1);
    } else {
        vs_itoa((unsigned int) v, out);
    }
}

static void say(const char *msg, unsigned char color) {
    print_string(msg, color);
}

static void err_at(int line_no, const char *msg, const char *detail) {
    char n[12];
    vs_itoa((unsigned int) (line_no + 1), n);
    say("vspl: line ", 0x0c);
    say(n, 0x0c);
    say(": ", 0x0c);
    say(msg, 0x0c);
    if (detail && detail[0]) {
        say(" '", 0x0c);
        say(detail, 0x0c);
        say("'", 0x0c);
    }
    say("\n", 0x0c);
}

/* --- variables --- */

static var_t *find_var(const char *name) {
    for (int i = 0; i < MAX_VARS; i++) {
        if (vars[i].used && vs_strcmp(vars[i].name, name) == 0) return &vars[i];
    }
    return 0;
}

static int set_var(const char *name, const char *value) {
    var_t *v = find_var(name);
    if (!v) {
        for (int i = 0; i < MAX_VARS; i++) {
            if (!vars[i].used) { v = &vars[i]; break; }
        }
        if (!v) return 0;                     /* table full */
        v->used = 1;
        int i = 0;
        while (name[i] && i < NAME_LEN - 1) { v->name[i] = name[i]; i++; }
        v->name[i] = 0;
    }
    int i = 0;
    while (value[i] && i < VAL_LEN - 1) { v->value[i] = value[i]; i++; }
    v->value[i] = 0;
    return 1;
}

/* Substitutes $name. An unknown name is an error rather than an empty
 * string: silently expanding a typo to nothing produces scripts that
 * misbehave without saying why. */
static int expand(const char *in, char *out, int max) {
    int o = 0;
    int i = 0;

    while (in[i] && o < max - 1) {
        if (in[i] == '$' && is_name_start(in[i + 1])) {
            char name[NAME_LEN];
            int n = 0;
            i++;
            while (in[i] && is_name_char(in[i])) {
                if (n < NAME_LEN - 1) name[n++] = in[i];
                i++;
            }
            name[n] = 0;

            var_t *v = find_var(name);
            if (!v) {
                int k = 0;
                while (name[k] && k < NAME_LEN - 1) { bad_name[k] = name[k]; k++; }
                bad_name[k] = 0;
                return 0;
            }
            for (int k = 0; v->value[k] && o < max - 1; k++) out[o++] = v->value[k];
        } else {
            out[o++] = in[i++];
        }
    }
    out[o] = 0;
    return 1;
}

/* --- statements --- */

static int first_word_is(const char *line, const char *kw) {
    const char *p = line;
    char t[16];
    if (!token(&p, t, sizeof(t))) return 0;
    return vs_strcmp(t, kw) == 0;
}

/* Finds the line closing the block that opens at `from`. With want_else,
 * a matching `else` counts as a stopping point too. Scans raw lines,
 * which is safe: keywords never come out of a variable. */
static int match_end(char *src, int *off, int nlines, int from, int want_else) {
    int depth = 0;
    for (int i = from; i < nlines; i++) {
        const char *l = &src[off[i]];
        if (first_word_is(l, "if") || first_word_is(l, "while")) {
            depth++;
        } else if (first_word_is(l, "end")) {
            depth--;
            if (depth == 0) return i;
        } else if (want_else && depth == 1 && first_word_is(l, "else")) {
            return i;
        }
    }
    return nlines;   /* unbalanced */
}

/* "A op B" -> 1/0 in *out; 0 if it isn't a condition we understand */
static int eval_cond(const char *p, int *out) {
    char a[VAL_LEN], op[4], b[VAL_LEN];
    if (!token(&p, a, sizeof(a))) return 0;
    if (!token(&p, op, sizeof(op))) return 0;
    if (!token(&p, b, sizeof(b))) return 0;

    int ai = 0, bi = 0;
    int numeric = parse_int(a, &ai) && parse_int(b, &bi);

    if (vs_strcmp(op, "==") == 0) {
        *out = numeric ? (ai == bi) : (vs_strcmp(a, b) == 0);
        return 1;
    }
    if (vs_strcmp(op, "!=") == 0) {
        *out = numeric ? (ai != bi) : (vs_strcmp(a, b) != 0);
        return 1;
    }

    if (!numeric) return 0;      /* ordering only makes sense on numbers */

    if (vs_strcmp(op, "<") == 0)       *out = ai < bi;
    else if (vs_strcmp(op, ">") == 0)  *out = ai > bi;
    else if (vs_strcmp(op, "<=") == 0) *out = ai <= bi;
    else if (vs_strcmp(op, ">=") == 0) *out = ai >= bi;
    else return 0;

    return 1;
}

/* `set NAME rest`: "A op B" is arithmetic, anything else is stored as text */
static int do_set(const char *p, int line_no) {
    char name[NAME_LEN];
    if (!token(&p, name, sizeof(name))) {
        err_at(line_no, "set needs a variable name", 0);
        return 0;
    }

    const char *rest = p;
    char a[VAL_LEN], op[4], b[VAL_LEN], extra[4];

    if (token(&p, a, sizeof(a)) && token(&p, op, sizeof(op)) &&
        token(&p, b, sizeof(b)) && !token(&p, extra, sizeof(extra))) {

        int ai, bi;
        if (parse_int(a, &ai) && parse_int(b, &bi) && op[1] == 0) {
            int r;
            if (op[0] == '+') r = ai + bi;
            else if (op[0] == '-') r = ai - bi;
            else if (op[0] == '*') r = ai * bi;
            else if (op[0] == '/') {
                if (bi == 0) { err_at(line_no, "division by zero", 0); return 0; }
                r = ai / bi;
            } else goto as_text;

            char buf[16];
            int_to_str(r, buf);
            if (!set_var(name, buf)) { err_at(line_no, "too many variables", name); return 0; }
            return 1;
        }
    }

as_text:
    skip_ws(&rest);
    if (!set_var(name, rest)) {
        err_at(line_no, "too many variables", name);
        return 0;
    }
    return 1;
}

void vspl_run_file(const char *path) {
    const char *content = fs_read(path);
    if (!content) {
        say("vspl: no such script: ", 0x0c);
        say(path, 0x0c);
        say("\n", 0x0c);
        return;
    }

    if (run_depth >= MAX_NESTED_RUNS) {
        say("vspl: scripts nested too deep\n", 0x0c);
        return;
    }
    run_depth++;

    /* a private copy: the script may well go on to edit files, and the
     * node it came from must not be read while that happens */
    char src[FS_FILE_SIZE];
    int off[MAX_LINES];
    int nlines = 0;

    int len = 0;
    while (content[len] && len < FS_FILE_SIZE - 1) { src[len] = content[len]; len++; }
    src[len] = 0;

    int start = 0;
    for (int i = 0; i <= len; i++) {
        if (src[i] == '\n' || src[i] == 0) {
            int last = (src[i] == 0);
            src[i] = 0;
            if (nlines < MAX_LINES) off[nlines++] = start;
            start = i + 1;
            if (last) break;
        }
    }

    int blk_kind[MAX_DEPTH];
    int blk_line[MAX_DEPTH];
    int top = 0;
    int pc = 0;

    while (pc < nlines) {
        /* the only way out of a runaway loop: there is no preemption and
         * no way to interrupt a script from outside it */
        if (keyboard_poll() == 27) {
            say("vspl: stopped\n", 0x0e);
            break;
        }

        const char *raw = &src[off[pc]];
        const char *probe = raw;
        char first[16];
        if (!token(&probe, first, sizeof(first)) || first[0] == '#') {
            pc++;                       /* blank line or comment */
            continue;
        }

        char line[LINE_LEN];
        if (!expand(raw, line, sizeof(line))) {
            err_at(pc, "unknown variable", bad_name);
            break;
        }

        const char *p = line;
        char cmd[16];
        token(&p, cmd, sizeof(cmd));

        if (vs_strcmp(cmd, "set") == 0) {
            if (!do_set(p, pc)) break;
            pc++;

        } else if (vs_strcmp(cmd, "if") == 0) {
            int cond;
            if (!eval_cond(p, &cond)) { err_at(pc, "bad condition", 0); break; }

            if (cond) {
                if (top >= MAX_DEPTH) { err_at(pc, "blocks nested too deep", 0); break; }
                blk_kind[top] = BLK_IF;
                blk_line[top] = pc;
                top++;
                pc++;
            } else {
                int target = match_end(src, off, nlines, pc, 1);
                if (target >= nlines) { err_at(pc, "if without end", 0); break; }

                if (first_word_is(&src[off[target]], "else")) {
                    if (top >= MAX_DEPTH) { err_at(pc, "blocks nested too deep", 0); break; }
                    blk_kind[top] = BLK_IF;
                    blk_line[top] = pc;
                    top++;
                    pc = target + 1;    /* run the else branch */
                } else {
                    pc = target + 1;
                }
            }

        } else if (vs_strcmp(cmd, "else") == 0) {
            /* reached by falling off the end of a taken branch */
            if (top == 0) { err_at(pc, "else without if", 0); break; }
            int target = match_end(src, off, nlines, blk_line[top - 1], 0);
            top--;
            if (target >= nlines) { err_at(pc, "if without end", 0); break; }
            pc = target + 1;

        } else if (vs_strcmp(cmd, "while") == 0) {
            int cond;
            if (!eval_cond(p, &cond)) { err_at(pc, "bad condition", 0); break; }

            if (cond) {
                if (top >= MAX_DEPTH) { err_at(pc, "blocks nested too deep", 0); break; }
                blk_kind[top] = BLK_WHILE;
                blk_line[top] = pc;
                top++;
                pc++;
            } else {
                int target = match_end(src, off, nlines, pc, 0);
                if (target >= nlines) { err_at(pc, "while without end", 0); break; }
                pc = target + 1;
            }

        } else if (vs_strcmp(cmd, "end") == 0) {
            if (top == 0) { err_at(pc, "end without if or while", 0); break; }
            top--;
            if (blk_kind[top] == BLK_WHILE) pc = blk_line[top];   /* re-test */
            else pc++;

        } else {
            /* not ours: the shell knows every other command there is */
            shell_execute(line);
            pc++;
        }
    }

    run_depth--;
}
