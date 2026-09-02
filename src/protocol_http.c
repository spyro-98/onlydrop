#include "onlydrop/protocol.h"

#include <stdio.h>

#if ONLYDROP_HAVE_LIBEVENT
#include "onlydrop/platform.h"
#include "onlydrop/qr.h"
#include "onlydrop/security.h"

#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/event.h>
#include <event2/http.h>
#include <event2/util.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    onlydrop_server *server;
    struct event_base *base;
    struct evhttp *http;
    struct event *expiry_event;
    struct event *progress_event;
    char path[68];
    bool transfer_active;
    bool progress_printed;
    size_t output_bytes_at_start;
    size_t last_sent_bytes;
    uint64_t last_progress_ns;
    uint64_t last_sample_ns;
    struct bufferevent *transfer_bev;
} http_context;

/* evhttp's error callback does not carry an application argument. OnlyDrop
 * deliberately allows one transfer, so this process-local pointer is safe for
 * the lifetime of its single event loop. */
static http_context *active_http_context;

static int selected_port(struct evhttp_bound_socket *handle, uint16_t *port) {
    struct sockaddr_in address;
    socklen_t length = (socklen_t)sizeof(address);
    const evutil_socket_t fd = evhttp_bound_socket_get_fd(handle);
    if (fd < 0 || getsockname(fd, (struct sockaddr *)&address, &length) != 0 || address.sin_family != AF_INET) return -1;
    *port = ntohs(address.sin_port);
    return 0;
}

static void print_startup(const onlydrop_server *server, const char *url) {
    char sha256[65];
    onlydrop_payload_sha256_hex(server->payload, sha256);
    if (server->config->json) {
        (void)printf("{\"name\":\"%s\",\"size\":%zu,\"sha256\":\"%s\",\"protocol\":\"http\",\"url\":\"%s\",\"expires_in_seconds\":%llu,\"downloads\":%u,\"expiry_mode\":\"%s\"}\n",
            server->payload->name, server->payload->size, sha256, url,
            (unsigned long long)server->config->expires_seconds, server->config->downloads,
            onlydrop_expiry_mode_name(server->config->expiry_mode));
    } else if (!server->config->quiet) {
        (void)printf("OnlyDrop v0.1\n\nLoaded into RAM: %zu bytes\nName:            %s\nStorage:         anonymous RAM\nExpires in:      %llu seconds\nDownloads:       %u\nExpiry mode:     %s\n",
            server->payload->size, server->payload->name, (unsigned long long)server->config->expires_seconds,
            server->config->downloads, onlydrop_expiry_mode_name(server->config->expiry_mode));
        if (server->config->print_hash) (void)printf("SHA-256:         %s\n", sha256);
        (void)printf("\nLink (copy this):\n%s\n", url);
    }
}

static void send_text(struct evhttp_request *request, int status, const char *reason, const char *body) {
    struct evbuffer *buffer = evbuffer_new();
    if (buffer != NULL) {
        (void)evbuffer_add_printf(buffer, "%s\n", body);
        evhttp_add_header(evhttp_request_get_output_headers(request), "Content-Type", "text/plain; charset=utf-8");
        evhttp_send_reply(request, status, reason, buffer);
        evbuffer_free(buffer);
    } else {
        evhttp_send_reply(request, status, reason, NULL);
    }
}

static void format_bytes(size_t bytes, char output[32]) {
    static const char *const units[] = {"B", "KB", "MB", "GB", "TB"};
    double value = (double)bytes;
    size_t unit = 0U;
    while (value >= 1024.0 && unit + 1U < sizeof(units) / sizeof(units[0])) {
        value /= 1024.0;
        ++unit;
    }
    (void)snprintf(output, 32U, unit == 0U ? "%.0f %s" : "%.2f %s", value, units[unit]);
}

static size_t transfer_sent_bytes(const http_context *context) {
    size_t queued;
    size_t sent;
    if (context->transfer_bev == NULL) return 0U;
    queued = evbuffer_get_length(bufferevent_get_output(context->transfer_bev));
    sent = context->output_bytes_at_start > queued ? context->output_bytes_at_start - queued : 0U;
    return sent > context->server->payload->size ? context->server->payload->size : sent;
}

static void clear_progress_line(http_context *context) {
    if (context->progress_printed) {
        (void)fprintf(stderr, "\r\033[2K");
        context->progress_printed = false;
    }
}

static void stop_progress(http_context *context) {
    if (context->progress_event != NULL) {
        event_free(context->progress_event);
        context->progress_event = NULL;
    }
    clear_progress_line(context);
}

