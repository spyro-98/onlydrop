#include "onlydrop/config.h"

void onlydrop_config_defaults(onlydrop_config *config) {
    *config = (onlydrop_config){
        .expires_seconds = 3600U,
        .downloads = 1U,
        .port = 0U,
        .expiry_mode = ONLYDROP_EXPIRY_GRACEFUL,
        .print_hash = true,
    };
}

const char *onlydrop_expiry_mode_name(onlydrop_expiry_mode mode) {
    return mode == ONLYDROP_EXPIRY_HARD ? "hard" : "graceful";
}
