#include "wallet.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include "../crypto/crypto_tools.h" // hash160, pubkey_to_address
#include "../crypto/double_sha256.h"
#include "../crypto/sha256.h"
#include "../crypto/ripemd160.h"
#include "../crypto/base58.h"
#ifdef USE_SECP256K1
#include <secp256k1.h>

static secp256k1_context* secp_ctx = NULL;
static int secp_init_once(void) {
    if (secp_ctx) return 0;
    secp_ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    return secp_ctx ? 0 : -1;
}
#endif

// helper: bytes -> hex
static void bytes_to_hex_static(const uint8_t* in, size_t len, char* out) {
    static const char* hex = "0123456789abcdef";
    for (size_t i = 0; i < len; ++i) {
        out[i * 2] = hex[(in[i] >> 4) & 0xF];
        out[i * 2 + 1] = hex[in[i] & 0xF];
    }
    out[len * 2] = '\0';
}

// Convert hex char to integer 0-15
static int hex_val(char c) {
    if ('0' <= c && c <= '9') return c - '0';
    if ('a' <= c && c <= 'f') return c - 'a' + 10;
    if ('A' <= c && c <= 'F') return c - 'A' + 10;
    return -1;
}

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
}

// generate 32 bytes random from /dev/urandom
static int random32(uint8_t out[32]) {
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) return -1;
    ssize_t r = read(fd, out, 32);
    close(fd);
    return (r == 32) ? 0 : -1;
}

// derive compressed pubkey from privkey using secp256k1 (if enabled)
static int derive_pubkey_compressed(const uint8_t priv[32], uint8_t pubout[33]) {
#ifdef USE_SECP256K1
    if (secp_init_once() != 0) return -1;
    secp256k1_pubkey pub;
    if (!secp256k1_ec_pubkey_create(secp_ctx, &pub, priv)) return -1;
    unsigned char out[33];
    size_t outlen = 33;
    secp256k1_ec_pubkey_serialize(secp_ctx, out, &outlen, &pub, SECP256K1_EC_COMPRESSED);
    if (outlen != 33) return -1;
    memcpy(pubout, out, 33);
    return 0;
#else
    // 没有启用 libsecp256k1：则无法产生公钥
    (void)priv; (void)pubout;
    return -1;
#endif
}

int wallet_create(Wallet* w) {
    if (!w) return -1;
    if (random32(w->privkey) != 0) return -1;

    if (derive_pubkey_compressed(w->privkey, w->pubkey) != 0) {
        // 如果没有可用的 secp256k1，返回错误（提示用户）
        fprintf(stderr, "ERROR: libsecp256k1 not available — compile with -DUSE_SECP256K1 and link -lsecp256k1\n");
        return -2;
    }

    // 用你已有的整合接口生成 address
    pubkey_to_address(w->pubkey, w->address);
    return 0;
}

int wallet_from_privkey(Wallet* w, const uint8_t privkey[32]) {
    if (!w || !privkey) return -1;
    memcpy(w->privkey, privkey, 32);
    if (derive_pubkey_compressed(w->privkey, w->pubkey) != 0) {
        fprintf(stderr, "ERROR: libsecp256k1 not available — cannot derive pubkey\n");
        return -2;
    }
    pubkey_to_address(w->pubkey, w->address);
    return 0;
}

void wallet_privkey_to_hex(const Wallet* w, char out[WALLET_KEY_HEX_LEN + 1]) {
    if (!w || !out) return;
    bytes_to_hex_static(w->privkey, 32, out);
}

int wallet_privkey_from_hex(Wallet* w, const char hex[WALLET_KEY_HEX_LEN]) {
    if (!w || !hex) return -1;
    if (hex_to_bytes(hex, w->privkey, 32) != 0) return -1;
    return wallet_from_privkey(w, w->privkey);
}

int wallet_save_keystore(const Wallet* w, const char* path) {
    if (!w || !path) return -1;
    char keyhex[WALLET_KEY_HEX_LEN + 1];
    wallet_privkey_to_hex(w, keyhex);

    FILE* f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "{\n");
    fprintf(f, "  \"address\": \"%s\",\n", w->address);
    fprintf(f, "  \"privkey_hex\": \"%s\"\n", keyhex);
    fprintf(f, "}\n");
    fclose(f);
    return 0;
}

int wallet_load_keystore(Wallet* w, const char* path) {
    if (!w || !path) return -1;
    FILE* f = fopen(path, "r");
    if (!f) return -1;
    // 解析 JSON
    char buf[512];
    char address[64] = { 0 };
    char keyhex[WALLET_KEY_HEX_LEN + 1] = { 0 };
    while (fgets(buf, sizeof(buf), f)) {

        // parse address
        if (strstr(buf, "address")) {
            sscanf(buf, " %*[^:] : \"%63[^\"]\"", address);
        }

        // parse privkey_hex
        else if (strstr(buf, "privkey_hex")) {
            sscanf(buf, " %*[^:] : \"%64[^\"]\"", keyhex);
        }
    }

    fclose(f);

    if (keyhex[0] == '\0') {
        return -2; // no private key found
    }

    // load private key
    if (wallet_privkey_from_hex(w, keyhex) != 0)
        return -3;

    // load address if present
    if (address[0]) {
        strlcpy(w->address, address, sizeof(w->address));

    }
    else {
        // rebuild from pubkey
        pubkey_to_address(w->pubkey, w->address);
    }

    return 0;
}

