#ifndef ONLYDROP_PLATFORM_H
#define ONLYDROP_PLATFORM_H

#include <stdint.h>
#include <stddef.h>

uint64_t onlydrop_monotonic_ns(void);
int onlydrop_stdin_is_interactive(void);
int onlydrop_choose_public_ipv4(const char *requested_bind, char *output, size_t output_size);

#endif
