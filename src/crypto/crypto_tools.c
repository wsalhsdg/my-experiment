#include "crypto_tools.h"
#include "sha256.h"
#include "ripemd160.h"
#include "base58.h"
#include "double_sha256.h"
#include <stdio.h>
#include <string.h>
#include <secp256k1.h>
#include <secp256k1_recovery.h>



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

static secp256k1_context* ctx = NULL;

static void ensure_ctx() {
    if (!ctx) {
        ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    }
}

// ====================== ECDSA Sign =========================

int ecdsa_sign(const uint8_t privkey[32],
    const uint8_t hash32[32],
    uint8_t sig_out[72], size_t* sig_len)
{
    ensure_ctx();

    if (!privkey || !hash32 || !sig_out || !sig_len) {
        printf("ecdsa_sign error: NULL pointer input\n");
        return 0;
    }

    if (!secp256k1_ec_seckey_verify(ctx, privkey)) {
        printf("ecdsa_sign error: invalid private key\n");
        return 0;
    }

    secp256k1_ecdsa_signature sig;

    int ret = secp256k1_ecdsa_sign(ctx, &sig, hash32, privkey, NULL, NULL);
    if (!ret) {
        printf("ecdsa_sign error: secp256k1_ecdsa_sign failed\n");
        return 0;
    }

    size_t max_len = 72;
    *sig_len = max_len;
    ret = secp256k1_ecdsa_signature_serialize_der(ctx, sig_out, sig_len, &sig);
    if (!ret) {
        printf("ecdsa_sign error: DER serialize failed\n");
        return 0;
    }

    printf("ecdsa_sign success: sig_len=%zu\n", *sig_len);
    return 1;
}

// ====================== ECDSA Verify =========================

int ecdsa_verify(const uint8_t pubkey33[33],
    const uint8_t hash32[32],
    const uint8_t* sig, size_t sig_len)
{
    ensure_ctx();

    secp256k1_pubkey pubkey;
    secp256k1_ecdsa_signature signature;

    // 解析公钥（压缩格式 33 字节）
    if (!secp256k1_ec_pubkey_parse(ctx, &pubkey, pubkey33, 33)) {
        return 0;
    }

    // 解析 DER 签名
    if (!secp256k1_ecdsa_signature_parse_der(ctx, &signature, sig, sig_len)) {
        return 0;
    }

    // 严格低 S 值（BIP62）
    secp256k1_ecdsa_signature_normalize(ctx, &signature, &signature);

    // 验证签名
    return secp256k1_ecdsa_verify(ctx, &signature, hash32, &pubkey);
}

// ====================== Get Compressed PubKey =========================

int ecdsa_get_pubkey(const uint8_t privkey[32], uint8_t pubkey33_out[33])
{
    ensure_ctx();

    secp256k1_pubkey pub;

    if (!secp256k1_ec_pubkey_create(ctx, &pub, privkey))
        return 0;

    size_t out_len = 33;

    if (!secp256k1_ec_pubkey_serialize(
        ctx,
        pubkey33_out,
        &out_len,
        &pub,
        SECP256K1_EC_COMPRESSED))
        return 0;

    return 1;
}

static secp256k1_context* g_ctx = NULL;

// 如果外部没有注入 context，就在内部创建并保存（单一实例）
static secp256k1_context* get_ctx() {
    if (g_ctx) return g_ctx;
    g_ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    return g_ctx;
}

void crypto_secp_set_context(secp256k1_context* ctx) {
    // 如果外部传入 ctx，我们使用它（并覆盖内部）
    if (g_ctx && ctx && g_ctx != ctx) {
        // 如果内部已有 ctx，先销毁它再替换（可选）
        secp256k1_context_destroy(g_ctx);
    }
    g_ctx = ctx;
}

secp256k1_context* crypto_secp_get_context() {
    return g_ctx ? g_ctx : get_ctx();
}