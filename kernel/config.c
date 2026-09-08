/* config.c — declarative config: parse vsos.conf (embedded at link time)
 * and apply it as kernel state (hostname, motd, ...).
 */
#include "config.h"
#include "string.h"

extern char _binary_vsos_conf_start[];
extern char _binary_vsos_conf_end[];

vsos_config_t vsos_config = {
    .hostname = "vsos",
    .motd = "Welcome to VSOS",
    .version = "1.3",
};

static void trim_copy(char *dst, const char *src, const char *line_end, int max_len) {
    while (src < line_end && (*src == ' ' || *src == '"')) src++;
    int i = 0;
    while (src < line_end && *src != '"' && i < max_len - 1) {
        dst[i++] = *src++;
    }
    dst[i] = 0;
}

void config_load(void) {
    char *p = _binary_vsos_conf_start;
    char *end = _binary_vsos_conf_end;

    while (p < end) {
        char *line_start = p;
        while (p < end && *p != '\n') p++;
        char *line_end = p;
        if (p < end) p++;

        while (line_start < line_end && (*line_start == ' ' || *line_start == '\t')) line_start++;

        if (line_start >= line_end || *line_start == '#') continue;

        char *eq = line_start;
        while (eq < line_end && *eq != '=') eq++;
        if (eq >= line_end) continue;

        int key_len = eq - line_start;
        while (key_len > 0 && line_start[key_len - 1] == ' ') key_len--;

        char *value = eq + 1;

        if (key_len == 8 && vs_strncmp(line_start, "hostname", 8) == 0) {
            trim_copy(vsos_config.hostname, value, line_end, sizeof(vsos_config.hostname));
        } else if (key_len == 4 && vs_strncmp(line_start, "motd", 4) == 0) {
            trim_copy(vsos_config.motd, value, line_end, sizeof(vsos_config.motd));
        } else if (key_len == 7 && vs_strncmp(line_start, "version", 7) == 0) {
            trim_copy(vsos_config.version, value, line_end, sizeof(vsos_config.version));
        }
    }
}
