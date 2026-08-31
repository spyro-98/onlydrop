#include "onlydrop/lifecycle.h"

#include <limits.h>

void onlydrop_lifecycle_init(onlydrop_lifecycle *lifecycle, const onlydrop_config *config, uint64_t now_ns) {
    const uint64_t seconds_ns = config->expires_seconds > UINT64_MAX / UINT64_C(1000000000)
        ? UINT64_MAX : config->expires_seconds * UINT64_C(1000000000);
    lifecycle->expires_at_ns = now_ns > UINT64_MAX - seconds_ns ? UINT64_MAX : now_ns + seconds_ns;
    lifecycle->max_downloads = config->downloads;
    lifecycle->remaining_downloads = config->downloads;
    lifecycle->active_transfers = 0U;
    lifecycle->expiry_mode = config->expiry_mode;
    lifecycle->state = ONLYDROP_LISTENING;
}

bool onlydrop_lifecycle_can_start(const onlydrop_lifecycle *lifecycle, uint64_t now_ns) {
    return lifecycle->state == ONLYDROP_LISTENING && now_ns < lifecycle->expires_at_ns &&
           lifecycle->remaining_downloads > 0U && lifecycle->active_transfers == 0U;
}

void onlydrop_lifecycle_expire(onlydrop_lifecycle *lifecycle) {
    if (lifecycle->expiry_mode == ONLYDROP_EXPIRY_HARD) {
        lifecycle->state = ONLYDROP_CLEANUP;
    } else if (lifecycle->active_transfers == 0U) {
        lifecycle->state = ONLYDROP_CLEANUP;
    } else {
        lifecycle->state = ONLYDROP_DRAINING;
    }
}

void onlydrop_lifecycle_transfer_started(onlydrop_lifecycle *lifecycle) {
    ++lifecycle->active_transfers;
}

bool onlydrop_lifecycle_transfer_finished(onlydrop_lifecycle *lifecycle, bool complete) {
    if (lifecycle->active_transfers > 0U) {
        --lifecycle->active_transfers;
    }
    if (complete && lifecycle->remaining_downloads > 0U) {
        --lifecycle->remaining_downloads;
    }
    if (lifecycle->remaining_downloads == 0U ||
        (lifecycle->state == ONLYDROP_DRAINING && lifecycle->active_transfers == 0U)) {
        lifecycle->state = ONLYDROP_CLEANUP;
    }
    return lifecycle->state == ONLYDROP_CLEANUP;
}

void onlydrop_lifecycle_transfer_cancelled(onlydrop_lifecycle *lifecycle) {
    lifecycle->active_transfers = 0U;
    lifecycle->state = ONLYDROP_CLEANUP;
}

bool onlydrop_lifecycle_should_stop(const onlydrop_lifecycle *lifecycle) {
    return lifecycle->state == ONLYDROP_CLEANUP || lifecycle->state == ONLYDROP_SHUTTING_DOWN;
}
