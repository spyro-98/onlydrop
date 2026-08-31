#ifndef ONLYDROP_PAYLOAD_H
#define ONLYDROP_PAYLOAD_H

#include <stddef.h>

typedef struct {
    unsigned char *data;
    size_t size;
    char *name;
    unsigned char sha256[32];
} onlydrop_payload;

typedef enum {
    ONLYDROP_PAYLOAD_OK = 0,
    ONLYDROP_PAYLOAD_EMPTY,
    ONLYDROP_PAYLOAD_OPEN_ERROR,
    ONLYDROP_PAYLOAD_MEMORY_ERROR,
    ONLYDROP_PAYLOAD_READ_ERROR,
    ONLYDROP_PAYLOAD_HASH_ERROR,
    ONLYDROP_PAYLOAD_TOO_LARGE
} onlydrop_payload_result;

void onlydrop_payload_init(onlydrop_payload *payload);
onlydrop_payload_result onlydrop_payload_load_file(onlydrop_payload *payload, const char *path, const char *name);
onlydrop_payload_result onlydrop_payload_load_stdin(onlydrop_payload *payload, const char *name);
onlydrop_payload_result onlydrop_payload_load_text(onlydrop_payload *payload, const char *text, const char *name);
void onlydrop_payload_free(onlydrop_payload *payload);
void onlydrop_payload_sha256_hex(const onlydrop_payload *payload, char output[65]);
const char *onlydrop_payload_result_message(onlydrop_payload_result result);

#endif
