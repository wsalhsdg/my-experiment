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