static void progress_callback(evutil_socket_t fd, short events, void *argument) {
    http_context *context = argument;
    const uint64_t now_ns = onlydrop_monotonic_ns();
    const size_t sent = transfer_sent_bytes(context);
    const size_t total = context->server->payload->size;
    char sent_text[32];
    char total_text[32];
    char rate_text[32];
    double rate = 0.0;
    (void)fd;
    (void)events;
    if (!context->transfer_active || total == 0U) return;
    if (sent > context->last_sent_bytes) context->last_progress_ns = now_ns;
    if (now_ns > context->last_sample_ns) {
        rate = (double)(sent - context->last_sent_bytes) * 1000000000.0 / (double)(now_ns - context->last_sample_ns);
    }
    format_bytes(sent, sent_text);
    format_bytes(total, total_text);
    format_bytes((size_t)rate, rate_text);
    clear_progress_line(context);
    if (now_ns - context->last_progress_ns >= UINT64_C(3000000000)) {
        (void)fprintf(stderr, "Sending: %s / %s (%.1f%%) · waiting for client (possibly paused)",
            sent_text, total_text, 100.0 * (double)sent / (double)total);
    } else {
        (void)fprintf(stderr, "Sending: %s / %s (%.1f%%) · %s/s",
            sent_text, total_text, 100.0 * (double)sent / (double)total, rate_text);
    }
    (void)fflush(stderr);
    context->progress_printed = true;
    context->last_sent_bytes = sent;
    context->last_sample_ns = now_ns;
}

static void start_progress(struct evhttp_request *request, http_context *context) {
    const struct timeval interval = { .tv_sec = 0L, .tv_usec = 500000L };
    if (context->server->config->quiet || context->server->config->json || !isatty(STDERR_FILENO)) return;
    context->transfer_bev = evhttp_connection_get_bufferevent(evhttp_request_get_connection(request));
    if (context->transfer_bev == NULL) return;
    context->output_bytes_at_start = evbuffer_get_length(bufferevent_get_output(context->transfer_bev));
    context->last_progress_ns = onlydrop_monotonic_ns();
    context->last_sample_ns = context->last_progress_ns;
    context->progress_event = event_new(context->base, -1, EV_PERSIST, progress_callback, context);
    if (context->progress_event != NULL) (void)event_add(context->progress_event, &interval);
}

static void complete_request(struct evhttp_request *request, void *argument) {
    http_context *context = argument;
    (void)request;
    if (!context->transfer_active) return;
    stop_progress(context);
    context->transfer_active = false;
    (void)onlydrop_lifecycle_transfer_finished(context->server->lifecycle, true);
    (void)fprintf(stderr, "Download completed.\n");
    if (onlydrop_lifecycle_should_stop(context->server->lifecycle)) event_base_loopexit(context->base, NULL);
}

static void request_error_callback(enum evhttp_request_error error, void *argument) {
    http_context *context = active_http_context;
    (void)error;
    (void)argument;
    if (context == NULL || !context->transfer_active) return;
    stop_progress(context);
    context->transfer_active = false;
    onlydrop_lifecycle_transfer_cancelled(context->server->lifecycle);
    (void)fprintf(stderr, "Download interrupted by the client or network. Closing drop.\n");
    event_base_loopexit(context->base, NULL);
}

static void request_callback(struct evhttp_request *request, void *argument) {
    http_context *context = argument;
    onlydrop_server *server = context->server;
    const char *uri = evhttp_request_get_uri(request);
    char safe_name[256];
    char disposition[300];
    char content_length[32];
    struct evbuffer *body;
    if (evhttp_request_get_command(request) != EVHTTP_REQ_GET) {
        send_text(request, 405, "Method Not Allowed", "Method not allowed.");
        return;
    }
    if (strcmp(uri, context->path) != 0 || !onlydrop_constant_time_equal(uri + 3U, server->token)) {
        send_text(request, 404, "Not Found", "Not found.");
        return;
    }
    if (onlydrop_monotonic_ns() >= server->lifecycle->expires_at_ns) {
        onlydrop_lifecycle_expire(server->lifecycle);
        send_text(request, 410, "Gone", "This drop has expired.");
        return;
    }
    if (!onlydrop_lifecycle_can_start(server->lifecycle, onlydrop_monotonic_ns())) {
        send_text(request, 409, "Conflict", "A download is already in progress.");
        return;
    }
    if (onlydrop_sanitize_filename(server->payload->name, safe_name, sizeof(safe_name)) != 0) {
        send_text(request, 500, "Internal Server Error", "Internal server error.");
        return;
    }
    (void)snprintf(disposition, sizeof(disposition), "attachment; filename=\"%s\"", safe_name);
    (void)snprintf(content_length, sizeof(content_length), "%zu", server->payload->size);
    body = evbuffer_new();
    /* The payload remains owned by onlydrop until the request completion callback.
     * Referencing it avoids a second multi-gigabyte allocation in libevent. */
    if (body == NULL || evbuffer_add_reference(body, server->payload->data, server->payload->size, NULL, NULL) != 0) {
        if (body != NULL) evbuffer_free(body);
        send_text(request, 500, "Internal Server Error", "Internal server error.");
        return;
    }
    onlydrop_lifecycle_transfer_started(server->lifecycle);
    context->transfer_active = true;
    evhttp_add_header(evhttp_request_get_output_headers(request), "Content-Type", "application/octet-stream");
    evhttp_add_header(evhttp_request_get_output_headers(request), "Content-Length", content_length);
    evhttp_add_header(evhttp_request_get_output_headers(request), "Content-Disposition", disposition);
    evhttp_add_header(evhttp_request_get_output_headers(request), "Cache-Control", "no-store");
    evhttp_request_set_error_cb(request, request_error_callback);
    evhttp_request_set_on_complete_cb(request, complete_request, context);
    evhttp_send_reply(request, 200, "OK", body);
    start_progress(request, context);
    evbuffer_free(body);
}

