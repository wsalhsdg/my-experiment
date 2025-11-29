#include "hex.h"

static const char* HEX = "0123456789abcdef";

void bytes_to_hex(const uint8_t* in, size_t len, char* out) {
    for (size_t i = 0; i < len; i++) {
        out[i * 2] = HEX[(in[i] >> 4) & 0xF];
        out[i * 2 + 1] = HEX[in[i] & 0xF];
    }
}
