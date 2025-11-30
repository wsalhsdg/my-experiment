#include "tx_pool.h"
#include <string.h>
#include <stdio.h>
#include "utxo_set.h"

void txpool_init(TxPool* pool) {
    pool->count = 0;
}



Transaction* txpool_find(TxPool* pool, const uint8_t txid[TXID_LEN]) {
    for (size_t i = 0; i < pool->count; i++) {
        if (memcmp(pool->txs[i].txid, txid, TXID_LEN) == 0) {
            return &pool->txs[i];
        }
    }
    return NULL;
}

int txpool_remove(TxPool* pool, const uint8_t txid[TXID_LEN]) {
    for (size_t i = 0; i < pool->count; i++) {
        if (memcmp(pool->txs[i].txid, txid, TXID_LEN) == 0) {
            // 用最后一个覆盖，提高效率
            pool->txs[i] = pool->txs[pool->count - 1];
            pool->count--;
            return 1;
        }
    }
    return 0;
}

// 内部：验证交易是否有效（余额、双花）
// 返回 1 有效，0 无效
static int txpool_tx_validate(const Transaction* tx, const UTXOSet* utxos)
{
    // --- Coinbase 特殊处理：prev_txid 全 0、vout=0 ---
    int is_coinbase = 1;
    for (int i = 0; i < TXID_LEN; i++) {
        if (tx->prev_txid[i] != 0) {
            is_coinbase = 0;
            break;
        }
    }
    if (is_coinbase && tx->vout == 0) {
        return 1;   // coinbase 一定合法
    }

    // 从 UTXO 集中查找输入引用的输出
    UTXO* input = utxo_set_find((UTXOSet*)utxos, tx->prev_txid, tx->vout);
    if (!input) {
        printf("[TxPool] Invalid TX: input UTXO not found (double spend?)\n");
        return 0;
    }

    // 计算所有输出金额
    uint64_t total_out = 0;
    for (size_t i = 0; i < tx->output_count; i++)
        total_out += tx->outputs[i].value;

    // 输入金额不足
    if (total_out > input->value) {
        printf("[TxPool] Invalid TX: insufficient input amount\n");
        return 0;
    }

    return 1;
}

int txpool_add(TxPool* pool, const Transaction* tx, const UTXOSet* utxos)
{
    if (pool->count >= MAX_TX_POOL_SIZE) {
        printf("[TxPool] Pool full!\n");
        return 0;
    }

    // 检查是否已经存在（防止重复 / 双花）
    if (txpool_find(pool, tx->txid)) {
        printf("[TxPool] Already exists\n");
        return 0;
    }

    // 验证交易
    if (!txpool_tx_validate(tx, utxos)) {
        printf("[TxPool] Validation failed\n");
        return 0;
    }

    // 添加到池
    pool->txs[pool->count] = *tx;
    pool->count++;

    printf("[TxPool] Added tx ");
    for (int i = 0; i < 4; i++)
        printf("%02x", tx->txid[i]);
    printf("...\n");

    return 1;
}

void txpool_print(const TxPool* pool) {
    printf("\n=== Transaction Pool (%zu txs) ===\n", pool->count);
    for (size_t i = 0; i < pool->count; i++) {
        printf("Tx %zu: ", i);
        for (int j = 0; j < 4; j++) {
            printf("%02x", pool->txs[i].txid[j]);
        }
        printf("...\n");
    }
}
