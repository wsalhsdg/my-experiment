#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "core/tx_pool.h"


/*void txpool_init(TxPool* pool) {
    pool->count = 0;
}*/


/* 打印交易池 */
void tx_pool_print(const Mempool* pool) {

    printf("===== Tx_pool Transactions =====\n");

    const MempoolTx* cur = pool->head;
    if (!cur) {
        printf("(empty)\n");
        return;
    }

    while (cur) {
        printf("  txid: ");
        for (int i = 0; i < 32; i++) {
            printf("%02X", cur->txid[i]);
        }
        printf("\n");

        cur = cur->next;
    }
}

// 初始化交易池
void tx_pool_init(Mempool* pool) {
    pool->head = NULL;             //将链表头指针设置为 NULL
}



// 是否已有相同 txid 的交易
static bool mempool_alreadyhave(Mempool* pool, const unsigned char txid[32]) {
    MempoolTx* cur = pool->head;

    while (cur) {
        if (memcmp(cur->txid, txid, 32) == 0)
            return true;
        cur = cur->next;
    }
    return false;
}

// 添加交易 
bool tx_pool_add_tx(Mempool* pool, Tx* tx, UTXO* utxo_set) {
    unsigned char txid[32];
    tx_hash(tx, txid);

    /* ------- 1. 检查重复交易 ------- */
    if (mempool_alreadyhave(pool, txid)) {
        printf("[Mempool] Duplicate transaction rejected.\n");
        return false;
    }

    /* ------- 2. 验证交易签名 ------- */
    if (!verify_tx(tx)) {
        printf("[Mempool] Invalid signature, rejected.\n");
        return false;
    }

    /* ------- 3. 验证所有输入的 UTXO 是否存在 ------- */
    for (uint32_t i = 0; i < tx->input_count; i++) {
        TxIn* in = &tx->inputs[i];
        if (!find_utxo(utxo_set, in->txid, in->output_index)) {
            printf("[Mempool] Input UTXO not found, rejected.\n");
            return false;
        }
    }

    /* ------- 4. 全部检查通过，加入交易池 ------- */
    MempoolTx* node = malloc(sizeof(MempoolTx));
    if (!node) {
        printf("[Mempool] Memory allocation failed!\n");
        return false;
    }
    node->tx = tx;
    memcpy(node->txid, txid, 32);

    /* 插入链表头（O(1) 操作）*/
    node->next = pool->head;
    pool->head = node;

    printf("[Mempool] Tx added.\n");
    return true;
}

// 删除交易
bool tx_pool_remove_tx(Mempool* pool, const unsigned char txid[32]) {
    MempoolTx** cur = &pool->head;
    while (*cur) {
        if (memcmp((*cur)->txid, txid, 32) == 0) {
            MempoolTx* tmp = *cur;
            *cur = (*cur)->next;
            free(tmp);
            return true;
        }
        cur = &(*cur)->next;
    }
    return false;
}

