#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "crypto/crypto_tools.h"

#define MAX_SCRIPT_SIZE 25
#define MAX_TXOUT_COUNT 16
#define TXID_LEN 32
#define MAX_TX_OUTPUTS 16

#ifndef TXID_LEN
#define TXID_LEN 32
#endif

#ifndef BTC_ADDRESS_MAXLEN
#define BTC_ADDRESS_MAXLEN 64
#endif

#ifndef MAX_TX_OUTPUTS
#define MAX_TX_OUTPUTS 16
#endif

#ifndef MAX_TX_INPUTS
#define MAX_TX_INPUTS 16
#endif

#ifndef MAX_PUBKEY_LEN
#define MAX_PUBKEY_LEN 33
#endif

#ifndef MAX_SIG_LEN
#define MAX_SIG_LEN 128
#endif

typedef struct {
    uint64_t value;
    char address[BTC_ADDRESS_MAXLEN];
} TxOut;

/* 单个交易输入（引用一个 UTXO） */
typedef struct {
    uint8_t txid[TXID_LEN];
    uint32_t vout;
} TxIn;

typedef struct {
    size_t input_count;
    TxIn inputs[MAX_TX_INPUTS];

    size_t output_count;
    TxOut outputs[MAX_TX_OUTPUTS];

    /* 交易 id */
    uint8_t txid[TXID_LEN];

    /* 签名相关：每个输入对应一个公钥和一个签名（灵活支持每输入不同签名）*/
    uint8_t pubkeys[MAX_TX_INPUTS][MAX_PUBKEY_LEN];
    uint8_t signatures[MAX_TX_INPUTS][MAX_SIG_LEN];
    size_t sig_lens[MAX_TX_INPUTS];
} Transaction;

/* 初始化多输入交易 (inputs 可以为 NULL 表示先不添加输入) */
void transaction_init(Transaction* tx,
    const TxIn* inputs,
    size_t input_count,
    const TxOut* outputs,
    size_t output_count);

/* 初始化带找零的多输入交易（total_input_value 为所有输入金额之和） */
void transaction_init_with_change(Transaction* tx,
    const TxIn* inputs,
    size_t input_count,
    const TxOut* outputs,
    size_t output_count,
    uint64_t total_input_value,
    const char* change_address);

/* 计算 txid（double SHA256） */
void transaction_compute_txid(Transaction* tx);

/* 打印交易（调试） */
void transaction_print(const Transaction* tx);

/* 对指定输入 index 生成签名消息（32字节） */
void transaction_hash_for_sign(const Transaction* tx, size_t in_index, uint8_t out32[32]);

/* 对交易的所有输入使用 privkey 签名（会为每个输入生成签名并写入 tx->signatures[in], pubkey[in]） */
int transaction_sign(Transaction* tx, const uint8_t* privkey32);

/* 验证交易：对每个输入使用 tx 中的 pubkey+signature 验证
   返回 1 = 全部验证通过，0 = 有任意一个失败 */
int transaction_verify(const Transaction* tx);

/* 序列化 / 反序列化 */
size_t transaction_serialize(const Transaction* tx, uint8_t* out, size_t maxlen);
int transaction_deserialize(Transaction* tx, const uint8_t* data, size_t len);


/*
// 单个交易输出
typedef struct {
    uint64_t value;                      // 金额（聪）
    char address[BTC_ADDRESS_MAXLEN];    // 接收地址
} TxOut;

// 交易结构（单输入+多输出最简版）
typedef struct {
    uint8_t txid[TXID_LEN];             // 交易ID
    uint8_t prev_txid[TXID_LEN];        // 输入引用的上一笔交易
    uint32_t vout;                       // 输入引用的输出索引
    TxOut outputs[MAX_TX_OUTPUTS];       // 输出数组
    size_t output_count;                 // 输出数量
    uint8_t pubkey[33];       // 压缩公钥
    uint8_t signature[72];    // DER 编码签名（最大 72 字节）
    size_t  sig_len;          // 签名实际长度

} Transaction;

// 初始化交易（指定输入和多个输出）
void transaction_init(Transaction* tx,
    const uint8_t prev_txid[TXID_LEN],
    uint32_t vout,
    const TxOut* outputs,
    size_t output_count);

void transaction_init_with_change(Transaction* tx,
    const uint8_t prev_txid[TXID_LEN],
    uint32_t vout,
    const TxOut* outputs,
    size_t output_count,
    uint64_t total_input_value,
    const char* change_address);

// 计算交易 TxID（double SHA256）
void transaction_compute_txid(Transaction* tx);

// 打印交易信息（调试用）
void transaction_print(const Transaction* tx);

// 使用私钥签名交易
int transaction_sign(Transaction* tx, const uint8_t* privkey32);

// 验证交易签名
int transaction_verify(const Transaction* tx);


*/