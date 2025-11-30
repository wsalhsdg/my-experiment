#include "base58.h"
#include "double_sha256.h"
#include <string.h>
#include <stdio.h>

static const char* BASE58 = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

// 计算大整数的 Base58 编码
size_t base58_encode(const uint8_t* in, size_t inlen, char* out, size_t outlen) {
    size_t zeroes = 0;
    while (zeroes < inlen && in[zeroes] == 0) zeroes++;

    size_t size = (inlen - zeroes) * 138 / 100 + 1; // log(256)/log(58) ≈ 1.38
    uint8_t buf[size];
    memset(buf, 0, size);

    size_t j;
    for (size_t i = zeroes; i < inlen; i++) {
        int carry = in[i];
        for (j = size; j-- > 0;) {
            carry += 256 * buf[j];
            buf[j] = carry % 58;
            carry /= 58;
        }
    }

    j = 0;
    while (j < size && buf[j] == 0) j++;

    size_t out_index = 0;
    for (size_t i = 0; i < zeroes; i++) {
        if (out_index >= outlen - 1) return 0;
        out[out_index++] = '1';
    }

    for (; j < size; j++) {
        if (out_index >= outlen - 1) return 0;
        out[out_index++] = BASE58[buf[j]];
    }

    if (out_index >= outlen) return 0;
    out[out_index] = '\0';
    return out_index;
}

// Base58Check 编码
size_t base58check_encode(const uint8_t* payload, size_t payload_len, char* out, size_t outlen) {
    if (payload_len + 4 > 128) return 0; // 简单限制
    uint8_t buf[128];
    memcpy(buf, payload, payload_len);

    uint8_t hash[32];
    double_sha256(payload, payload_len, hash);
    memcpy(buf + payload_len, hash, 4); // 前4字节为 checksum

    return base58_encode(buf, payload_len + 4, out, outlen);
}

// =====================================================
// Base58 decode (raw)
// =====================================================
int base58_decode(const char* in, uint8_t* out, size_t* out_len) {
    size_t len = strlen(in);
    size_t zeroes = 0;

    // Count leading '1'
    while (zeroes < len && in[zeroes] == '1') zeroes++;

    size_t size = (len - zeroes) * 733 / 1000 + 1; // log(58)/log(256) ≈ 0.733
    uint8_t buf[size];
    memset(buf, 0, size);

    for (size_t i = zeroes; i < len; i++) {
        const char* p = strchr(BASE58, in[i]);
        if (!p) return 0; // invalid char
        int carry = p - BASE58;

        for (int j = size - 1; j >= 0; j--) {
            carry += 58 * buf[j];
            buf[j] = carry % 256;
            carry /= 256;
        }
        if (carry != 0) return 0; // overflow
    }

    // Skip leading zeroes in decoded buffer
    size_t j = 0;
    while (j < size && buf[j] == 0) j++;

    size_t decoded_len = zeroes + (size - j);
    if (*out_len < decoded_len) return 0; // no enough space

    // Write leading zeros
    memset(out, 0, zeroes);

    // Copy rest
    memcpy(out + zeroes, buf + j, size - j);

    *out_len = decoded_len;
    return 1;
}

// =====================================================
// Base58Check decode
// =====================================================
// Returns payload length (without version & checksum)
size_t base58check_decode(const char* input, uint8_t* payload_out, size_t payload_max_len) {
    uint8_t tmp[128];
    size_t tmplen = sizeof(tmp);

    // Base58 decode raw bytes
    if (!base58_decode(input, tmp, &tmplen)) return 0;

    if (tmplen < 4) return 0; // must at least contain checksum

    // Extract checksum
    uint8_t check1[32];
    double_sha256(tmp, tmplen - 4, check1);

    if (memcmp(check1, tmp + tmplen - 4, 4) != 0) {
        return 0; // checksum mismatch
    }

    // Payload = raw - checksum
    size_t payload_len = tmplen - 4;
    if (payload_len > payload_max_len) return 0;

    memcpy(payload_out, tmp, payload_len);
    return payload_len;
}
