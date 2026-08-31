#ifndef ONLYDROP_LIFECYCLE_H
#define ONLYDROP_LIFECYCLE_H

#include "onlydrop/config.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ONLYDROP_LOADING,
    ONLYDROP_LISTENING,
    ONLYDROP_EXPIRED,
    ONLYDROP_DRAINING,
    ONLYDROP_SHUTTING_DOWN,
    ONLYDROP_CLEANUP
} onlydrop_state;

typedef struct {
    uint64_t expires_at_ns;
    uint32_t max_downloads;
    uint32_t remaining_downloads;
    uint32_t active_transfers;
    onlydrop_expiry_mode expiry_mode;
    onlydrop_state state;
} onlydrop_lifecycle;

void onlydrop_lifecycle_init(onlydrop_lifecycle *lifecycle, const onlydrop_config *config, uint64_t now_ns);
bool onlydrop_lifecycle_can_start(const onlydrop_lifecycle *lifecycle, uint64_t now_ns);
void onlydrop_lifecycle_expire(onlydrop_lifecycle *lifecycle);
void onlydrop_lifecycle_transfer_started(onlydrop_lifecycle *lifecycle);
bool onlydrop_lifecycle_transfer_finished(onlydrop_lifecycle *lifecycle, bool complete);
void onlydrop_lifecycle_transfer_cancelled(onlydrop_lifecycle *lifecycle);
bool onlydrop_lifecycle_should_stop(const onlydrop_lifecycle *lifecycle);

#endif
