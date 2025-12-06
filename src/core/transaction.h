#ifndef TRANSACTION_H
#define TRANSACTION_H
#include <stdint.h>
#include <stddef.h>
#include <string.h>
//#include "crypto/crypto_tools.h"


// ----交易输出----
typedef struct {
    char addr[36];       // 交易的接收方的地址
    uint32_t amount;     // 交易金额
} TxOut;

/*
// 单个交易输出
typedef struct {
    uint64_t value;                      // 金额
    char address[BTC_ADDRESS_MAXLEN];    // 接收地址
} TxOut;*/

// ----交易输入----
typedef struct {
    unsigned char txid[32];        // 引用的前一笔交易的哈希 (32 bytes)
    uint32_t output_index;         

    unsigned char signature[64];   // 签名
    size_t sig_len;                // 签名长度

    unsigned char pubkey[65];      // 公钥（用于验证）
    size_t pubkey_len;             // 公钥长度
} TxIn;


// ----交易----
typedef struct {
    TxIn* inputs;                   //输入
    uint32_t input_count;           //输入数量

    TxOut* outputs;                 //输出
    uint32_t output_count;          //输出数量

    unsigned char txid[32];         //当前交易自身的哈希（交易ID）
} Tx;


/**
 * 初始化交易结构（将计数置 0，将指针置 NULL）
 * 不分配任何内存，只保证安全的初始状态。
 */
void transaction_init(Tx* tx);

/**
 * 添加一个输入到交易中
 * - prev_txid : 被引用交易的 txid（32 字节）
 * - index     : 引用的输出索引
 * 自动扩容 inputs 数组（malloc/realloc）
 */
void add_txin(Tx* tx, const unsigned char txid[32], uint32_t index);

/**
 * 添加一个输出到交易中
 * - addr   : 接收地址（字符串）
 * - amount : 支付金额
 * 自动扩容 outputs 数组（malloc/realloc）
 */
void add_txout(Tx* tx, const char* addr, uint32_t amount);

// 释放交易内存
void free_tx(Tx* tx);

/* 打印交易（调试） */
//void transaction_print(const Transaction* tx);

#endif // TRANSACTION_H

