#pragma once
#include <stdint.h>
#include <stddef.h>

size_t base58_encode(const uint8_t* in, size_t inlen, char* out, size_t outlen);
size_t base58check_encode(const uint8_t* payload, size_t payload_len, char* out, size_t outlen);
