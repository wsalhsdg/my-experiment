#include "double_sha256.h"
#include "sha256.h"

/**
 * 先 sha256 一次，然后对结果再 sha256
 */
void double_sha256(const uint8_t* data, size_t len, uint8_t out[32]) {
    uint8_t tmp[32];
    sha256(data, len, tmp);    // 第一次 SHA256
    sha256(tmp, 32, out);      // 第二次 SHA256
}
