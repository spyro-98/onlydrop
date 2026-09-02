#include "onlydrop/cli.h"
#include "onlydrop/lifecycle.h"
#include "onlydrop/payload.h"
#include "onlydrop/platform.h"
#include "onlydrop/protocol.h"
#include "onlydrop/security.h"

#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

typedef struct {
    bool active;
} loading_progress;

static void report_loading_progress(size_t loaded, size_t total, void *context) {
    loading_progress *progress = context;
    const double loaded_mebibytes = (double)loaded / (1024.0 * 1024.0);
    const double total_mebibytes = (double)total / (1024.0 * 1024.0);
    const double percent = total == 0U ? 100.0 : ((double)loaded * 100.0) / (double)total;
    if (!progress->active) return;
    (void)fprintf(stderr, "\rLoading into RAM: %.1f MiB / %.1f MiB (%.1f%%)",
                  loaded_mebibytes, total_mebibytes, percent);
    (void)fflush(stderr);
}

static onlydrop_payload_result load_payload(const onlydrop_config *config, onlydrop_payload *payload) {
    loading_progress progress = { .active = !config->quiet && !config->json && isatty(STDERR_FILENO) != 0 };
    onlydrop_payload_result result;
    if (config->text != NULL) return onlydrop_payload_load_text(payload, config->text, config->name);
    if (config->input_path != NULL) {
        result = onlydrop_payload_load_file(payload, config->input_path, config->name, report_loading_progress, &progress);
        if (progress.active) (void)fputc('\n', stderr);
        return result;
    }
    if (!onlydrop_stdin_is_interactive()) return onlydrop_payload_load_stdin(payload, config->name);
    (void)fprintf(stderr, "Error: provide FILE, piped stdin, or --text.\n");
    return ONLYDROP_PAYLOAD_OPEN_ERROR;
}

int main(int argc, char **argv) {
    onlydrop_config config;
    onlydrop_payload payload;
    onlydrop_lifecycle lifecycle;
    onlydrop_server server;
    char token[65] = {0};
    const onlydrop_cli_result parsed = onlydrop_cli_parse(argc, argv, &config);
    if (parsed == ONLYDROP_CLI_HELP) {
        onlydrop_cli_print_usage(argv[0]);
        return 0;
    }
    if (parsed == ONLYDROP_CLI_VERSION) {
        (void)puts("OnlyDrop v0.1.0");
        return 0;
    }
    if (parsed != ONLYDROP_CLI_OK) {
        onlydrop_cli_print_usage(argv[0]);
        return 64;
    }
    onlydrop_payload_init(&payload);
    const onlydrop_payload_result payload_result = load_payload(&config, &payload);
    if (payload_result != ONLYDROP_PAYLOAD_OK) {
        (void)fprintf(stderr, "Error: %s. Nothing was served.\n", onlydrop_payload_result_message(payload_result));
        onlydrop_payload_free(&payload);
        return 1;
    }
    if (onlydrop_generate_token(token) != 0) {
        (void)fprintf(stderr, "Error: secure token generation failed. Nothing was served.\n");
        onlydrop_payload_free(&payload);
        return 1;
    }
    onlydrop_lifecycle_init(&lifecycle, &config, onlydrop_monotonic_ns());
    server = (onlydrop_server){ .config = &config, .payload = &payload, .lifecycle = &lifecycle, .token = token };
    const int result = onlydrop_server_run(&server);
    onlydrop_payload_free(&payload);
    return result == 0 ? 0 : 1;
}
