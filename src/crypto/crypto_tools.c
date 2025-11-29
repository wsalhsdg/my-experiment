#include "crypto_tools.h"
#include "sha256.h"
#include "ripemd160.h"
#include "base58.h"
#include "double_sha256.h"
#include <string.h>

// 将任意数据生成 hash160 (SHA256 + RIPEMD160)
void hash160(const uint8_t* data, size_t len, uint8_t out[20]) {
    uint8_t sha[32];
    sha256(data, len, sha);
    ripemd160(sha, 32, out);
}

// 将公钥转换为 P2PKH 地址
void pubkey_to_address(const uint8_t pubkey[33], char out_address[BTC_ADDRESS_MAXLEN]) {
    uint8_t payload[21];
    payload[0] = 0x00; // mainnet version
    hash160(pubkey, 33, payload + 1); // hash160(pubkey)

    base58check_encode(payload, 21, out_address, BTC_ADDRESS_MAXLEN);
}
