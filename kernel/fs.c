/* fs.c — unified in-memory directory tree with real file content, backed
 * by a simple flat on-disk layout via ata.c:
 *
 *   LBA FS_DISK_BASE_LBA         : superblock (magic/version/node count)
 *   + 1 .. + FS_META_SECTORS     : packed node metadata (16 entries/sector)
 *   + FS_META_SECTORS+1 .. +N    : one 512-byte sector of content per node
 *
 * Node i's content always lives at a fixed LBA (FS_DATA_START_LBA + i),
 * whether the node is used or not - no free-space bookkeeping needed
 * since FS_FILE_SIZE is exactly one sector.
 */
#include "fs.h"
#include "ata.h"
#include "string.h"

fs_node_t fs_nodes[FS_MAX_NODES];
int fs_current;

/* --- disk layout --- */
#define FS_MAGIC 0x53534F56u /* "VOSS" (little-endian "VSOS"-ish), arbitrary */
/* LBA 0 of fs.img - the dedicated filesystem disk (see ata.h/ata.c: this
 * is the primary IDE *slave*, entirely separate from os-image.bin, the
 * boot disk). No need to leave a gap for boot/kernel sectors here, since
 * this is a different disk than the one they live on. */
#define FS_DISK_BASE_LBA 0u

typedef struct {
    char name[FS_NAME_LEN];
    int is_dir;
    int parent;
    int used;
    unsigned int size;
} fs_disk_meta_t; /* must be exactly 32 bytes: 16 entries pack into one sector */

/* compile-time check: negative array size is a build error if this ever
 * stops being 32 bytes (e.g. FS_NAME_LEN changes) */
typedef char fs_disk_meta_size_check[(sizeof(fs_disk_meta_t) == 32) ? 1 : -1];

#define FS_META_PER_SECTOR 16
#define FS_META_SECTORS ((FS_MAX_NODES + FS_META_PER_SECTOR - 1) / FS_META_PER_SECTOR)
#define FS_DATA_START_LBA (FS_DISK_BASE_LBA + 1 + FS_META_SECTORS)

static void zero_buf(unsigned char *buf, int n) {
    for (int i = 0; i < n; i++) buf[i] = 0;
}

/* --- node helpers --- */

