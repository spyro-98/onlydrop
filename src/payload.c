#include "onlydrop/payload.h"
#include "onlydrop/platform.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <unistd.h>

#define ONLYDROP_FILE_READ_CHUNK (4U * 1024U * 1024U)
#define ONLYDROP_PROGRESS_INTERVAL (16U * 1024U * 1024U)
#define ONLYDROP_MEMORY_RESERVE (512ULL * 1024ULL * 1024ULL)

static char *copy_name(const char *name) {
    const size_t length = strlen(name);
    char *copy = malloc(length + 1U);
    if (copy != NULL) memcpy(copy, name, length + 1U);
    return copy;
}

static onlydrop_payload_result finalize(onlydrop_payload *payload, const char *name) {
    unsigned int length = 0U;
    payload->name = copy_name(name);
    if (payload->name == NULL) {
        return ONLYDROP_PAYLOAD_MEMORY_ERROR;
    }
    if (EVP_Digest(payload->data, payload->size, payload->sha256, &length, EVP_sha256(), NULL) != 1 || length != 32U) {
        return ONLYDROP_PAYLOAD_HASH_ERROR;
    }
    return ONLYDROP_PAYLOAD_OK;
}

static onlydrop_payload_result append_fd(onlydrop_payload *payload, int fd, const char *name) {
    size_t capacity = 0U;
    for (;;) {
        ssize_t read_count;
        if (payload->size == capacity) {
            size_t next = capacity == 0U ? 65536U : capacity * 2U;
            unsigned char *resized;
            if (next < capacity) return ONLYDROP_PAYLOAD_TOO_LARGE;
            resized = realloc(payload->data, next);
            if (resized == NULL) return ONLYDROP_PAYLOAD_MEMORY_ERROR;
            payload->data = resized;
            capacity = next;
        }
        read_count = read(fd, payload->data + payload->size, capacity - payload->size);
        if (read_count < 0) {
            if (errno == EINTR) continue;
            return ONLYDROP_PAYLOAD_READ_ERROR;
        }
        if (read_count == 0) break;
        if ((size_t)read_count > SIZE_MAX - payload->size) return ONLYDROP_PAYLOAD_TOO_LARGE;
        payload->size += (size_t)read_count;
    }
    if (payload->size == 0U) return ONLYDROP_PAYLOAD_EMPTY;
    return finalize(payload, name);
}

static bool fits_memory_policy(size_t requested_size) {
    uint64_t available_memory;
    struct rlimit address_space_limit;
    if (onlydrop_available_memory_bytes(&available_memory) == 0 &&
        (available_memory <= ONLYDROP_MEMORY_RESERVE ||
         (uint64_t)requested_size > available_memory - ONLYDROP_MEMORY_RESERVE)) {
        return false;
    }
    if (getrlimit(RLIMIT_AS, &address_space_limit) == 0 && address_space_limit.rlim_cur != RLIM_INFINITY &&
        (uintmax_t)requested_size > (uintmax_t)address_space_limit.rlim_cur) {
        return false;
    }
    return true;
}

static onlydrop_payload_result read_regular_file(onlydrop_payload *payload, int fd, size_t expected_size, const char *name,
                                                 onlydrop_payload_progress_callback progress, void *progress_context) {
    size_t next_progress = 0U;
    if (progress != NULL) progress(0U, expected_size, progress_context);
    while (payload->size < expected_size) {
        const size_t remaining = expected_size - payload->size;
        const size_t requested = remaining < ONLYDROP_FILE_READ_CHUNK ? remaining : ONLYDROP_FILE_READ_CHUNK;
        const ssize_t read_count = read(fd, payload->data + payload->size, requested);
        if (read_count < 0) {
            if (errno == EINTR) continue;
            return ONLYDROP_PAYLOAD_READ_ERROR;
        }
        if (read_count == 0) return ONLYDROP_PAYLOAD_READ_ERROR;
        payload->size += (size_t)read_count;
        if (progress != NULL && (payload->size >= next_progress || payload->size == expected_size)) {
            progress(payload->size, expected_size, progress_context);
            next_progress = payload->size > SIZE_MAX - ONLYDROP_PROGRESS_INTERVAL
                ? SIZE_MAX : payload->size + ONLYDROP_PROGRESS_INTERVAL;
        }
    }
    return finalize(payload, name);
}

