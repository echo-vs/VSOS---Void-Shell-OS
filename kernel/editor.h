#ifndef EDITOR_H
#define EDITOR_H

#define EDITOR_MAX_FILES 4
#define EDITOR_FILE_SIZE 512

typedef struct {
    char name[16];
    char content[EDITOR_FILE_SIZE];
    int used;
} vedit_file_t;

extern vedit_file_t vedit_files[EDITOR_MAX_FILES];

/* opens (or creates) a named in-memory file and runs the line editor on it */
void vedit_run(const char *filename);

/* creates an empty file if it doesn't already exist; 1 = created/exists, 0 = no space */
int vedit_touch(const char *filename);

/* removes a file; 1 = removed, 0 = not found */
int vedit_remove(const char *filename);

/* returns the file's content, or 0 (NULL) if it doesn't exist */
const char *vedit_read(const char *filename);

#endif
