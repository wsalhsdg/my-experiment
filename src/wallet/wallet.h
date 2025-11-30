#pragma once
#include <stdint.h>
#include <stddef.h>

#define WALLET_ADDR_MAX 50
#define WALLET_KEY_HEX_LEN 64

typedef struct {
    uint8_t privkey[32];           // raw 32-byte private key
    uint8_t pubkey[33];            // compressed pubkey (33 bytes)
    char address[WALLET_ADDR_MAX]; // Base58Check P2PKH address
} Wallet;

/**
 * 产生一个新的钱包：生成私钥 -> 生成 pubkey -> 生成 address
 * 返回 0 成功，非0失败
 *
 * 需要 libsecp256k1 可用用于 pubkey 生成。
 */
int wallet_create(Wallet* w);

/**
 * 从 32 字节私钥加载并填充 Wallet（计算 pubkey/address）
 * privkey: 32字节
 * 返回 0 成功
 */
int wallet_from_privkey(Wallet* w, const uint8_t privkey[32]);

/**
 * 把私钥以 hex（64字节 ASCII）形式写入 out（带终止 '\0'），out 长度至少 65
 */
void wallet_privkey_to_hex(const Wallet* w, char out[WALLET_KEY_HEX_LEN + 1]);

/**
 * 从 hex 私钥字符串加载（长度 64），返回 0 成功
 */
int wallet_privkey_from_hex(Wallet* w, const char hex[WALLET_KEY_HEX_LEN]);

/**
 * 将钱包保存到文件（JSON），包含 address 与私钥 hex（便于老师查看）
 * path 可以是相对或绝对路径。返回 0 成功
 */
int wallet_save_keystore(const Wallet* w, const char* path);

/**
 * 从 keystore 加载钱包（JSON），返回 0 成功
 */
int wallet_load_keystore(Wallet* w, const char* path);

int wallet_sign(const Wallet* w,//使用钱包私钥对 32 字节哈希进行 secp256k1 ECDSA 签名
    const uint8_t hash32[32],
    uint8_t sig_out[72],
    size_t* sig_len_out);

int wallet_verify(const uint8_t pubkey[33],//验证公钥对哈希的签名
    const uint8_t hash32[32],
    const uint8_t* sig,
    size_t sig_len);

// 获取钱包公钥的 HASH160（20 字节）
int wallet_get_pubkey_hash(const Wallet* w, uint8_t out20[20]);

// 构建 P2PKH 输出脚本（scriptPubKey）
// script_out: 输出缓冲区
// script_len_out: 返回实际长度
int wallet_build_p2pkh_scriptpubkey(const Wallet* w, uint8_t* script_out, size_t* script_len_out);

// 构建 P2PKH 输入脚本（scriptSig）
// 使用钱包私钥对给定的 32 字节 sighash 签名
// scriptSig_out: 输出缓冲区
// scriptSig_len_out: 返回实际长度
int wallet_build_p2pkh_scriptsig(
    const Wallet* w,
    const uint8_t sighash32[32],
    uint8_t* scriptSig_out,
    size_t* scriptSig_len_out);




// 将私钥导出为比特币 WIF 格式字符串
 
int wallet_privkey_to_wif(const Wallet* w, char* out, size_t out_len);

//从 WIF 导入钱包（恢复 privkey/pukey/address）
int wallet_privkey_from_wif(Wallet* w, const char* wif);

//打印钱包内容（调试用）
void wallet_print(const Wallet* w);