#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include "wallet/wallet.h"
#include "core/block.h"
#include "core/transaction.h"
#include "core/utxo_set.h"
#include "core/tx_pool.h"

int main()
{
    printf("==== Bitcoin Mini Project ====\n\n");

    //
    // 1. 初始化交易池与 UTXO 集合
    //
    TxPool pool;
    txpool_init(&pool);

    UTXOSet utxo;
    utxo_set_init(&utxo);

    printf("Initialized TxPool and UTXOSet.\n");

    //
    // 2. 创建 coinbase 交易 tx1
    //
    TxOut outs1[2] = {
        {.value = 5000000000ULL, .address = "Alice"},
        {.value = 1000000000ULL, .address = "Bob"}
    };

    uint8_t ZERO_PREV[TXID_LEN] = { 0 };

    Transaction tx1;
    transaction_init(&tx1, ZERO_PREV, 0, outs1, 2);
    transaction_compute_txid(&tx1);

    printf("\nCreated transaction tx1:\n");
    transaction_print(&tx1);

    //
    // 3. 把 tx1 添加到交易池（会自动识别为 coinbase）
    //
    printf("\nAdding tx1 to TxPool...\n");
    txpool_add(&pool, &tx1, &utxo);

    //
    // 4. 更新 UTXO（放入 tx1 的输出）
    //
    printf("Applying tx1 to UTXO...\n");
    utxo_set_update_from_tx(&utxo, &tx1);

    //
    // 5. 创建 tx2，花费 tx1 的 vout = 0（Alice 的那笔）
    //
    TxOut outs2[1] = {
        {.value = 2000000000ULL, .address = "Charlie"}
    };

    Transaction tx2;
    transaction_init(&tx2, tx1.txid, 0, outs2, 1);
    transaction_compute_txid(&tx2);

    printf("\nCreated transaction tx2:\n");
    transaction_print(&tx2);

    //
    // 6. 现在 utxo 中已经包含 tx1 的输出，因此 tx2 能顺利加入交易池
    //
    printf("\nAdding tx2 to TxPool...\n");
    txpool_add(&pool, &tx2, &utxo);

    //
    // 7. 应用 tx2 更新 UTXO
    //
    printf("Applying tx2 to UTXO...\n");
    utxo_set_update_from_tx(&utxo, &tx2);

    //
    // 8. 打印交易池
    //
    printf("\n===== TxPool =====\n");
    txpool_print(&pool);
    printf("\n=== Balance Check ===\n");
    printf("Alice:   %llu\n", (unsigned long long)utxo_set_get_balance(&utxo, "Alice"));
    printf("Bob:     %llu\n", (unsigned long long)utxo_set_get_balance(&utxo, "Bob"));
    printf("Charlie: %llu\n", (unsigned long long)utxo_set_get_balance(&utxo, "Charlie"));

    //
    // 9. 打印最终 UTXO 集合
    //
    printf("\n===== Final UTXO Set =====\n");
    utxo_set_print(&utxo);

    printf("\nDone.\n");
    return 0;
}













/*
int main() {
    // 初始化钱包
    Wallet w1, w2;
    wallet_create(&w1);
    wallet_create(&w2);

    printf("=== Wallets ===\n");
    wallet_print(&w1);
    wallet_print(&w2);

    // 初始化 UTXO 集
    UTXOSet utxos;
    utxo_set_init(&utxos);

    // 给 w1 一个初始 UTXO（创世块模拟奖励）
    UTXO coinbase_utxo;
    memset(&coinbase_utxo, 0, sizeof(coinbase_utxo));
    coinbase_utxo.value = 5000000000; // 50 BTC
    coinbase_utxo.vout = 0;
    memset(coinbase_utxo.txid, 0x01, TXID_LEN); // 模拟创世交易ID
    strncpy(coinbase_utxo.address, w1.address, ADDR_LEN - 1);
    coinbase_utxo.address[ADDR_LEN - 1] = '\0';
    coinbase_utxo.address[ADDR_LEN - 1] = '\0';
    utxo_set_add(&utxos, &coinbase_utxo);

    printf("\nInitial UTXO set:\n");
    utxo_set_print(&utxos);

    // 构造一笔交易：w1 -> w2
    TxOut outputs[1];
    outputs[0].value = 3000000000; // 30 BTC
    strncpy(outputs[0].address, w2.address, BTC_ADDRESS_MAXLEN - 1);
    outputs[0].address[BTC_ADDRESS_MAXLEN - 1] = '\0';

    Transaction tx;
    transaction_init(&tx, coinbase_utxo.txid, coinbase_utxo.vout, outputs, 1);
    transaction_compute_txid(&tx);

    printf("\nTransaction created:\n");
    transaction_print(&tx);

    // 更新 UTXO 集
    utxo_set_update_from_tx(&utxos, &tx);

    printf("\nUTXO set after transaction:\n");
    utxo_set_print(&utxos);

    return 0;
}*/