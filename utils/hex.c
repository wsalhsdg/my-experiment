#include "hex.h"
#include <string.h>
#include <stdio.h>

//----hex×ª»»³Ébytes----
int hex_bin(const char* hex, unsigned char* out, size_t outlen) {

    if (!hex || !out) return 0;

    size_t len = strlen(hex);
    if (len % 2 != 0 || len / 2 > outlen) return 0;

    for (size_t i = 0; i < len / 2; i++) {
        unsigned int byte;
        if (sscanf(hex + 2 * i, "%2x", &byte) != 1) return 0;
        out[i] = (unsigned char)byte;
    }

    return 1;
}
