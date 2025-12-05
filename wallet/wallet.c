#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <stdlib.h>
#include <secp256k1.h>
#include "wallet/wallet.h"
#include "crypto/base58check.h"
//#include "../crypto/crypto_tools.h" // hash160, pubkey_to_address
//#include "../crypto/double_sha256.h"
//#include "../crypto/sha256.h"
//#include "../crypto/ripemd160.h"
#include "utils/hex.h"

/*
int hex_to_bytes(const char* hex, uint8_t* out, size_t out_len) {
    size_t hex_len = strlen(hex);

    if (hex_len % 2 != 0) return -1;
    if (out_len < hex_len / 2) return -1;

    for (size_t i = 0; i < hex_len; i += 2) {
        int a = hex_val(hex[i]);
        int b = hex_val(hex[i + 1]);
        if (a < 0 || b < 0) return -1;

        out[i / 2] = (uint8_t)((a << 4) | b);
    }

    return hex_len / 2; // number of bytes written
}*/

//----随机生成私钥----
void generate_privkey(unsigned char* priv) {
    srand((unsigned int)time(NULL));
    for (int i = 0; i < 32; i++) priv[i] = rand() % 256;
}

//----私钥生成WIF----
void privkey_to_WIF(unsigned char* priv, int priv_len, int compressed, char* wif_out, int wif_outlen) {
    // payload 前缀 + 私钥数据
    unsigned char payload[34];
    payload[0] = 0xB0; // 自定义 WIF 前缀，类似比特币中 0x80
    memcpy(payload + 1, priv, priv_len);

    int payload_size = priv_len + 1;

    // 如果使用压缩公钥，则在私钥末尾加 0x01
    if (compressed) { 
        payload[payload_size] = 0x01; payload_size += 1; 
    }

    // 计算双 SHA256 校验码，使用两次sha256
    unsigned char hash1[32], hash2[32], final[38];
    SHA256(payload, payload_size, hash1);   // 第一次 SHA256
    SHA256(hash1, 32, hash2);               // 第二次 SHA256

    // 构建最终字节序列
    memcpy(final, payload, payload_size);
    memcpy(final + payload_size, hash2, 4);

    Base58check_encode(final, payload_size + 4, wif_out, wif_outlen);
}

//----私钥生成公钥 + 地址----
int privkey_to_pubkey_and_addr(
    const unsigned char* priv_key,       // 输入私钥
    unsigned char* pub_key_out,          // 输出公钥
    size_t* pub_key_out_len,             // 输入/输出：公钥长度
    char* addr_out,                      // 输出地址字符串
    int addr_out_len,                    // 地址缓冲区长度
    int compressed                       // 是否使用压缩公钥
) {
    if (!priv_key || !pub_key_out || !pub_key_len || !addr_out) return 0;

    // 创建 secp256k1 上下文
    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    if (!ctx) return 0;

    // 验证私钥
    if (!secp256k1_ec_seckey_verify(ctx, priv_key)) {
        secp256k1_context_destroy(ctx);
        return 0;
    }

    // 根据私钥生成公钥
    secp256k1_pubkey pub_key;
    if (!secp256k1_ec_pubkey_create(ctx, &pub_key, priv_key)) {
        secp256k1_context_destroy(ctx);
        return 0;
    }

    // 序列化公钥
    size_t publen = *pub_key_out_len;
    if (compressed)
        secp256k1_ec_pubkey_serialize(ctx, pub_key_out, &publen, &pub_key, SECP256K1_EC_COMPRESSED);
    else
        secp256k1_ec_pubkey_serialize(ctx, pub_key_out, &publen, &pub_key, SECP256K1_EC_UNCOMPRESSED);
    *pub_key_out_len = publen;

    // ----计算HASH160(publickey)----
    // HASH160 = RIPEMD160(SHA256(pubkey))
    unsigned char SHA256[32], ripemd_hash[20]; 
    unsigned int ripemdlen;
    SHA256(pub_key_out, publen, SHA256);
    EVP_MD_CTX* ctx2 = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx2, EVP_ripemd160(), NULL);
    EVP_DigestUpdate(ctx2, SHA256, 32);
    EVP_DigestFinal_ex(ctx2, ripemd_hash, &ripemdlen);
    EVP_MD_CTX_free(ctx2);

    // ----生成地址 payload和校验码----
    unsigned char payload[21];
    unsigned char checksum1[32], checksum2[32], final[25];
    payload[0] = 0xA1; 

    memcpy(payload + 1, ripemd_hash, 20);
    SHA256(payload, 21, checksum1);
    SHA256(checksum1, 32, checksum2);

    // 最终字节序列 = payload + 校验码前 4 字节
    memcpy(final, payload, 21);
    memcpy(final + 21, checksum2, 4);

    Base58check_encode(final, 25, addr_out, addr_out_len);
    secp256k1_context_destroy(ctx);

    return 1;
}

// ----计算交易哈希----
void tx_hash(const Tx* tx, unsigned char hash_out[32]) {

    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL);

    // 对每个输入进行摘要
    for (uint32_t i = 0; i < tx->input_count; i++) {
        EVP_DigestUpdate(mdctx, tx->inputs[i].txid, 32);

        EVP_DigestUpdate(mdctx, &tx->inputs[i].output_index, sizeof(uint32_t));
    }

    // 对每个输出进行摘要
    for (uint32_t i = 0; i < tx->output_count; i++) {
        EVP_DigestUpdate(mdctx, tx->outputs[i].addr, strlen(tx->outputs[i].addr));
        EVP_DigestUpdate(mdctx, &tx->outputs[i].amount, sizeof(uint32_t));
    }

    unsigned int outlen;

    EVP_DigestFinal_ex(mdctx, hash_out, &outlen);


    EVP_MD_CTX_free(mdctx);
}