static int find_child(int dir, const char *name) {
    for (int i = 0; i < FS_MAX_NODES; i++) {
        if (fs_nodes[i].used && fs_nodes[i].parent == dir && i != dir &&
            vs_strcmp(fs_nodes[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int alloc_node(void) {
    for (int i = 0; i < FS_MAX_NODES; i++) {
        if (!fs_nodes[i].used) return i;
    }
    return -1;
}

/* resolves one path component ("", ".", "..", or a name) against dir;
 * returns the resulting directory index or -1 if not found */
static int fs_step(int dir, const char *comp, int len) {
    if (len == 0) return dir;
    if (len == 1 && comp[0] == '.') return dir;
    if (len == 2 && comp[0] == '.' && comp[1] == '.') {
        return (dir == 0) ? 0 : fs_nodes[dir].parent;
    }
    for (int i = 0; i < FS_MAX_NODES; i++) {
        if (fs_nodes[i].used && fs_nodes[i].is_dir && fs_nodes[i].parent == dir && i != dir) {
            int j = 0;
            while (j < len && fs_nodes[i].name[j] == comp[j]) j++;
            if (j == len && fs_nodes[i].name[j] == 0) return i;
        }
    }
    return -1;
}

/* fully resolves path to a directory index, walking every component */
static int fs_walk_dir(const char *path) {
    int dir = (path[0] == '/') ? 0 : fs_current;
    if (path[0] == '/') path++;

    while (*path) {
        int len = 0;
        while (path[len] && path[len] != '/') len++;
        dir = fs_step(dir, path, len);
        if (dir < 0) return -1;
        path += len;
        if (*path == '/') path++;
    }
    return dir;
}

/* splits path into (existing) parent dir + final component name, without
 * resolving the final component itself - used by ops that create/find/
 * remove the leaf (mkdir, touch, rm, read, write) */
static int fs_split(const char *path, int *parent_out, char *leaf_out, int leaf_max) {
    int dir = (path[0] == '/') ? 0 : fs_current;
    if (path[0] == '/') path++;

    if (*path == 0) return 0;

    while (1) {
        int len = 0;
        while (path[len] && path[len] != '/') len++;

        if (path[len] == 0) {
            if (len == 0 || len >= leaf_max) return 0;
            for (int i = 0; i < len; i++) leaf_out[i] = path[i];
            leaf_out[len] = 0;
            *parent_out = dir;
            return 1;
        }

        dir = fs_step(dir, path, len);
        if (dir < 0) return 0;
        path += len + 1;
    }
}

/* --- public API --- */

void fs_init(void) {
    for (int i = 0; i < FS_MAX_NODES; i++) fs_nodes[i].used = 0;

    fs_nodes[0].used = 1;
    fs_nodes[0].is_dir = 1;
    fs_nodes[0].parent = 0;
    fs_nodes[0].size = 0;
    fs_nodes[0].content[0] = 0;
    fs_nodes[0].name[0] = '/';
    fs_nodes[0].name[1] = 0;

    fs_current = 0;
}

int fs_mkdir(const char *path) {
    int parent;
    char leaf[FS_NAME_LEN];
    if (!fs_split(path, &parent, leaf, sizeof(leaf))) return 0;
    if (find_child(parent, leaf) >= 0) return 0;

    int idx = alloc_node();
    if (idx < 0) return 0;

    fs_nodes[idx].used = 1;
    fs_nodes[idx].is_dir = 1;
    fs_nodes[idx].parent = parent;
    fs_nodes[idx].size = 0;
    fs_nodes[idx].content[0] = 0;
    int j = 0;
    while (leaf[j] && j < FS_NAME_LEN - 1) { fs_nodes[idx].name[j] = leaf[j]; j++; }
    fs_nodes[idx].name[j] = 0;

    fs_save();
    return 1;
}

int fs_touch(const char *path) {
    int parent;
    char leaf[FS_NAME_LEN];
    if (!fs_split(path, &parent, leaf, sizeof(leaf))) return 0;

    int existing = find_child(parent, leaf);
    if (existing >= 0) {
        return fs_nodes[existing].is_dir ? 0 : 1;
    }

    int idx = alloc_node();
    if (idx < 0) return 0;

    fs_nodes[idx].used = 1;
    fs_nodes[idx].is_dir = 0;
    fs_nodes[idx].parent = parent;
    fs_nodes[idx].size = 0;
    fs_nodes[idx].content[0] = 0;
    int j = 0;
    while (leaf[j] && j < FS_NAME_LEN - 1) { fs_nodes[idx].name[j] = leaf[j]; j++; }
    fs_nodes[idx].name[j] = 0;

    fs_save();
    return 1;
}

int fs_cd(const char *path) {
    if (*path == 0) { fs_current = 0; return 1; }
    int dir = fs_walk_dir(path);
    if (dir < 0) return 0;
    fs_current = dir;
    return 1;
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

int fs_rm(const char *path) {
    int parent;
    char leaf[FS_NAME_LEN];
    if (!fs_split(path, &parent, leaf, sizeof(leaf))) return 0;

    int idx = find_child(parent, leaf);
    if (idx < 0) return 0;

    if (idx == fs_current) return 0;

    if (fs_nodes[idx].is_dir) {
        for (int i = 0; i < FS_MAX_NODES; i++) {
            if (fs_nodes[i].used && fs_nodes[i].parent == idx) return 0; /* not empty */
        }
    }

    fs_nodes[idx].used = 0;
    fs_save();
    return 1;
}

const char *fs_read(const char *path) {
    int parent;
    char leaf[FS_NAME_LEN];
    if (!fs_split(path, &parent, leaf, sizeof(leaf))) return 0;

    int idx = find_child(parent, leaf);
    if (idx < 0 || fs_nodes[idx].is_dir) return 0;
    return fs_nodes[idx].content;
}

int fs_write(const char *path, const char *data, unsigned int len) {
    int parent;
    char leaf[FS_NAME_LEN];
    if (!fs_split(path, &parent, leaf, sizeof(leaf))) return 0;

    int idx = find_child(parent, leaf);
    if (idx < 0) {
        idx = alloc_node();
        if (idx < 0) return 0;
        fs_nodes[idx].used = 1;
        fs_nodes[idx].is_dir = 0;
        fs_nodes[idx].parent = parent;
        int j = 0;
        while (leaf[j] && j < FS_NAME_LEN - 1) { fs_nodes[idx].name[j] = leaf[j]; j++; }
        fs_nodes[idx].name[j] = 0;
    } else if (fs_nodes[idx].is_dir) {
        return 0;
    }

    if (len >= FS_FILE_SIZE) len = FS_FILE_SIZE - 1;
    for (unsigned int i = 0; i < len; i++) fs_nodes[idx].content[i] = data[i];
    fs_nodes[idx].content[len] = 0;
    fs_nodes[idx].size = len;

    fs_save();
    return 1;
}

/* --- disk persistence --- */

int fs_save(void) {
    unsigned char sector[512];

    zero_buf(sector, 512);
    unsigned int *sb = (unsigned int *) sector;
    sb[0] = FS_MAGIC;
    sb[1] = 1;
    sb[2] = FS_MAX_NODES;
    if (!ata_write_sector(FS_DISK_BASE_LBA, sector)) return 0;

    for (int s = 0; s < FS_META_SECTORS; s++) {
        zero_buf(sector, 512);
        fs_disk_meta_t *entries = (fs_disk_meta_t *) sector;
        for (int j = 0; j < FS_META_PER_SECTOR; j++) {
            int idx = s * FS_META_PER_SECTOR + j;
            int k = 0;
            while (fs_nodes[idx].name[k] && k < FS_NAME_LEN - 1) {
                entries[j].name[k] = fs_nodes[idx].name[k];
                k++;
            }
            entries[j].name[k] = 0;
            entries[j].is_dir = fs_nodes[idx].is_dir;
            entries[j].parent = fs_nodes[idx].parent;
            entries[j].used = fs_nodes[idx].used;
            entries[j].size = fs_nodes[idx].size;
        }
        if (!ata_write_sector(FS_DISK_BASE_LBA + 1 + s, sector)) return 0;
    }

    for (int i = 0; i < FS_MAX_NODES; i++) {
        zero_buf(sector, 512);
        unsigned int n = fs_nodes[i].size;
        if (n > FS_FILE_SIZE - 1) n = FS_FILE_SIZE - 1;
        for (unsigned int k = 0; k < n; k++) sector[k] = (unsigned char) fs_nodes[i].content[k];
        if (!ata_write_sector(FS_DATA_START_LBA + i, sector)) return 0;
    }

    return 1;
}

int fs_load(void) {
    unsigned char sector[512];

    if (!ata_read_sector(FS_DISK_BASE_LBA, sector)) return 0;
    unsigned int *sb = (unsigned int *) sector;
    if (sb[0] != FS_MAGIC) return 0;

    for (int s = 0; s < FS_META_SECTORS; s++) {
        if (!ata_read_sector(FS_DISK_BASE_LBA + 1 + s, sector)) return 0;
        fs_disk_meta_t *entries = (fs_disk_meta_t *) sector;
        for (int j = 0; j < FS_META_PER_SECTOR; j++) {
            int idx = s * FS_META_PER_SECTOR + j;
            int k = 0;
            while (entries[j].name[k] && k < FS_NAME_LEN - 1) {
                fs_nodes[idx].name[k] = entries[j].name[k];
                k++;
            }
            fs_nodes[idx].name[k] = 0;
            fs_nodes[idx].is_dir = entries[j].is_dir;
            fs_nodes[idx].parent = entries[j].parent;
            fs_nodes[idx].used = entries[j].used;
            fs_nodes[idx].size = entries[j].size;
        }
    }

    for (int i = 0; i < FS_MAX_NODES; i++) {
        if (!fs_nodes[i].used || fs_nodes[i].is_dir) {
            fs_nodes[i].content[0] = 0;
            continue;
        }
        if (!ata_read_sector(FS_DATA_START_LBA + i, sector)) return 0;
        unsigned int n = fs_nodes[i].size;
        if (n > FS_FILE_SIZE - 1) n = FS_FILE_SIZE - 1;
        for (unsigned int k = 0; k < n; k++) fs_nodes[i].content[k] = (char) sector[k];
        fs_nodes[i].content[n] = 0;
    }

    fs_current = 0;
    return 1;
}
