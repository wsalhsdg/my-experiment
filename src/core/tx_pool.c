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

/*
    多输入交易验证逻辑：
    1. 每个 input 的引用都必须存在
    2. pubkey 与引用的 UTXO 地址匹配
    3. 每个 input 都必须通过签名验证
    4. 输入总额 >= 输出总额
*/
static int txpool_tx_validate(const Transaction* tx, const UTXOSet* utxos)
{
    if (!tx || !utxos) return 0;

    /* 1. 输入数量合法性 */
    if (tx->input_count == 0) {
        printf("[TXPOOL] invalid: no inputs\n");
        return 0;
    }

    /* 2. 输出数量合法性 */
    if (tx->output_count == 0) {
        printf("[TXPOOL] invalid: no outputs\n");
        return 0;
    }

    uint64_t total_input = 0;
    uint64_t total_output = 0;

    /* 3. 检查每个输入是否对应有效 UTXO */
    for (size_t i = 0; i < tx->input_count; i++) {

        const TxIn* in = &tx->inputs[i];

        /* 输入不能是全 0，否则非法 */
        int all_zero = 1;
        for (int j = 0; j < TXID_LEN; j++) {
            if (in->txid[j] != 0) {
                all_zero = 0;
                break;
            }
        }
        if (all_zero) {
            printf("[TXPOOL] invalid: input %zu has zero txid\n", i);
            return 0;
        }

        /* 查询 UTXO */
        UTXO* utxo = utxo_set_find((UTXOSet*)utxos, in->txid, in->vout);
        if (!utxo) {
            printf("[TXPOOL] invalid: input %zu references non-existing UTXO\n", i);
            return 0;
        }

        total_input += utxo->value;

        /* 检查 pubkey 是否和 UTXO 地址一致（P2PKH 风格） */
        char addr_from_pub[BTC_ADDRESS_MAXLEN];
        pubkey_to_address(tx->pubkeys[i], addr_from_pub);
        if (strcmp(addr_from_pub, utxo->address) != 0) {
            printf("[TxPool] Invalid TX: pubkey does not match UTXO owner for input %zu\n", i);
            return 0;
        }

        /* 验证签名：先计算签名哈希，再用 crypto_secp_verify */
        uint8_t hash32[32];
        transaction_hash_for_sign(tx, i, hash32);

        if (!crypto_secp_verify(tx->pubkeys[i], hash32, tx->signatures[i], tx->sig_lens[i])) {
            printf("[TxPool] Invalid TX: signature verify failed for input %zu\n", i);
            return 0;
        }
    }

    /* 5. 检查输出金额 */
    
    for (size_t i = 0; i < tx->output_count; i++) {
        total_output += tx->outputs[i].value;
    }

    if (total_output > total_input) {
        printf("[TXPOOL] invalid: output > input\n");
        return 0;
    }

    return 1; // OK
}



// 放入 pool
int txpool_add(TxPool* pool, const Transaction* tx, const UTXOSet* utxos)
{
    if (pool->count >= MAX_TX_POOL_SIZE) {
        printf("[TxPool] Pool full!\n");
        return 0;
    }

    if (txpool_find(pool, tx->txid)) {
        printf("[TxPool] Already in pool\n");
        return 0;
    }

    if (!txpool_tx_validate(tx, utxos)) {
        printf("[TxPool] Validation failed\n");
        return 0;
    }

    pool->txs[pool->count++] = *tx;

    printf("[TxPool] Added TX: ");
    for (int i = 0; i < 4; i++) printf("%02x", tx->txid[i]);
    printf("...\n");

    return 1;
}



void txpool_print(const TxPool* pool) {
    printf("\n=== Transaction Pool (%zu txs) ===\n", pool->count);
    for (size_t i = 0; i < pool->count; i++) {
        printf("Tx %zu: ", i);
        for (int j = 0; j < 4; j++)
            printf("%02x", pool->txs[i].txid[j]);
        printf("...\n");
    }
}


/*
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

    // pubkey 是否与 UTXO 地址一致（P2PKH）
    char addr_from_pubkey[BTC_ADDRESS_MAXLEN];
    pubkey_to_address(tx->pubkey, addr_from_pubkey);

    if (strcmp(addr_from_pubkey, input->address) != 0) {
        printf("[TxPool] Invalid TX: pubkey does not match UTXO owner\n");
        return 0;
    }

    // 验证签名是否正确
    if (!transaction_verify(tx)) {
        printf("[TxPool] Invalid TX: signature check failed\n");
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
*/