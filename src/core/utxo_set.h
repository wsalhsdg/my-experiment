#ifndef UTXO_SET_H
#define UTXO_SET_H
#include <stddef.h>
#include <stdint.h>
#include "core/transaction.h"

// ----UTXO结构----
typedef struct UTXONode {
    unsigned char txid[32];     // 交易ID
    uint32_t output_index;      // 输出索引
    uint32_t amount;            // 交易金额
    char addr[128];              // 收款地址
    struct UTXONode* next;      // 下一个节点
} UTXO;

/*typedef struct {
    UTXO set[MAX_UTXOS];
    size_t count;
} UTXOSet;*/

typedef struct {
    const UTXO* utxos[64];      // 收集到的指针
    int count;                  //UTXO数量
    uint64_t total;             //总金额
} CoinSelection;

//int Select_coins(const UTXO* utxo_set, const char* addr, uint64_t amount, CoinSelection* result);

// ----向链表头部添加一个新的 UTXO 节点----
void add_utxo(UTXO** utxo_set, const unsigned char txid[32], uint32_t index, const char* addr, uint32_t amount);


// ----查找 UTXO----
UTXO* find_utxo(UTXO* utxo_set, const unsigned char txid[32], uint32_t index);

// ----移除 UTXO----
void remove_utxo(UTXO** utxo_set, const unsigned char txid[32], uint32_t index);

// ---更新 UTXO 集----
int update_utxo_set(UTXO** utxo_set, const Tx* tx, const unsigned char txid[32]);

// ----打印UTXO列表----
void print_utxo_set(const UTXO* utxo_set);

// ----确认余额是否足够----
int has_sufficient_balance(const UTXO* utxo_set, const char* from_addr, uint64_t amount);

// ----选币----
int select_coins(const UTXO* utxo_set, const char* addr, uint64_t amount, CoinSelection* result);

// ----余额查询----
uint64_t get_balance(const UTXO* utxo_set, const char* addr);

#endif

