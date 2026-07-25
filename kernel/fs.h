#ifndef FS_H
#define FS_H

#define FS_MAX_NODES 32
#define FS_NAME_LEN 16

typedef struct {
    char name[FS_NAME_LEN];
    int is_dir;
    int parent;
    int used;
} fs_node_t;

extern fs_node_t fs_nodes[FS_MAX_NODES];
extern int fs_current;

void fs_init(void);
int fs_mkdir(const char *name);                  /* 1 = ok, 0 = exists/no space */
int fs_cd(const char *name);                       /* 1 = ok, 0 = not found */
void fs_pwd(char *out, int max_len);
int fs_list_children(int *out, int max);            /* returns count */

#endif