void onlydrop_payload_init(onlydrop_payload *payload) { *payload = (onlydrop_payload){0}; }

onlydrop_payload_result onlydrop_payload_load_file(onlydrop_payload *payload, const char *path, const char *name,
                                                   onlydrop_payload_progress_callback progress, void *progress_context) {
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    struct stat file_info;
    onlydrop_payload_result result;
    if (fd < 0) return ONLYDROP_PAYLOAD_OPEN_ERROR;
    if (fstat(fd, &file_info) != 0) {
        (void)close(fd);
        return ONLYDROP_PAYLOAD_READ_ERROR;
    }
    if (file_info.st_size <= 0) {
        (void)close(fd);
        return ONLYDROP_PAYLOAD_EMPTY;
    }
    if ((uintmax_t)file_info.st_size > (uintmax_t)SIZE_MAX) {
        (void)close(fd);
        return ONLYDROP_PAYLOAD_TOO_LARGE;
    }
    if (!fits_memory_policy((size_t)file_info.st_size)) {
        (void)close(fd);
        return ONLYDROP_PAYLOAD_MEMORY_UNSAFE;
    }
    payload->data = malloc((size_t)file_info.st_size);
    if (payload->data == NULL) {
        (void)close(fd);
        return ONLYDROP_PAYLOAD_MEMORY_ERROR;
    }
    payload->size = 0U;
    const char *base_name = strrchr(path, '/');
    result = read_regular_file(payload, fd, (size_t)file_info.st_size,
                               name != NULL ? name : (base_name != NULL ? base_name + 1U : path), progress, progress_context);
    if (result == ONLYDROP_PAYLOAD_OK) {
        struct stat final_info;
        if (fstat(fd, &final_info) != 0 || final_info.st_size != file_info.st_size) {
            result = ONLYDROP_PAYLOAD_READ_ERROR;
        }
    }
    (void)close(fd);
    return result;
}

onlydrop_payload_result onlydrop_payload_load_stdin(onlydrop_payload *payload, const char *name) {
    return append_fd(payload, STDIN_FILENO, name != NULL ? name : "stdin.bin");
}

onlydrop_payload_result onlydrop_payload_load_text(onlydrop_payload *payload, const char *text, const char *name) {
    size_t length = strlen(text);
    if (length == 0U) return ONLYDROP_PAYLOAD_EMPTY;
    payload->data = malloc(length);
    if (payload->data == NULL) return ONLYDROP_PAYLOAD_MEMORY_ERROR;
    memcpy(payload->data, text, length);
    payload->size = length;
    return finalize(payload, name != NULL ? name : "text.txt");
}

void onlydrop_payload_free(onlydrop_payload *payload) {
    if (payload->data != NULL) OPENSSL_cleanse(payload->data, payload->size);
    free(payload->data);
    if (payload->name != NULL) OPENSSL_cleanse(payload->name, strlen(payload->name));
    free(payload->name);
    OPENSSL_cleanse(payload->sha256, sizeof(payload->sha256));
    onlydrop_payload_init(payload);
}

void onlydrop_payload_sha256_hex(const onlydrop_payload *payload, char output[65]) {
    static const char hex[] = "0123456789abcdef";
    for (size_t index = 0U; index < 32U; ++index) {
        output[index * 2U] = hex[payload->sha256[index] >> 4U];
        output[index * 2U + 1U] = hex[payload->sha256[index] & 0x0fU];
    }
    output[64] = '\0';
}

const char *onlydrop_payload_result_message(onlydrop_payload_result result) {
    switch (result) {
        case ONLYDROP_PAYLOAD_EMPTY: return "payload is empty";
        case ONLYDROP_PAYLOAD_OPEN_ERROR: return "input could not be opened";
        case ONLYDROP_PAYLOAD_MEMORY_UNSAFE: return "payload would leave less than the 512 MiB RAM safety reserve";
        case ONLYDROP_PAYLOAD_MEMORY_ERROR: return "payload cannot fit in currently available process memory";
        case ONLYDROP_PAYLOAD_READ_ERROR: return "input changed or could not be read completely";
        case ONLYDROP_PAYLOAD_HASH_ERROR: return "SHA-256 calculation failed";
        case ONLYDROP_PAYLOAD_TOO_LARGE: return "payload is too large for this process";
        case ONLYDROP_PAYLOAD_OK: return "success";
    }
    return "unknown payload error";
}
