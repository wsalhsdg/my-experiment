#pragma once
#include <stdint.h>
#include <stddef.h>

size_t base58_encode(const uint8_t* in, size_t inlen, char* out, size_t outlen);
size_t base58check_encode(const uint8_t* payload, size_t payload_len, char* out, size_t outlen);

int base58_decode(const char* in, uint8_t* out, size_t* out_len);
size_t base58check_decode(const char* in, uint8_t* payload_out, size_t payload_max_len);