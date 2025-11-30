#pragma once
#include <stdint.h>
#include <stddef.h>
#include "transaction.h"
#include "utxo_set.h" 

#define MAX_TX_POOL_SIZE 1024   // 最大交易池容量

typedef struct {
    Transaction txs[MAX_TX_POOL_SIZE];
    size_t count;
} TxPool;

// 初始化交易池
void txpool_init(TxPool* pool);

// 添加交易（返回 1 成功，0 失败）
int txpool_add(TxPool* pool, const Transaction* tx, const UTXOSet* utxos);

// 通过 txid 查找交易（返回 Transaction* 或 NULL）
Transaction* txpool_find(TxPool* pool, const uint8_t txid[TXID_LEN]);

// 删除交易（返回 1 成功）
int txpool_remove(TxPool* pool, const uint8_t txid[TXID_LEN]);

// 打印交易池
void txpool_print(const TxPool* pool);
