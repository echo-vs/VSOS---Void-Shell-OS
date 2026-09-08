#ifndef SHELL_H
#define SHELL_H

void shell_init(void);
void shell_execute(const char *line);

/* The index-th command name, for Tab completion; 0 once the list runs
 * out. Commands marked hidden are skipped, so completion won't give away
 * the ones that aren't in `help` either. */
const char *shell_command_name(int index);

#endif
