#ifndef VFS_H
#define VFS_H

/* vfs.c — seeds the root directory with files derived from kernel state
 * (motd.txt, hostname, vsos.conf). Called once, only on a fresh/blank
 * disk (fs_load() found nothing to load) - never overwrites a disk that
 * already has a saved tree, so edits to these files survive reboots. */
void vfs_seed(void);

#endif
