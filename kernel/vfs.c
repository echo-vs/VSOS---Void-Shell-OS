/* vfs.c — first-boot seed: turns kernel config state into real files in
 * the fs.c tree (motd.txt, hostname, vsos.conf). Only ever called once,
 * right after fs_init(), when fs_load() reports no saved filesystem on
 * disk yet.
 */
#include "vfs.h"
#include "fs.h"
#include "config.h"
#include "string.h"

extern char _binary_vsos_conf_start[];
extern char _binary_vsos_conf_end[];

void vfs_seed(void) {
    fs_write("motd.txt", vsos_config.motd, vs_strlen(vsos_config.motd));
    fs_write("hostname", vsos_config.hostname, vs_strlen(vsos_config.hostname));

    unsigned int len = (unsigned int) (_binary_vsos_conf_end - _binary_vsos_conf_start);
    fs_write("vsos.conf", _binary_vsos_conf_start, len);
}
