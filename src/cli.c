#include "onlydrop/cli.h"

#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_duration(const char *value, uint64_t *seconds) {
    uint64_t total = 0U;
    const char *cursor = value;
    while (*cursor != '\0') {
        char *end = NULL;
        unsigned long long number;
        uint64_t multiplier;
        errno = 0;
        number = strtoull(cursor, &end, 10);
        if (errno != 0 || end == cursor || number == 0U) {
            return -1;
        }
        switch (*end) {
            case 's': multiplier = 1U; break;
            case 'm': multiplier = 60U; break;
            case 'h': multiplier = 3600U; break;
            default: return -1;
        }
        if ((uint64_t)number > UINT64_MAX / multiplier || total > UINT64_MAX - (uint64_t)number * multiplier) {
            return -1;
        }
        total += (uint64_t)number * multiplier;
        cursor = end + 1;
    }
    *seconds = total;
    return total == 0U ? -1 : 0;
}

static int parse_u32(const char *value, uint32_t *output) {
    char *end = NULL;
    unsigned long parsed;
    errno = 0;
    parsed = strtoul(value, &end, 10);
    if (errno != 0 || end == value || *end != '\0' || parsed == 0UL || parsed > UINT32_MAX) {
        return -1;
    }
    *output = (uint32_t)parsed;
    return 0;
}

static int parse_port(const char *value, uint16_t *output) {
    uint32_t parsed;
    if (parse_u32(value, &parsed) != 0 || parsed > UINT16_MAX) {
        return -1;
    }
    *output = (uint16_t)parsed;
    return 0;
}

void onlydrop_cli_print_usage(const char *program) {
    (void)fprintf(stderr,
        "Usage: %s [FILE] [options]\n"
        "  -e, --expires DURATION       Expiry window (e.g. 30s, 10m, 2h30m)\n"
        "  -n, --downloads NUMBER       Completed downloads allowed\n"
        "      --expiry-mode MODE       graceful (default) or hard\n"
        "      --http | --https         Select protocol (HTTP is default)\n"
        "      --bind ADDRESS            Listener address\n"
        "      --port PORT               Listener port (0 selects one)\n"
        "      --name NAME               Download file name\n"
        "      --text TEXT               Use UTF-8 text as the payload\n"
        "      --qr | --no-qr            Control terminal QR rendering\n"
        "      --hash | --no-hash        Control SHA-256 output\n"
        "      --quiet | --json          Human or structured output\n"
        "      --version | --help\n", program);
}

onlydrop_cli_result onlydrop_cli_parse(int argc, char **argv, onlydrop_config *config) {
    static const struct option options[] = {
        {"expires", required_argument, NULL, 'e'}, {"downloads", required_argument, NULL, 'n'},
        {"expiry-mode", required_argument, NULL, 1}, {"http", no_argument, NULL, 2},
        {"https", no_argument, NULL, 3}, {"bind", required_argument, NULL, 4},
        {"port", required_argument, NULL, 5}, {"name", required_argument, NULL, 6},
        {"text", required_argument, NULL, 7}, {"qr", no_argument, NULL, 8},
        {"no-qr", no_argument, NULL, 9}, {"hash", no_argument, NULL, 10},
        {"no-hash", no_argument, NULL, 11}, {"quiet", no_argument, NULL, 12},
        {"json", no_argument, NULL, 13}, {"version", no_argument, NULL, 14},
        {"help", no_argument, NULL, 15}, {NULL, 0, NULL, 0}
    };
    int option;
    int saw_http = 0;
    int saw_https = 0;
    onlydrop_config_defaults(config);
    opterr = 0;
    while ((option = getopt_long(argc, argv, "e:n:", options, NULL)) != -1) {
        switch (option) {
            case 'e': if (parse_duration(optarg, &config->expires_seconds) != 0) goto invalid; break;
            case 'n': if (parse_u32(optarg, &config->downloads) != 0) goto invalid; break;
            case 1:
                if (strcmp(optarg, "graceful") == 0) config->expiry_mode = ONLYDROP_EXPIRY_GRACEFUL;
                else if (strcmp(optarg, "hard") == 0) config->expiry_mode = ONLYDROP_EXPIRY_HARD;
                else goto invalid;
                break;
            case 2: saw_http = 1; config->use_https = false; break;
            case 3: saw_https = 1; config->use_https = true; break;
            case 4: config->bind_address = optarg; break;
            case 5: if (parse_port(optarg, &config->port) != 0) goto invalid; break;
            case 6: config->name = optarg; break;
            case 7: config->text = optarg; break;
            case 8: config->qr = true; break;
            case 9: config->qr = false; break;
            case 10: config->print_hash = true; break;
            case 11: config->print_hash = false; break;
            case 12: config->quiet = true; break;
            case 13: config->json = true; break;
            case 14: return ONLYDROP_CLI_VERSION;
            case 15: return ONLYDROP_CLI_HELP;
            default: goto invalid;
        }
    }
    if (saw_http && saw_https) goto invalid;
    if (optind + 1 < argc || (optind < argc && config->text != NULL)) goto invalid;
    if (optind < argc) config->input_path = argv[optind];
    if ((config->quiet && config->json) || (config->json && config->qr)) goto invalid;
    return ONLYDROP_CLI_OK;
invalid:
    (void)fprintf(stderr, "Error: invalid command-line arguments.\n");
    return ONLYDROP_CLI_ERROR;
}
