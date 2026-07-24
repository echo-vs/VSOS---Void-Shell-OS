/* vfs.c — minimal virtual filesystem.
 * Real disk-backed FS is a future stage; for now "files" are backed by
 * kernel state (config) or the raw embedded vsos.conf blob.
 */
#include "vfs.h"
#include "config.h"
#include "string.h"

extern char _binary_vsos_conf_start[];
extern char _binary_vsos_conf_end[];

static char conf_buf[512];

const char *vfs_list[] = { "motd.txt", "hostname", "vsos.conf" };
const int vfs_list_count = 3;

const char *vfs_read(const char *name) {
    if (vs_strcmp(name, "motd.txt") == 0) return vsos_config.motd;
    if (vs_strcmp(name, "hostname") == 0) return vsos_config.hostname;

    if (vs_strcmp(name, "vsos.conf") == 0) {
        int len = _binary_vsos_conf_end - _binary_vsos_conf_start;
        if (len > (int) sizeof(conf_buf) - 1) len = sizeof(conf_buf) - 1;
        for (int i = 0; i < len; i++) conf_buf[i] = _binary_vsos_conf_start[i];
        conf_buf[len] = 0;
        return conf_buf;
    }

    return 0;
}
