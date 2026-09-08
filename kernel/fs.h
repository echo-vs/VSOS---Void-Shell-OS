#ifndef FS_H
#define FS_H

/* fs.c — the one real filesystem: an in-memory tree of directories and
 * files (both are "nodes"; a file just carries content) that is loaded
 * from / flushed to disk through ata.c. This replaces the old trio of a
 * dir-only tree (fs.c), a disk-less flat file list (editor.c) and a
 * config-reading stub (vfs.c): files now actually live inside
 * directories, and persist across reboots when a disk is available.
 */

#define FS_MAX_NODES 48
#define FS_NAME_LEN 16
#define FS_FILE_SIZE 512

typedef struct {
    char name[FS_NAME_LEN];
    int is_dir;
    int parent;
    int used;
    unsigned int size;            /* bytes of content in use (files only) */
    char content[FS_FILE_SIZE];   /* files only; NUL-terminated */
} fs_node_t;

extern fs_node_t fs_nodes[FS_MAX_NODES];
extern int fs_current;

void fs_init(void);

/* all paths below resolve relative to fs_current unless they start with
 * '/'; "." and ".." work as path components (not just whole arguments) */
int fs_mkdir(const char *path);                 /* 1 = ok, 0 = exists/bad path/no space */
int fs_touch(const char *path);                 /* create empty file; 1 = ok (no-op if file exists) */
int fs_cd(const char *path);                    /* 1 = ok, 0 = not found */
void fs_pwd(char *out, int max_len);
int fs_list_children(int *out, int max);        /* children of fs_current; returns count */
int fs_rm(const char *path);                    /* remove a file or empty dir; 1 = ok */

const char *fs_read(const char *path);                                /* file content or 0 */
int fs_write(const char *path, const char *data, unsigned int len);   /* replace content, creating the file if needed; 1 = ok */

/* disk persistence (kernel/ata.c). fs_load reads a previously saved tree
 * from disk into fs_nodes (1 = loaded, 0 = no valid filesystem found -
 * caller should seed defaults and fs_save() them). fs_save flushes the
 * whole tree to disk (1 = ok, 0 = disk unavailable - state stays
 * RAM-only, which is safe, just not persistent). */
int fs_load(void);
int fs_save(void);

#endif
