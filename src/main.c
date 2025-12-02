#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>     // read(), close()
#include <fcntl.h>      // open(), O_RDONLY
#include <sys/types.h>
#include <sys/stat.h>
#include "wallet/wallet.h"
#include "core/block.h"
#include "core/transaction.h"
#include "core/utxo_set.h"
#include "core/tx_pool.h"
#include "crypto/crypto_tools.h"
#include "p2p/p2p.h"
#include <pthread.h>
#include <secp256k1.h>

// 随机生成私钥（32字节）
void random_privkey(uint8_t priv[32]) {
    secp256k1_context* ctx = crypto_secp_get_context();
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        // fallback to rand (非理想)
        do {
            for (int i = 0; i < 32; i++) priv[i] = rand() & 0xFF;
        } while (!secp256k1_ec_seckey_verify(ctx, priv));
        return;
    }

    do {
        ssize_t r = read(fd, priv, 32);
        if (r != 32) {
            // fallback to rand
            for (int i = 0; i < 32; i++) priv[i] = rand() & 0xFF;
        }
    } while (!secp256k1_ec_seckey_verify(ctx, priv));

    close(fd);
}

int main(int argc, char* argv[]) {
    srand((unsigned)time(NULL));

    secp256k1_context* ctx =
        secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
    crypto_secp_set_context(ctx);
    /*
    printf("==== test ====\n\n");

    // -----------------------------------------
    // 1. 生成用户钱包：Alice & Bob
    // -----------------------------------------
    uint8_t privA[32], privB[32];
    uint8_t pubA[33], pubB[33];
    char addrA[BTC_ADDRESS_MAXLEN];
    char addrB[BTC_ADDRESS_MAXLEN];

    random_privkey(privA);
    random_privkey(privB);

    ecdsa_get_pubkey(privA, pubA);
    ecdsa_get_pubkey(privB, pubB);

    pubkey_to_address(pubA, addrA);
    pubkey_to_address(pubB, addrB);

    printf("Alice Address = %s\n", addrA);
    printf("Bob   Address = %s\n\n", addrB);

    // -----------------------------------------
    // 2. 初始化模块
    // -----------------------------------------
    UTXOSet utxos;
    utxo_set_init(&utxos);

    TxPool pool;
    txpool_init(&pool);

    // -----------------------------------------
    // 3. 创建“创世UTXO”：给 Alice 50 BTC
    // -----------------------------------------
    Transaction coinbase;
    uint8_t zero_txid[32] = { 0 };
    TxIn cb_in;
    memset(&cb_in, 0, sizeof(cb_in));
    memcpy(cb_in.txid, zero_txid, TXID_LEN);
    cb_in.vout = 0;

    TxOut out_cb = {
        .value = 50ull * 100000000ull,   // 50 BTC
    };
    strncpy(out_cb.address, addrA, BTC_ADDRESS_MAXLEN);

    transaction_init(&coinbase, &cb_in, 1, &out_cb, 1);

    utxo_set_update_from_tx(&utxos, &coinbase);

    printf("=== UTXO ===\n");
    utxo_set_print(&utxos);

    // -----------------------------------------
    // 4. Alice → Bob 转账 20 BTC
    // -----------------------------------------
    Transaction tx1;

    TxOut out1 = {
        .value = 20ull * 100000000ull,
    };
    strncpy(out1.address, addrB, BTC_ADDRESS_MAXLEN);

    // 找到 Alice 的 UTXO
    UTXO* u = utxo_set_find(&utxos, coinbase.txid, 0);
    if (!u) {
        printf("couldn't find UTXO，stop\n");
        return 0;
    }

    TxIn in1;
    memset(&in1, 0, sizeof(in1));
    memcpy(in1.txid, u->txid, TXID_LEN);
    in1.vout = u->vout;

    transaction_init_with_change(&tx1, &in1, 1, &out1, 1, u->value, addrA);

    // Alice 进行签名
    if (!transaction_sign(&tx1, privA)) {
        printf("fail to sign transaction\n");
        return 0;
    }

    printf("\n=== Alice→Bob tx ===\n");
    transaction_print(&tx1);

    // 验证
    if (!transaction_verify(&tx1)) {
        printf("fail to verify!\n");
        return 0;
    }

    printf("successful signature verification √\n");

    // -----------------------------------------
    // 5. 将交易加入 TxPool（自动执行 UTXO 检查+签名验证）
    // -----------------------------------------
    if (!txpool_add(&pool, &tx1, &utxos)) {
        printf("fail to join txpool！\n");
        return 0;
    }

    printf("\n=== Txpool ===\n");
    txpool_print(&pool);

    // -----------------------------------------
    // 6. 更新UTXO
    // -----------------------------------------
    utxo_set_update_from_tx(&utxos, &tx1);

    printf("\n=== UTXO after tx1 ===\n");
    utxo_set_print(&utxos);

    // -----------------------------------------
    // 7. 查询余额
    // -----------------------------------------
    printf("\n=== balance enquiry ===\n");
    printf("Alice: %llu sat\n", (unsigned long long)utxo_set_get_balance(&utxos, addrA));
    printf("Bob:   %llu sat\n", (unsigned long long)utxo_set_get_balance(&utxos, addrB));
    // -----------------------------------------
// 8. 测试：Alice 有两个100BTC的UTXO，转给 Bob 150BTC
// -----------------------------------------
    printf("\n\n==============================\n");
    printf("=== Test Multi-Input TX ===\n");
    printf("==============================\n");

    // 清空UTXO
    utxo_set_init(&utxos);

    //
    // UTXO #0: 100 BTC → Alice
    //
    Transaction txA0;
    memset(&txA0, 0, sizeof(txA0));

    uint8_t zero_txid2[32] = { 0 };
    TxIn cb_in2;
    memset(&cb_in2, 0, sizeof(cb_in2));
    memcpy(cb_in2.txid, zero_txid2, TXID_LEN);
    cb_in2.vout = 0;
    TxOut outA0 = { .value = 100ull * 100000000ull };
    strncpy(outA0.address, addrA, BTC_ADDRESS_MAXLEN);

    transaction_init(&txA0, &cb_in2, 1, &outA0, 1);
    utxo_set_update_from_tx(&utxos, &txA0);

    //
    // UTXO #1: 100 BTC → Alice
    //
    Transaction txA1;
    memset(&txA1, 0, sizeof(txA1));

    uint8_t zero_txid3[32] = { 0 };
    TxIn cb_in3;
    memset(&cb_in3, 0, sizeof(cb_in3));
    memcpy(cb_in3.txid, zero_txid3, TXID_LEN);
    cb_in3.vout = 0;
    TxOut outA1 = { .value = 100ull * 100000000ull };
    strncpy(outA1.address, addrA, BTC_ADDRESS_MAXLEN);

    transaction_init(&txA1, &cb_in3, 1, &outA1, 1);
    utxo_set_update_from_tx(&utxos, &txA1);

    printf("\n=== Alice initial UTXOs (2 × 100BTC) ===\n");
    utxo_set_print(&utxos);

    // -----------------------------------------
    // 选币 150 BTC（将自动选到两个UTXO）
    // -----------------------------------------
    uint64_t amount_send = 150ull * 100000000ull;

    UTXO selected2[10];
    size_t selected2_count = 0;
    uint64_t change2 = 0;

    if (utxo_set_select(&utxos, addrA, amount_send,
        selected2, 10, &selected2_count, &change2) != 0)
    {
        printf("fail selcet!\n");
        return 0;
    }

    printf("\n=== Selected inputs for 150 BTC ===\n");
    for (size_t i = 0; i < selected2_count; i++) {
        printf("  Input %zu: %llu sat\n",
            i, (unsigned long long)selected2[i].value);
    }
    printf("找零 = %llu sat (%.8f BTC)\n",
        (unsigned long long)change2, change2 / 100000000.0);

    // -----------------------------------------
    // 构造真正的多输入、多输出交易
    // -----------------------------------------
    Transaction tx2;
    memset(&tx2, 0, sizeof(tx2));

    // ---- 输入 ----
    tx2.input_count = selected2_count;

    for (size_t i = 0; i < selected2_count; i++) {
        memcpy(tx2.inputs[i].txid, selected2[i].txid, TXID_LEN);
        tx2.inputs[i].vout = selected2[i].vout;
    }

    // ---- 输出：150 BTC -> Bob ----
    TxOut outBob = { .value = amount_send };
    strncpy(outBob.address, addrB, BTC_ADDRESS_MAXLEN);

    // ---- 输出：找零 -> Alice ----
    TxOut outChange = { .value = change2 };
    strncpy(outChange.address, addrA, BTC_ADDRESS_MAXLEN);

    tx2.outputs[0] = outBob;
    tx2.outputs[1] = outChange;
    tx2.output_count = 2;

    // 计算 TxID
    transaction_compute_txid(&tx2);

    // 签名
    if (!transaction_sign(&tx2, privA)) {
        printf("tx2 sign fail!\n");
        return 0;
    }

    if (!transaction_verify(&tx2)) {
        printf("tx2 verify fail!\n");
        return 0;
    }

    printf("\n=== Alice → Bob 150 BTC (Multi-input TX) ===\n");
    transaction_print(&tx2);

    // -----------------------------------------
    // 更新UTXO
    // -----------------------------------------
    utxo_set_update_from_tx(&utxos, &tx2);

    printf("\n=== UTXO set after transfer ===\n");
    utxo_set_print(&utxos);

    // -----------------------------------------
    // 查询余额
    // -----------------------------------------
    printf("\n=== Final Balance ===\n");
    printf("Alice: %llu sat\n",
        (unsigned long long)utxo_set_get_balance(&utxos, addrA));
    printf("Bob:   %llu sat\n",
        (unsigned long long)utxo_set_get_balance(&utxos, addrB));
        */
    /* printf("\n=== Test Block & Mining ===\n");

    // -----------------------------
    // 1. 初始化创世块
    // -----------------------------
    Block genesis;
    memset(&genesis, 0, sizeof(genesis));
    genesis.header.version = 1;
    memset(genesis.header.prev_block, 0, BLOCK_HASH_LEN);

    // 假设已有 coinbase 交易
    Transaction coinbase;
    memset(&coinbase, 0, sizeof(coinbase));
    coinbase.output_count = 1;
    coinbase.outputs[0].value = 50ull * 100000000ull; // 50 BTC
    strncpy(coinbase.outputs[0].address, "Alice", BTC_ADDRESS_MAXLEN);
    transaction_compute_txid(&coinbase);

    genesis.tx_count = 1;
    genesis.txs[0] = coinbase;

    uint8_t merkle[BLOCK_HASH_LEN];
    compute_merkle_root(&genesis, merkle);
    memcpy(genesis.header.merkle_root, merkle, BLOCK_HASH_LEN);

    printf("[DEBUG] Genesis Merkle Root: ");
    for (int i = 0; i < BLOCK_HASH_LEN; i++) printf("%02x", genesis.header.merkle_root[i]);
    printf("\n");

    // 挖矿难度：前2字节为0
    block_mine(&genesis, 2);

    block_print(&genesis);

    // -----------------------------
    // 2. 添加新区块
    // -----------------------------
    Block block1;
    memset(&block1, 0, sizeof(block1));
    block1.header.version = 1;
    memcpy(block1.header.prev_block, genesis.block_hash, BLOCK_HASH_LEN);

    // 假设已有交易 tx1
    Transaction tx1;
    memset(&tx1, 0, sizeof(tx1));
    tx1.output_count = 1;
    tx1.outputs[0].value = 20ull * 100000000ull;
    strncpy(tx1.outputs[0].address, "Bob", BTC_ADDRESS_MAXLEN);
    transaction_compute_txid(&tx1);

    block1.tx_count = 1;
    block1.txs[0] = tx1;

    compute_merkle_root(&block1, block1.header.merkle_root);

    printf("[DEBUG] Block1 Merkle Root: ");
    for (int i = 0; i < BLOCK_HASH_LEN; i++) printf("%02x", block1.header.merkle_root[i]);
    printf("\n");

    // 挖矿
    block_mine(&block1, 2);

    block_print(&block1);
    
    // -----------------------------
    // 3. 链式打印//备注，只用动态分配才能正常运行，否则会出现Segmentation fault (core dumped)，即使减小MAX_BLOCKS也不行
    // -----------------------------
    Blockchain* chain = malloc(sizeof(Blockchain));
    if (!chain) { perror("malloc"); exit(1); }
    blockchain_init(chain);
    blockchain_add_block(chain, &genesis);
    blockchain_add_block(chain, &block1);
    
    printf("\n=== Blockchain ===\n");
    blockchain_print(chain);
    
    free(chain);
    */
     if (argc < 2) {
        printf("Usage: %s <port> [peer_ip:peer_port]\n", argv[0]);
        return 1;
    }

    uint16_t port = atoi(argv[1]);

    P2PNetwork net;
    p2p_init(&net);
    net.listen_port = port;
    // 启动 server 线程
    pthread_t server_tid;
    pthread_create(&server_tid, NULL, p2p_server_thread, &net);
    pthread_detach(server_tid);

    // 连接其他节点（可选）
    if (argc >= 3) {
        char ip[32];
        uint16_t peer_port;
        sscanf(argv[2], "%31[^:]:%hu", ip, &peer_port);
        p2p_add_peer(&net, ip, peer_port);
    }
    /*    // 启动线程监听（假设我们用同一个程序连接回自己模拟）
    for (size_t i = 0; i < net.peer_count; i++) {
        pthread_t tid;
        pthread_create(&tid, NULL, p2p_handle_incoming, &net.peers[i]);
        pthread_detach(tid);
    }

    // 简单发送 ping 测试
    sleep(1);
    Message ping = { .type = MSG_PING, .payload_len = 0 };
    p2p_broadcast(&net, &ping);

    printf("Press Ctrl+C to exit\n");
    while (1) sleep(1);*/
    // 简单测试：每 5 秒广播 PING
    while (1) {
        Message ping = { .type = MSG_PING, .payload_len = 0 };
        p2p_broadcast(&net, &ping);
        sleep(5);
    }

    
    return 0;
}



















/*
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
*/












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