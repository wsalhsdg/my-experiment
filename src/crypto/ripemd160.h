#pragma once
#include <stdint.h>
#include <stddef.h>

void ripemd160(const uint8_t* data, size_t len, uint8_t out[20]);
