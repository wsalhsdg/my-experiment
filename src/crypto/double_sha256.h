#pragma once
#include <stdint.h>
#include <stddef.h>

/**
 * 对 data 长度为 len 的数据进行双 SHA256
 * out: 32 字节输出缓冲
 */
void double_sha256(const uint8_t* data, size_t len, uint8_t out[32]);
