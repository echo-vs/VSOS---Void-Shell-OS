/* fs.c — minimal in-memory directory tree for mkdir/cd/ls navigation.
 * No real disk filesystem exists yet (Stage 5); this is just a fixed-size
 * node table living in kernel BSS, purely for structure/navigation. Files
 * (vedit, vfs) stay flat/global for now, independent of this tree.
 */
#include "fs.h"
#include "string.h"

fs_node_t fs_nodes[FS_MAX_NODES];
int fs_current;

void fs_init(void) {
    for (int i = 0; i < FS_MAX_NODES; i++) fs_nodes[i].used = 0;

    fs_nodes[0].used = 1;
    fs_nodes[0].is_dir = 1;
    fs_nodes[0].parent = 0;
    fs_nodes[0].name[0] = '/';
    fs_nodes[0].name[1] = 0;

    fs_current = 0;
}

int fs_mkdir(const char *name) {
    for (int i = 0; i < FS_MAX_NODES; i++) {
        if (fs_nodes[i].used && fs_nodes[i].parent == fs_current &&
            vs_strcmp(fs_nodes[i].name, name) == 0) {
            return 0;   /* already exists at this level */
        }
    }
    for (int i = 0; i < FS_MAX_NODES; i++) {
        if (!fs_nodes[i].used) {
            fs_nodes[i].used = 1;
            fs_nodes[i].is_dir = 1;
            fs_nodes[i].parent = fs_current;
            int j = 0;
            while (name[j] && j < FS_NAME_LEN - 1) { fs_nodes[i].name[j] = name[j]; j++; }
            fs_nodes[i].name[j] = 0;
            return 1;
        }
    }
    return 0;   /* out of node slots */
}

int fs_cd(const char *name) {
    if (vs_strcmp(name, "/") == 0) { fs_current = 0; return 1; }
    if (vs_strcmp(name, "..") == 0) {
        if (fs_current != 0) fs_current = fs_nodes[fs_current].parent;
        return 1;
    }
    for (int i = 0; i < FS_MAX_NODES; i++) {
        if (fs_nodes[i].used && fs_nodes[i].is_dir && fs_nodes[i].parent == fs_current &&
            vs_strcmp(fs_nodes[i].name, name) == 0) {
            fs_current = i;
            return 1;
        }
    }
    return 0;
}

void fs_pwd(char *out, int max_len) {
    if (fs_current == 0) {
        out[0] = '/';
        out[1] = 0;
        return;
    }

    int stack[FS_MAX_NODES];
    int depth = 0;
    int idx = fs_current;
    while (idx != 0 && depth < FS_MAX_NODES) {
        stack[depth++] = idx;
        idx = fs_nodes[idx].parent;
    }

    int pos = 0;
    for (int i = depth - 1; i >= 0; i--) {
        if (pos < max_len - 1) out[pos++] = '/';
        int j = 0;
        while (fs_nodes[stack[i]].name[j] && pos < max_len - 1) {
            out[pos++] = fs_nodes[stack[i]].name[j++];
        }
    }
    out[pos] = 0;
}

int fs_list_children(int *out, int max) {
    int count = 0;
    for (int i = 0; i < FS_MAX_NODES && count < max; i++) {
        if (fs_nodes[i].used && fs_nodes[i].parent == fs_current && i != fs_current) {
            out[count++] = i;
        }
    }
    return count;
}
