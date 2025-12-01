#pragma once
#include "sha256.h"
#include "ripemd160.h"
#include "base58.h"
#include "double_sha256.h"
#include <stdint.h>
#include <stddef.h>
#include <secp256k1.h>

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

// ECDSA 签名（私钥为 32 字节）
// 返回 1=成功，0=失败
int ecdsa_sign(const uint8_t privkey[32],
    const uint8_t hash32[32],
    uint8_t sig_out[72], size_t* sig_len);

// ECDSA 验证（压缩公钥 33 字节）
// 返回 1=有效，0=无效
int ecdsa_verify(const uint8_t pubkey[33],
    const uint8_t hash32[32],
    const uint8_t* sig, size_t sig_len);

// 生成压缩公钥（33 字节）
int ecdsa_get_pubkey(const uint8_t privkey[32], uint8_t pubkey33_out[33]);

void crypto_secp_set_context(secp256k1_context* ctx);
secp256k1_context* crypto_secp_get_context();

// ========== 兼容旧接口（wrapper） ==========
#define crypto_secp_sign      ecdsa_sign
#define crypto_secp_verify    ecdsa_verify
#define crypto_secp_get_pubkey ecdsa_get_pubkey