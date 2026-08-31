#ifndef ONLYDROP_PROTOCOL_H
#define ONLYDROP_PROTOCOL_H

#include "onlydrop/config.h"
#include "onlydrop/lifecycle.h"
#include "onlydrop/payload.h"

typedef struct {
    const onlydrop_config *config;
    const onlydrop_payload *payload;
    onlydrop_lifecycle *lifecycle;
    const char *token;
} onlydrop_server;

int onlydrop_server_run(onlydrop_server *server);

#endif
