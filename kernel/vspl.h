#ifndef VSPL_H
#define VSPL_H

/* VSPL — Void Shell Programming Language.
 *
 * A script is a file of ordinary VSOS commands, plus variables (`set`),
 * conditions (`if`/`else`/`end`) and loops (`while`/`end`). Anything the
 * interpreter doesn't recognise as one of its own keywords is handed to
 * shell_execute(), so every command the shell already has works inside a
 * script for free - that, rather than a built-in library, is where the
 * language gets its vocabulary.
 *
 * Values are strings; arithmetic and comparisons read them as numbers
 * when both sides look like numbers. Esc aborts a running script, which
 * matters because there is no preemption to rescue an endless loop.
 */
void vspl_run_file(const char *path);

#endif