static void expiry_callback(evutil_socket_t fd, short events, void *argument) {
    http_context *context = argument;
    (void)fd;
    (void)events;
    onlydrop_lifecycle_expire(context->server->lifecycle);
    if (context->server->config->expiry_mode == ONLYDROP_EXPIRY_HARD && context->transfer_active) {
        stop_progress(context);
        context->transfer_active = false;
        (void)fprintf(stderr, "Hard expiry reached. Interrupting active download.\n");
    }
    if (onlydrop_lifecycle_should_stop(context->server->lifecycle)) event_base_loopexit(context->base, NULL);
}

int onlydrop_server_run(onlydrop_server *server) {
    http_context context = { .server = server };
    struct evhttp_bound_socket *handle;
    struct timeval delay;
    char address[INET_ADDRSTRLEN];
    char url[160];
    uint16_t port;
    const char *failure_reason = "HTTP server initialisation failed";
    if (server->config->use_https) {
        (void)fprintf(stderr, "Error: HTTPS is not implemented yet.\n");
        return -1;
    }
    context.base = event_base_new();
    context.http = context.base != NULL ? evhttp_new(context.base) : NULL;
    if (context.http == NULL) {
        failure_reason = "could not initialise libevent HTTP";
        goto failure;
    }
    (void)snprintf(context.path, sizeof(context.path), "/d/%s", server->token);
    active_http_context = &context;
    evhttp_set_gencb(context.http, request_callback, &context);
    handle = evhttp_bind_socket_with_handle(context.http, server->config->bind_address != NULL ? server->config->bind_address : "0.0.0.0", server->config->port);
    if (handle == NULL) {
        failure_reason = "could not bind the requested address and port";
        goto failure;
    }
    if (selected_port(handle, &port) != 0) {
        failure_reason = "could not read the listener port";
        goto failure;
    }
    if (onlydrop_choose_public_ipv4(server->config->bind_address, address, sizeof(address)) != 0) {
        failure_reason = "could not choose a LAN address";
        goto failure;
    }
    if (snprintf(url, sizeof(url), "http://%s:%u%s", address, (unsigned int)port, context.path) >= (int)sizeof(url)) {
        failure_reason = "generated URL is too long";
        goto failure;
    }
    delay.tv_sec = (long)server->config->expires_seconds;
    delay.tv_usec = 0L;
    context.expiry_event = evtimer_new(context.base, expiry_callback, &context);
    if (context.expiry_event == NULL || evtimer_add(context.expiry_event, &delay) != 0) {
        failure_reason = "could not schedule expiry";
        goto failure;
    }
    print_startup(server, url);
    if (server->config->qr && onlydrop_qr_print(url) != 0) goto failure;
    (void)fflush(stdout);
    (void)fprintf(stderr, "Waiting for one download. Press Ctrl+C to stop.\n");
    (void)event_base_dispatch(context.base);
    active_http_context = NULL;
    stop_progress(&context);
    if (context.expiry_event != NULL) event_free(context.expiry_event);
    evhttp_free(context.http);
    event_base_free(context.base);
    return 0;
failure:
    active_http_context = NULL;
    (void)fprintf(stderr, "Error: %s. Nothing was served.\n", failure_reason);
    if (context.expiry_event != NULL) event_free(context.expiry_event);
    if (context.http != NULL) evhttp_free(context.http);
    if (context.base != NULL) event_base_free(context.base);
    return -1;
}
#else
int onlydrop_server_run(onlydrop_server *server) {
    (void)server;
    (void)fprintf(stderr, "Error: this build has no libevent HTTP server support.\n");
    return -1;
}
#endif
