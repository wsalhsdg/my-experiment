#include "utxo_set.h"
#include <stdio.h>
#include <string.h>


void utxo_set_init(UTXOSet* utxos) {
    if (!utxos) return;
    utxos->count = 0;
    memset(utxos->set, 0, sizeof(utxos->set));
}

int utxo_set_add(UTXOSet* utxos, const UTXO* utxo) {
    if (!utxos || !utxo) return -1;
    if (utxos->count >= MAX_UTXOS) return -1;

    utxos->set[utxos->count++] = *utxo;
    return 0;
}

int utxo_set_remove(UTXOSet* utxos, const uint8_t txid[TXID_LEN], uint32_t vout) {
    if (!utxos || !txid) return -1;

    for (size_t i = 0; i < utxos->count; i++) {
        if (memcmp(utxos->set[i].txid, txid, TXID_LEN) == 0 && utxos->set[i].vout == vout) {
            // 找到 UTXO，删除并移动最后一个元素
            utxos->set[i] = utxos->set[utxos->count - 1];
            utxos->count--;
            return 0;
        }
    }
    return -1;  // 未找到
}

UTXO* utxo_set_find(UTXOSet* utxos, const uint8_t txid[TXID_LEN], uint32_t vout) {
    if (!utxos || !txid) return NULL;

    for (size_t i = 0; i < utxos->count; i++) {
        if (memcmp(utxos->set[i].txid, txid, TXID_LEN) == 0 &&
            utxos->set[i].vout == vout) {
            return &utxos->set[i];
        }
    }
    return NULL;
}

void utxo_set_print(const UTXOSet* utxos) {
    if (!utxos) return;

    printf("===== UTXO Set (%zu entries) =====\n", utxos->count);
    if (utxos->count == 0) {
        printf("(empty)\n");
        return;
    }

    for (size_t i = 0; i < utxos->count; i++) {
        const UTXO* u = &utxos->set[i];
        printf("UTXO %zu:\n", i);
        printf("  txid  = ");
        for (int j = 0; j < TXID_LEN; j++) printf("%02x", u->txid[j]);
        printf("\n");
        printf("  vout  = %u\n", u->vout);
        printf("  value = %llu\n", (unsigned long long)u->value);
        printf("  addr  = %s\n", u->address);
    }
}

void utxo_set_update_from_tx(UTXOSet* utxos, const Transaction* tx) {
    if (!utxos || !tx) return;

    // 删除输入对应的 UTXO（花费掉的）
    utxo_set_remove(utxos, tx->prev_txid, tx->vout);

    // 添加输出到 UTXO 集（产生新的未花费输出）
    for (size_t i = 0; i < tx->output_count; i++) {
        if (utxos->count >= MAX_UTXOS) {
            printf("UTXO 集已满，无法添加新的输出\n");
            break;
        }
        UTXO u = { 0 };
        memcpy(u.txid, tx->txid, TXID_LEN);
        u.vout = i;
        u.value = tx->outputs[i].value;
        strncpy(u.address, tx->outputs[i].address, ADDR_LEN - 1);
        u.address[ADDR_LEN - 1] = '\0';

        utxo_set_add(utxos, &u);
    }
}

uint64_t utxo_set_get_balance(const UTXOSet* utxos, const char* address)
{
    if (!utxos || !address) return 0;

    uint64_t total = 0;

    for (size_t i = 0; i < utxos->count; i++) {
        const UTXO* u = &utxos->set[i];

        if (strcmp(u->address, address) == 0) {
            total += u->value;
        }
    }
    return total;
}
