#ifndef READLINE_H
#define READLINE_H

/* Line input with echo, backspace, and optionally history and Tab
 * completion. Runs in normal context (not the keyboard ISR), which is
 * what lets completion consult the shell's command table and the
 * filesystem. Blocks until Enter. */

#define RL_HISTORY  1   /* up/down arrows walk previous lines */
#define RL_COMPLETE 2   /* Tab completes a command or a filename */

void readline(char *out, int max_len, int flags);

/* How to reprint the prompt. Completion needs it: when several matches
 * exist it lists them on a fresh line and then has to redraw the prompt
 * and the half-typed line. Without a hook set, it lists nothing. */
void readline_set_prompt(void (*fn)(void));

#endif
