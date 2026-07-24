#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
    char hostname[32];
    char motd[128];
} vsos_config_t;

extern vsos_config_t vsos_config;

void config_load(void);

#endif
