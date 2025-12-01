#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "crypto/crypto_tools.h"

#define MAX_SCRIPT_SIZE 25
#define MAX_TXOUT_COUNT 16
#define TXID_LEN 32
#define MAX_TX_OUTPUTS 16

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


