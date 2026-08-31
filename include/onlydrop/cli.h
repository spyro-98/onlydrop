#ifndef ONLYDROP_CLI_H
#define ONLYDROP_CLI_H

#include "onlydrop/config.h"

typedef enum {
    ONLYDROP_CLI_OK,
    ONLYDROP_CLI_HELP,
    ONLYDROP_CLI_VERSION,
    ONLYDROP_CLI_ERROR
} onlydrop_cli_result;

onlydrop_cli_result onlydrop_cli_parse(int argc, char **argv, onlydrop_config *config);
void onlydrop_cli_print_usage(const char *program);

#endif
