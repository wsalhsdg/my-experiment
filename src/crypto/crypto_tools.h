#pragma once
#include <stdint.h>
#include <stddef.h>

#define BTC_ADDRESS_MAXLEN 35

// 双 SHA256
void double_sha256(const uint8_t* data, size_t len, uint8_t out[32]);

// RIPEMD160
void ripemd160(const uint8_t* data, size_t len, uint8_t out[20]);

// Base58Check 编码
size_t base58check_encode(const uint8_t* payload, size_t payload_len, char* out, size_t outlen);

// 工具函数：将公钥转换为比特币地址（P2PKH）
void pubkey_to_address(const uint8_t pubkey[33], char out_address[BTC_ADDRESS_MAXLEN]);

// 工具函数：将任意数据计算为 hash160（SHA256+RIPEMD160）
void hash160(const uint8_t* data, size_t len, uint8_t out[20]);
