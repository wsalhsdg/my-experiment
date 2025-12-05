#include "crypto/base58check.h"
#include <string.h>
#include <stdio.h>

//----Base58 ×ÖÄ¸±í----
static const char* BASE58 = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

//----Base58Check±àÂë----
void Base58check_encode(const unsigned char* input, int len, char* out, int outlen) {
    unsigned char buf[64];

    if (len > (int)sizeof(buf)) { out[0] = '\0'; return; }
    memset(buf, 0, sizeof(buf));
    memcpy(buf + (sizeof(buf) - len), input, len);

    int zeros = 0;
    for (int i = 0; i < len; i++) if (input[i] == 0) zeros++; else break;

    char temp[128];
    int temp_index = 0;
    int start = sizeof(buf) - len;

    while (start < (int)sizeof(buf)) {
        unsigned int remainder = 0;
        for (int i = start; i < (int)sizeof(buf); i++) {
            unsigned int acc = (remainder << 8) + buf[i];
            buf[i] = (unsigned char)(acc / 58);
            remainder = acc % 58;
        }
        temp[temp_index++] = BASE58[remainder];

        while (start < (int)sizeof(buf) && buf[start] == 0) start++;
        if (start == (int)sizeof(buf)) break;
    }

    int outpos = 0;
    for (int i = 0; i < zeros; i++)
        if (outpos < outlen - 1) out[outpos++] = '1';
    for (int i = temp_index - 1; i >= 0; i--)
        if (outpos < outlen - 1) out[outpos++] = temp[i];
    out[outpos] = '\0';
}


