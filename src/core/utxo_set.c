#include "utxo_set.h"
#include <stdio.h>
#include <string.h>

/* 初始化 */
void utxo_set_init(UTXOSet* utxos) {
    if (!utxos) return;
    utxos->count = 0;
    memset(utxos->set, 0, sizeof(utxos->set));
}

/* 添加 */
int utxo_set_add(UTXOSet* utxos, const UTXO* utxo) {
    if (!utxos || !utxo) return -1;
    if (utxos->count >= MAX_UTXOS) return -1;
    utxos->set[utxos->count++] = *utxo;
    return 0;
}

/* 删除 */
int utxo_set_remove(UTXOSet* utxos, const uint8_t txid[TXID_LEN], uint32_t vout) {
    if (!utxos || !txid) return -1;
    for (size_t i = 0; i < utxos->count; i++) {
        if (memcmp(utxos->set[i].txid, txid, TXID_LEN) == 0 &&
            utxos->set[i].vout == vout) {
            utxos->set[i] = utxos->set[utxos->count - 1];
            utxos->count--;
            return 0;
        }
    }
    return -1;
}

/* 查找 */
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

/* 打印 */
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

/*
 * 根据交易更新 UTXO 集（多输入/多输出）
 * - 删除 tx 中所有 inputs 引用的 UTXO
 * - 添加 tx 中的 outputs（vout 从 0 开始）
 */
void utxo_set_update_from_tx(UTXOSet* utxos, const Transaction* tx) {
    if (!utxos || !tx) return;

    /* 删除被花费的 UTXO（所有输入） */
    for (size_t i = 0; i < tx->input_count; i++) {
        const TxIn* in = &tx->inputs[i];

        printf("Removing spent UTXO: ");
        for (int k = 0; k < 4; k++) printf("%02x", in->txid[k]);
        printf(" (vout=%u)\n", in->vout);

        utxo_set_remove(utxos, in->txid, in->vout);
    }

    /* 添加新的 outputs */
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

        printf("Added UTXO vout=%zu value=%llu addr=%s\n",
            i, (unsigned long long)u.value, u.address);
    }
}

/* 余额统计 */
uint64_t utxo_set_get_balance(const UTXOSet* utxos, const char* address)
{
    if (!utxos || !address) return 0;
    uint64_t total = 0;
    for (size_t i = 0; i < utxos->count; i++) {
        if (strcmp(utxos->set[i].address, address) == 0)
            total += utxos->set[i].value;
    }
    return total;
}

/* 选币：顺序累加 */
int utxo_set_select(
    const UTXOSet* utxos,
    const char* address,
    uint64_t amount_needed,
    UTXO* selected,
    size_t max_selected,
    size_t* out_selected_count,
    uint64_t* out_change
) {
    if (!utxos || !address || !selected || !out_selected_count || !out_change)
        return -1;

    uint64_t accumulated = 0;
    size_t count = 0;

    for (size_t i = 0; i < utxos->count; i++) {
        const UTXO* u = &utxos->set[i];
        if (strcmp(u->address, address) == 0) {
            if (count < max_selected) {
                selected[count++] = *u;
                accumulated += u->value;
                if (accumulated >= amount_needed) break;
            }
        }
    }

    if (accumulated < amount_needed) return -1;

    *out_selected_count = count;
    *out_change = accumulated - amount_needed;
    return 0;
}





/*
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

//关键函数：根据交易更新 UTXO 集

void utxo_set_update_from_tx(UTXOSet* utxos, const Transaction* tx)
{
    if (!utxos || !tx) return;

    //1. 删除所有输入引用的 UTXO 
    for (size_t i = 0; i < tx->input_count; i++) {
        const TxIn* in = &tx->inputs[i];

        printf("Removing spent UTXO: ");
        for (int k = 0; k < 4; k++) printf("%02x", in->prev_txid[k]);
        printf(" (vout=%u)\n", in->prev_vout);

        utxo_set_remove(utxos, in->prev_txid, in->prev_vout);
    }

    //2. 添加所有输出 
    for (size_t i = 0; i < tx->output_count; i++) {
        if (utxos->count >= MAX_UTXOS) {
            printf("UTXO set full, cannot add output!\n");
            break;
        }

        UTXO u = { 0 };
        memcpy(u.txid, tx->txid, TXID_LEN);
        u.vout = i;
        u.value = tx->outputs[i].value;
        strncpy(u.address, tx->outputs[i].address, ADDR_LEN - 1);

        utxo_set_add(utxos, &u);

        printf("Added new UTXO: vout=%zu value=%llu addr=%s\n",
            i, (unsigned long long)u.value, u.address);
    }
}

//统计余额 
uint64_t utxo_set_get_balance(const UTXOSet* utxos, const char* address)
{
    uint64_t total = 0;

    for (size_t i = 0; i < utxos->count; i++) {
        if (strcmp(utxos->set[i].address, address) == 0)
            total += utxos->set[i].value;
    }
    return total;
}

//简单选币：从该地址的 UTXO 顺序累加直到够钱 
int utxo_set_select(
    const UTXOSet* utxos,
    const char* address,
    uint64_t amount_needed,
    UTXO* selected,
    size_t max_selected,
    size_t* out_selected_count,
    uint64_t* out_change
) {
    uint64_t accumulated = 0;
    size_t count = 0;

    for (size_t i = 0; i < utxos->count; i++) {
        const UTXO* u = &utxos->set[i];

        if (strcmp(u->address, address) == 0) {
            if (count < max_selected) {
                selected[count++] = *u;
                accumulated += u->value;
                if (accumulated >= amount_needed)
                    break;
            }
        }
    }

    if (accumulated < amount_needed)
        return -1;

    *out_selected_count = count;
    *out_change = accumulated - amount_needed;
    return 0;
}
*/

/*
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

int utxo_set_select(
    const UTXOSet* utxos,
    const char* address,
    uint64_t amount_needed,
    UTXO* selected,
    size_t max_selected,
    size_t* out_selected_count,
    uint64_t* out_change
) {
    if (!utxos || !address || !selected || !out_selected_count || !out_change)
        return -1;

    uint64_t accumulated = 0;
    size_t count = 0;

    // 遍历所有 UTXO，寻找属于该地址的
    for (size_t i = 0; i < utxos->count; i++) {
        const UTXO* u = &utxos->set[i];

        if (strcmp(u->address, address) == 0) {
            if (count < max_selected) {
                selected[count++] = *u;
                accumulated += u->value;

                if (accumulated >= amount_needed)
                    break;
            }
        }
    }

    if (accumulated < amount_needed) {
        // 金额不足
        return -1;
    }

    *out_selected_count = count;
    *out_change = accumulated - amount_needed;
    return 0;
}
*/