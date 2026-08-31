#ifndef ONLYDROP_SECURITY_H
#define ONLYDROP_SECURITY_H

#include <stddef.h>

int onlydrop_generate_token(char output[65]);
int onlydrop_constant_time_equal(const char *left, const char *right);
int onlydrop_sanitize_filename(const char *input, char *output, size_t output_size);

#endif
