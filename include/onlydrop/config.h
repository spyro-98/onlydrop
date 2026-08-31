#ifndef ONLYDROP_CONFIG_H
#define ONLYDROP_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ONLYDROP_EXPIRY_GRACEFUL,
    ONLYDROP_EXPIRY_HARD
} onlydrop_expiry_mode;

typedef struct {
    const char *input_path;
    const char *text;
    const char *name;
    const char *bind_address;
    uint64_t expires_seconds;
    uint32_t downloads;
    uint16_t port;
    onlydrop_expiry_mode expiry_mode;
    bool use_https;
    bool qr;
    bool print_hash;
    bool quiet;
    bool json;
} onlydrop_config;

void onlydrop_config_defaults(onlydrop_config *config);
const char *onlydrop_expiry_mode_name(onlydrop_expiry_mode mode);

#endif
