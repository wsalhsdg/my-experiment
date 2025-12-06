#include "core/utxo_set.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>



// ----添加UTXO----
void add_utxo(UTXO** utxo_set, const unsigned char txid[32], uint32_t index, const char* addr, uint32_t amount) 
{
    UTXO* node = malloc(sizeof(UTXO)+1);

    if (!node) return;

    memcpy(node->txid, txid, 32);
    node->output_index = index;
    snprintf(node->addr, 128, "%s", addr); 
    node->amount = amount;

    node->next = *utxo_set;     // 新节点挂到链表头

    *utxo_set = node;
}

// ----余额查询----
uint64_t get_balance(const UTXO* utxo_set, const char* addr) {
    uint64_t sum = 0;
    const UTXO* cur = utxo_set;

    while (cur) {
        //printf("cur=%s\n", cur->addr);
        if (strcmp(cur->addr, addr) == 0) {
            sum += cur->amount;
            printf("amount=%d\n", cur->amount);
        }
        
        cur = cur->next;
    }
    return sum;
}

// ----余额是否充足----
int has_sufficient_balance(const UTXO* utxo_set,
    const char* from_addr,
    uint64_t amount)
{
    uint64_t bal = get_balance(utxo_set, from_addr);
    return bal >= amount;
}

// ----查询UTXO----
UTXO* find_utxo(UTXO* utxo_set, const unsigned char txid[32], uint32_t index) 
{
    UTXO* cur = utxo_set;

    while (cur) {
        if (memcmp(cur->txid, txid, 32) == 0 && 
            cur->output_index == index) 
        {
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}


// ----删除UTXO----
void remove_utxo(UTXO** utxo_set, const unsigned char txid[32], uint32_t index) {
    UTXO** cur = utxo_set;

    //遍历整个链表
    while (*cur) {
        if (memcmp((*cur)->txid, txid, 32) == 0 && (*cur)->output_index == index) 
        {
            // 找到节点
            UTXO* tmp = *cur;
            *cur = (*cur)->next;//脱链
            free(tmp);
            return;
        }


        cur = &(*cur)->next;
    }
}

// 更新 UTXO 集
int update_utxo_set(UTXO** utxo_set, const Tx* tx, const unsigned char txid[32]) {
    
    /* ---- Step 1: 删除 Inputs 对应的 UTXO ---- */
    for (uint32_t i = 0; i < tx->input_count; i++) 
    {
        TxIn* in = &tx->inputs[i];

        UTXO* u = find_utxo(*utxo_set, in->txid, in->output_index);
        if (!u) {
            printf("UTXO not found for input!\n");
            return 0; // 输入 UTXO 不存在 → 无效交易
        }
        remove_utxo(utxo_set, in->txid, in->output_index);
    }

    /* ---- Step 2: 添加 Outputs 成为新的 UTXO ---- */
    for (uint32_t i = 0; i < tx->output_count; i++) 
    {
        TxOut* out = &tx->outputs[i];
        add_utxo(utxo_set, txid, i, out->addr, out->amount);
        
    }

    return 1;
}

// ----打印UTXO----
void print_utxo_set(const UTXO* utxo_set) {
    const UTXO* cur = utxo_set;
    printf("UTXO set:\n");
    while (cur) {
        printf("  Addr=%s\n", cur->addr);
        printf("  Amount=%u\n", cur->amount);
        printf("  TxID=");
        for (int i = 0; i < 32; i++) {
            printf("%02X", cur->txid[i]);
        }
        printf("\n  Index=%u\n\n", cur->output_index);
        cur = cur->next;
    }
}

// ----选币----
int select_coins(const UTXO* utxo_set,
                 const char* addr, 
                 uint64_t amount, 
                 CoinSelection* result)
{
    result->count = 0;
    result->total = 0;
    const UTXO* cur = utxo_set;

    while (cur) {
        if (strcmp(cur->addr, addr) == 0) {
            result->utxos[result->count++] = cur;
            result->total += cur->amount;

            if (result->total >= amount) {
                return 1;   
            }
        }
        cur = cur->next;
    }

    return 0; // 余额不足无法选币
}

// ----打印UTXO的id----
/*void print_utxo_ids(const UTXO* utxo_set) {
    const UTXO* cur = utxo_set;

    printf("UTXO IDs:\n");

    while (cur) {
        printf("  TxID=");
        for (int i = 0; i < 32; i++) printf("%02X", cur->txid[i]); //以 32 字节 txid 形式输出，便于比对链状态
        printf(", Index=%u, Address=%s, Amount=%u\n",
            cur->output_index, cur->addr, cur->amount);
        cur = cur->next;
    }
}*/