#ifndef VFS_H
#define VFS_H

extern const char *vfs_list[];
extern const int vfs_list_count;

const char *vfs_read(const char *name);

#endif
