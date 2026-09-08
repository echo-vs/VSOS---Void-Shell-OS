#ifndef EDITOR_H
#define EDITOR_H

/* opens (or creates) a file through the fs.c tree and runs the line
 * editor on it; saving writes back through fs_write (and, when a disk is
 * available, persists there too) */
void vedit_run(const char *filename);

#endif
