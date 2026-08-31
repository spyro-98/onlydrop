#include "onlydrop/qr.h"

#include <stdio.h>

#if ONLYDROP_HAVE_QRENCODE
#include <qrencode.h>

int onlydrop_qr_print(const char *url) {
    QRcode *code = QRcode_encodeString8bit(url, 0, QR_ECLEVEL_M);
    if (code == NULL) return -1;
    (void)puts("\nQR:");
    for (int row = -2; row < code->width + 2; ++row) {
        for (int column = -2; column < code->width + 2; ++column) {
            const int is_black = row >= 0 && row < code->width && column >= 0 && column < code->width &&
                (code->data[row * code->width + column] & 1U) != 0U;
            (void)fputs(is_black ? "██" : "  ", stdout);
        }
        (void)putchar('\n');
    }
    QRcode_free(code);
    return 0;
}
#else
int onlydrop_qr_print(const char *url) {
    (void)url;
    (void)fprintf(stderr, "Error: this build has no libqrencode support.\n");
    return -1;
}
#endif
