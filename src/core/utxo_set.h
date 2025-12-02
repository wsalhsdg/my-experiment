#pragma once
#include <stdint.h>
#include <stddef.h>
#include "core/transaction.h"

#define MAX_UTXOS 1024
#define ADDR_LEN 50

typedef struct {
    uint8_t txid[TXID_LEN];    // 交易ID
    uint32_t vout;             // 输出索引
    uint64_t value;            // 输出金额（最小单位，例如聪）
    char address[ADDR_LEN];    // 收款地址
} UTXO;

typedef struct {
    UTXO set[MAX_UTXOS];
    size_t count;
} UTXOSet;

/**
 * 初始化 UTXO 集
 */
void utxo_set_init(UTXOSet* utxos);

/**
 * 添加一个新的 UTXO
 * 返回 0 成功，-1 失败（空间不足）
 */
int utxo_set_add(UTXOSet* utxos, const UTXO* utxo);

/**
 * 删除一个 UTXO
 * 根据 txid + vout 定位
 * 返回 0 成功，-1 未找到
 */
int utxo_set_remove(UTXOSet* utxos, const uint8_t txid[TXID_LEN], uint32_t vout);

/**
 * 根据 txid + vout 查询 UTXO
 * 返回指针（可能为 NULL）
 */
UTXO* utxo_set_find(UTXOSet* utxos, const uint8_t txid[TXID_LEN], uint32_t vout);

/**
 * 打印当前 UTXO 集
 */
void utxo_set_print(const UTXOSet* utxos);

// 自动更新 UTXO 集
// 添加 outputs，删除 inputs
void utxo_set_update_from_tx(UTXOSet* utxos, const Transaction* tx);

/**
 * 查询某地址的余额（所有 UTXO 的 value 求和）
 */
uint64_t utxo_set_get_balance(const UTXOSet* utxos, const char* address);

//选币功能
int utxo_set_select(
    const UTXOSet* utxos,
    const char* address,
    uint64_t amount_needed,
    UTXO* selected,
    size_t max_selected,
    size_t* out_selected_count,
    uint64_t* out_change
);