int wallet_sign(const Wallet* w, const uint8_t hash32[32],
    uint8_t sig_out[72], size_t* sig_len_out)
{
#ifdef USE_SECP256K1
    if (!w || !hash32 || !sig_out || !sig_len_out) return -1;
    if (secp_init_once() != 0) return -2;

    secp256k1_ecdsa_signature sig;

    if (!secp256k1_ecdsa_sign(secp_ctx, &sig, hash32, w->privkey,
        secp256k1_nonce_function_rfc6979, NULL)) {
        return -3;
    }

    size_t len = 72;
    if (!secp256k1_ecdsa_signature_serialize_der(secp_ctx, sig_out, &len, &sig)) {
        return -4;
    }

    *sig_len_out = len;
    return 0;
#else
    (void)w; (void)hash32; (void)sig_out; (void)sig_len_out;
    return -99; // libsecp256k1 未开启
#endif
}

int wallet_verify(const uint8_t pubkey[33],
    const uint8_t hash32[32],
    const uint8_t* sig, size_t sig_len)
{
#ifdef USE_SECP256K1
    if (!pubkey || !hash32 || !sig) return -1;
    if (secp_init_once() != 0) return -2;

    secp256k1_pubkey pk;
    if (!secp256k1_ec_pubkey_parse(secp_ctx, &pk, pubkey, 33)) {
        return -3;
    }

    secp256k1_ecdsa_signature s;
    if (!secp256k1_ecdsa_signature_parse_der(secp_ctx, &s, sig, sig_len)) {
        return -4;
    }

    return secp256k1_ecdsa_verify(secp_ctx, &s, hash32, &pk) ? 0 : -5;
#else
    (void)pubkey; (void)hash32; (void)sig; (void)sig_len;
    return -99;
#endif
}

// ------------------------
// wallet_get_pubkey_hash
// ------------------------
int wallet_get_pubkey_hash(const Wallet* w, uint8_t out20[20]) {
    if (!w || !out20) return -1;
    uint8_t hash[32];
    sha256(w->pubkey, 33, hash);      // SHA256(pubkey)
    ripemd160(hash, 32, out20);       // RIPEMD160(SHA256(pubkey))
    return 0;
}

// ------------------------
// wallet_build_p2pkh_scriptpubkey
// ------------------------
int wallet_build_p2pkh_scriptpubkey(const Wallet* w, uint8_t* script_out, size_t* script_len_out) {
    if (!w || !script_out || !script_len_out) return -1;

    uint8_t pubkey_hash[20];
    if (wallet_get_pubkey_hash(w, pubkey_hash) != 0) return -2;

    // OP_DUP OP_HASH160 20 <pubkeyhash> OP_EQUALVERIFY OP_CHECKSIG
    size_t idx = 0;
    script_out[idx++] = 0x76;            // OP_DUP
    script_out[idx++] = 0xa9;            // OP_HASH160
    script_out[idx++] = 0x14;            // push 20 bytes
    memcpy(script_out + idx, pubkey_hash, 20);
    idx += 20;
    script_out[idx++] = 0x88;            // OP_EQUALVERIFY
    script_out[idx++] = 0xac;            // OP_CHECKSIG

    *script_len_out = idx;
    return 0;
}

// ------------------------
// wallet_build_p2pkh_scriptsig
// ------------------------
int wallet_build_p2pkh_scriptsig(
    const Wallet* w,
    const uint8_t sighash32[32],
    uint8_t* scriptSig_out,
    size_t* scriptSig_len_out)
{
#ifdef USE_SECP256K1
    if (!w || !sighash32 || !scriptSig_out || !scriptSig_len_out) return -1;

    uint8_t sig[72];
    size_t sig_len;
    if (wallet_sign(w, sighash32, sig, &sig_len) != 0) return -2;

    // scriptSig = <sig+SIGHASH_ALL> <pubkey>
    size_t idx = 0;

    // push signature (append SIGHASH_ALL 0x01)
    sig[sig_len++] = 0x01; // SIGHASH_ALL
    scriptSig_out[idx++] = (uint8_t)sig_len;
    memcpy(scriptSig_out + idx, sig, sig_len);
    idx += sig_len;

    // push compressed pubkey
    scriptSig_out[idx++] = 33; // pubkey length
    memcpy(scriptSig_out + idx, w->pubkey, 33);
    idx += 33;

    *scriptSig_len_out = idx;
    return 0;
#else
    (void)w; (void)sighash32; (void)scriptSig_out; (void)scriptSig_len_out;
    return -99;
#endif
}

/*
    简化版本：只处理 SIGHASH_ALL，假设 tx_raw 已经完整序列化
    input_index 指定签名输入，其他输入的 script 置空
    script_pubkey 填入要签名输入的 scriptPubKey

int wallet_compute_sighash_p2pkh(
    const uint8_t* tx_raw, size_t tx_len,
    size_t input_index,
    const uint8_t* script_pubkey, size_t script_pubkey_len,
    uint32_t sighash_type,
    uint8_t out_hash32[32])
{
    if (!tx_raw || !script_pubkey || !out_hash32) return -1;
    // 这里演示一个简单方案：构造 tx_copy，替换输入的 script
    // 对实验用最小交易即可。完整实现需解析序列化交易，复杂一些
    // 为实验，这里直接 double_sha256(tx_raw || sighash_type)
    size_t buf_len = tx_len + 4;
    uint8_t* buf = (uint8_t*)malloc(buf_len);
    if (!buf) return -2;
    memcpy(buf, tx_raw, tx_len);
    buf[tx_len + 0] = sighash_type & 0xFF;
    buf[tx_len + 1] = (sighash_type >> 8) & 0xFF;
    buf[tx_len + 2] = (sighash_type >> 16) & 0xFF;
    buf[tx_len + 3] = (sighash_type >> 24) & 0xFF;

    double_sha256(buf, buf_len, out_hash32);
    free(buf);
    return 0;
}*/

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
