#ifndef TX_POOL_H
#define TX_POOL_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "core/transaction.h"
#include "core/utxo_set.h"
#include "wallet/wallet.h"   // 用于签名验证等功能



typedef struct MempoolTx {
    Tx* tx;                         // 交易内容
    unsigned char txid[32];         // 交易标识
    struct MempoolTx* next;
} MempoolTx;

typedef struct {
    MempoolTx* head;
} Mempool;

//初始化
void tx_pool_init(Mempool* pool);

//添加交易
bool tx_pool_add_tx(Mempool* pool, Tx* tx, UTXO* utxo_set);

//删除交易
bool tx_pool_remove_tx(Mempool* pool, const unsigned char txid[32]);
void tx_pool_print(const Mempool* pool);
#endif
