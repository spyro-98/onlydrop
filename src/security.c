#include "onlydrop/security.h"

#include <openssl/crypto.h>
#include <openssl/rand.h>
#include <string.h>

int onlydrop_generate_token(char output[65]) {
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    unsigned char random[48];
    if (RAND_bytes(random, (int)sizeof(random)) != 1) return -1;
    for (size_t index = 0U; index < sizeof(random); ++index) {
        output[index] = alphabet[random[index] & 0x3fU];
    }
    output[sizeof(random)] = '\0';
    OPENSSL_cleanse(random, sizeof(random));
    return 0;
}

int onlydrop_constant_time_equal(const char *left, const char *right) {
    const size_t left_length = strlen(left);
    const size_t right_length = strlen(right);
    return left_length == right_length && CRYPTO_memcmp(left, right, left_length) == 0;
}

int onlydrop_sanitize_filename(const char *input, char *output, size_t output_size) {
    size_t written = 0U;
    if (input == NULL || output_size < 2U) return -1;
    for (; *input != '\0' && written + 1U < output_size; ++input) {
        const unsigned char character = (unsigned char)*input;
        if (character >= 0x20U && character != 0x7fU && character != '"' && character != '\\' && character != '/' && character != '\r' && character != '\n') {
            output[written++] = (char)character;
        } else {
            output[written++] = '_';
        }
    }
    if (written == 0U) {
        output[written++] = 'd';
    }
    output[written] = '\0';
    return 0;
}
