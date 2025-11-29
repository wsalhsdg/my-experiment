#pragma once
#include <stdint.h>
#include <stddef.h>
#include "crypto/crypto_tools.h"


#define MAX_SCRIPT_SIZE 72   // 署名 + pubkey 最大长度
#define MAX_TXOUT_COUNT 16
#define MAX_TXIN_COUNT 16

// 交易输入
typedef struct {
    uint8_t prevTxid[32];       // 被花费的交易ID
    uint32_t prevIndex;         // 被花费的输出索引
    uint8_t scriptSig[MAX_SCRIPT_SIZE]; // 签名 + 公钥
    size_t scriptSigLen;
    uint32_t sequence;
} TxIn;

// 交易输出
typedef struct {
    uint64_t value;                // 聪
    uint8_t scriptPubKey[25];      // P2PKH 脚本
    size_t scriptLen;
    char address[BTC_ADDRESS_MAXLEN];
} TxOut;

// 交易
typedef struct {
    int32_t version;
    size_t VinCount;
    TxIn vin[MAX_TXIN_COUNT];
    size_t VoutCount;
    TxOut vout[MAX_TXOUT_COUNT];
    uint32_t locktime;
} Transaction;

// 初始化 TxOut
void txout_init(TxOut* txout, uint64_t value, const uint8_t pubkey[33]);

// 初始化 TxIn（占位）
void txin_init(TxIn* txin, const uint8_t prevTxid[32], uint32_t prevIndex);

// 初始化 Transaction
void transaction_init(Transaction* tx, int32_t version, uint32_t locktime);

// 序列化交易（无见证）到 buf，返回长度
size_t serialize_tx(const Transaction* tx, uint8_t* buf);

// 生成 SIGHASH_ALL 哈希（输入索引）
void sighash_all(const Transaction* tx, size_t input_index, uint8_t out[32]);

// 签名某个输入
void tx_sign_input(Transaction* tx, size_t input_index, const uint8_t privkey[32]);

// 计算完整 TxID（双 SHA256 序列化）
void txid(const Transaction* tx, uint8_t out[32]);