//----交易输入签名----
// 对交易 tx 的每个输入使用私钥 priv 进行签名
// 返回 1 表示成功，0 表示失败
int sign_tx(Tx* tx, const unsigned char* priv) {
    if (!tx || !priv) return 0;

    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
    if (!ctx) return 0;

    // 验证私钥
    if (!secp256k1_ec_seckey_verify(ctx, priv)) {
        secp256k1_context_destroy(ctx);
        return 0;
    }

    unsigned char hash[32];
    tx_hash(tx, hash);

    for (uint32_t i = 0; i < tx->input_count; i++) {
        secp256k1_ecdsa_signature sig;
        if (!secp256k1_ecdsa_sign(ctx, &sig, hash, priv, NULL, NULL)) {
            secp256k1_context_destroy(ctx);
            return 0;
        }
        // 将签名序列化为 compact 格式,然后保存
        secp256k1_ecdsa_signature_serialize_compact(ctx, tx->inputs[i].signature, &sig);
        tx->inputs[i].sig_len = 64;

        // 公钥
        secp256k1_pubkey pub;
        if (!secp256k1_ec_pubkey_create(ctx, &pub, priv)) {//没能正确创建公钥
            secp256k1_context_destroy(ctx);
            return 0; 
        }

        // 序列化公钥
        size_t publen = 65;
        if (!secp256k1_ec_pubkey_serialize(ctx, tx->inputs[i].pubkey, &publen, &pub, SECP256K1_EC_UNCOMPRESSED)) {//没能正确序列化
            secp256k1_context_destroy(ctx);
            return 0; 
        }
        tx->inputs[i].pubkey_len = publen;
    }

    //释放内存
    secp256k1_context_destroy(ctx);
    return 1;
}




// ----验证交易签名----
// 验证交易 tx 的每个输入是否由对应私钥签名
// 返回 1 表示所有签名有效，0 表示有无效签名
int verify_tx(const Tx* tx) {
    if (!tx) return 0;

    secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_VERIFY);
    if (!ctx) return 0;

    // 计算交易哈希（交易所有输入和输出的 SHA256 摘要）
    unsigned char hash[32];
    tx_hash(tx, hash);

    // 遍历交易的每个输入
    for (uint32_t j = 0; j < tx->input_count; j++) {
        secp256k1_pubkey pub;
        // 解析公钥
        if (!secp256k1_ec_pubkey_parse(ctx, &pub, tx->inputs[j].pubkey, tx->inputs[j].pubkey_len)) {
            secp256k1_context_destroy(ctx);
            return 0;
        }
        // 解析签名
        secp256k1_ecdsa_signature sig;
        if (!secp256k1_ecdsa_signature_parse_compact(ctx, &sig, tx->inputs[j].signature)) {
            secp256k1_context_destroy(ctx);
            return 0;
        }
        if (!secp256k1_ecdsa_verify(ctx, &sig, hash, &pub)) {
            secp256k1_context_destroy(ctx);
            return 0;
        }
    }
    secp256k1_context_destroy(ctx);
    return 1;
}






/*
int wallet_privkey_to_wif(const Wallet* w, char* out, size_t out_len)
{
    if (!w || !out || out_len < 60) return -1;

    uint8_t buf[34];
    buf[0] = 0x80;                 // mainnet prefix
    memcpy(buf + 1, w->privkey, 32);
    buf[33] = 0x01;                // compressed pubkey flag

    // double sha256 checksum
    uint8_t hash1[32], hash2[32];
    sha256(buf, 34, hash1);
    sha256(hash1, 32, hash2);

    uint8_t full[34 + 4];
    memcpy(full, buf, 34);
    memcpy(full + 34, hash2, 4);

    // base58
    if (!base58_encode(full, 34 + 4, out, out_len))
        return -2;

    return 0;
}
int wallet_privkey_from_wif(Wallet* w, const char* wif)
{
    if (!w || !wif) return -1;

    uint8_t raw[64];
    size_t raw_len = sizeof(raw);

    if (!base58_decode(wif, raw, &raw_len)) {
        return -2;  // invalid base58
    }

    if (raw_len != 38) {
        return -3;  // must be 1 + 32 + 1 + 4 = 38 bytes
    }

    if (raw[0] != 0x80) {
        return -4;  // wrong version
    }

    if (raw[33] != 0x01) {
        return -5;  // must be compressed flag
    }

    // verify checksum
    uint8_t hash1[32], hash2[32];
    sha256(raw, 34, hash1);
    sha256(hash1, 32, hash2);

    if (memcmp(hash2, raw + 34, 4) != 0) {
        return -6;  // checksum error
    }

    memcpy(w->privkey, raw + 1, 32);

    // regenerate pubkey + address
    return wallet_from_privkey(w, w->privkey);
}
void wallet_print(const Wallet* w)
{
    if (!w) return;

    char privhex[65];
    wallet_privkey_to_hex(w, privhex);

    char wif[64];
    wallet_privkey_to_wif(w, wif, sizeof(wif));

    printf("=== Wallet ===\n");
    printf("Address  : %s\n", w->address);
    printf("PrivKey  : %s\n", privhex);
    printf("WIF      : %s\n", wif);

    char pubhex[67];
    bytes_to_hex_static(w->pubkey, 33, pubhex);
    printf("PubKey   : %s\n", pubhex);
}
*/