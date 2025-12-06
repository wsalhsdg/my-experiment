#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <pthread.h>
#include <unistd.h>          // read(), close()
#include <fcntl.h>           // open(), O_RDONLY

#include <global/global.h>
#include <core/block/block.h>
#include <core/block/blockchain.h>
#include <wallet/wallet.h>
#include <core/utxo_set.h>
#include <core/tx_pool.h>
#include <p2p/p2p.h>
#include <core/transaction.h>

#define MINING_REWARD 100

//=====================================================
//                  全局运行状态
//=====================================================

//标记当前节点是否作为矿工运行
int running_as_miner = 0;
// 钱包地址（Base58 字符串）
extern char addr[128];

// 钱包私钥（32 字节）
// 真实实现不应明文保存，这里为了实验保持不变
extern unsigned char priv[32];

// 节点在 P2P 网络中的地址（与钱包地址不同）
extern char node_addr[128];

// 节点用于 P2P 身份验证的公钥
extern unsigned char node_pubkey[65];
extern size_t node_pubkey_len;

// =======================
//   全局区块链运行状态
// =======================

// 全局区块链（在 main.c 初始化）
extern Blockchain* blockchain;

// UTXO 集
extern UTXO* utxo_set;

// 全局内存池
extern Mempool mempool;

//钱包公钥和WIF
unsigned char pub[65]; 
size_t publen = 65;
char wif[128];

//------------------------------------------------------
//     创建 coinbase（挖矿奖励） 交易
//------------------------------------------------------
Tx* build_coinbase_tx(UTXO** utxo_set_ptr, Mempool* pool_ptr, const char* miner_addr)
{
    if (!miner_addr) 
    {
        return NULL;
    }

    // 申请 coinbase 交易结构体
    Tx* tx = malloc(sizeof(Tx));
    if (!tx) 
    {
        return NULL;
    }
    transaction_init(tx);     //初始化交易

    // Coinbase 输入为空，只有奖励输出
    add_txout(tx, miner_addr, MINING_REWARD);

    // coinbase 正常情况下不需要签名，使用 dummy 私钥保持统一处理
    unsigned char dummy_priv[32] = { 0 };
    sign_tx(tx, dummy_priv);

    // 计算 coinbase txid
    tx_hash(tx, tx->txid);

    // 将 coinbase 加入 tx_pool
    if (pool_ptr) {
        tx_pool_add_tx(pool_ptr, tx, *utxo_set_ptr);
    }

    // 挖矿后直接更新本地 UTXO作为矿工奖励
    update_utxo_set(utxo_set_ptr, tx, tx->txid);

    printf("[Coinbase] %u reward sent to %s\n", MINING_REWARD, miner_addr);
    return tx;
}


//------------------------------------------------------
// 构造 + 挖掘区块（矿工）
//------------------------------------------------------
Block* build_and_mine_block()
{
    if (!blockchain) {
        printf("[Mining] Error: chain not initialized.\n");
        return NULL;
    }
    printf("[Mining] Constructing block...\n");

    // 获取链尾
    Blockchain* tail = blockchain;
    while (tail->next) tail = tail->next;
    Block* prev = tail->block;

    // 生成交易奖励
    Tx* reward = build_coinbase_tx(&utxo_set, &mempool, addr);
    if (!reward) {
        printf("[Mining] coinbase build failed.\n");
        return NULL;
    }

    // 获取 txpool 中t交易的数量
    int tx_count = 1;
    MempoolTx* p = mempool.head;
    while (p) { tx_count++; p = p->next; }

    // 为 block 构造 tx 数组（在栈上分配合理范围内的数组）
    Tx* txlist = malloc(sizeof(Tx) * tx_count);
    if (!txlist) {
        free(reward);
        return NULL;
    }
    txlist[0] = *reward;
    int i = 1;

    p = mempool.head;

    while (p) {
        txlist[i++] = *(p->tx); 
        p = p->next;
    }

    // 创建block
    Block* block = create_block(prev->header.block_hash, txlist, (uint32_t)tx_count);
    if (!block) 
    {
        free(txlist);//释放
        return NULL;
    }
    //开始挖矿
    printf("[Mining] Start mining block...\n");
    mine_block(block, 2);

    // 加入本地链
    blockchain = blockchain_add(blockchain, block);

    //广播给peers
    broadcast_block(block);

    printf("[Mining] Block mined, %d transactions included.\n", tx_count);

    free(txlist);
    return block;
}


//------------------------------------------------------
// 创建交易
//------------------------------------------------------
void handle_user_transaction()
{
    char to_addr[128];
    uint64_t amount;

    printf("Enter address to send to: ");
    if (scanf("%127s", to_addr) != 1) 
    { 
        while (getchar() != '\n'); 
        return; 
    }
    printf("Enter amount: ");
    if (scanf("%" SCNu64, &amount) != 1) 
    { 
        while (getchar() != '\n'); 
        return;
    }
    while (getchar() != '\n');

    //----创建交易----
    Tx* tx = create_transaction(&utxo_set, &mempool, addr, to_addr, amount, priv);

    if (!tx) {
        printf("[TX] Failed to build transaction.\n");
        return;
    }

    printf("[TX] Added transaction to tx_pool.\n");

    // 自动挖一个只包含此交易的区块
    Blockchain* tail = blockchain;
    while (tail->next) tail = tail->next;

    Block* prev = tail->block;
    Tx txlist[1];
    txlist[0] = *tx;
    Block* b = create_block(prev->header.block_hash, txlist, 1);
    printf("[Mining] Mining block for new TX...\n");
    mine_block(b, 2);
    blockchain = blockchain_add(blockchain, b);
    broadcast_block(b);

    printf("[Block] New block mined .\n");
}


void start_as_server()
{
    if (running_as_miner) {
        printf("[Info] Already running as server.\n");
        return;
    }

    running_as_miner = 1;

    // 创建并挖掘创世区块
    Block* genesis = create_genesis_block(addr);
    mine_block(genesis, 1);
    blockchain = blockchain_add(NULL, genesis);

    unsigned char genesis_txid[32];
    tx_hash(&genesis->txs[0], genesis_txid);

    // 给矿工生成初始UTXO
    add_utxo(&utxo_set, genesis_txid, 0, addr, 0);

    printf("[Server] Genesis block created.\n");

    int port = 0;
    printf("Enter listen port: ");
    if (scanf("%d", &port) != 1) return;
    while (getchar() != '\n');

    start_server(port);
    broadcast_addresss(addr, pub);

    printf("[Server] Started successfully.\n");
}


void start_as_client()
{
    if (!running_as_miner && blockchain) {
        printf("[Info] Already running as client.\n");
        return;
    }

    running_as_miner = 0;

    char ip[64];
    int port = 0;
    printf("Enter server IP: ");
    if (scanf("%63s", ip) != 1) return;

    printf("Enter server port: ");
    if (scanf("%d", &port) != 1) return;

    while (getchar() != '\n');

    connect_to_peer(ip, port);
    broadcast_addresss(addr, pub);
    printf("[Client] Connected to server.\n");
}


void bitcoin_menu()
{
    while (1) {
        printf("\n=============================\n");
        printf("          BITCOIN            \n");
        printf("=============================\n");
        printf("[1] Run as Miner (server)\n");
        printf("[2] Run as user (client)\n");
        printf("[3] Create Transaction\n");
        printf("[4] Mine Block\n");
        printf("[5] Show Blockchain\n");
        printf("[6] Show UTXO\n");
        printf("[7] Show Tx_pool\n");
        printf("[8] Balance Enquiry\n");
        printf("[9] Exit The System\n");
        printf("=============================\n");
        printf("Enter num of function: ");

        int choice = 0;
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');
            printf("Invalid input! Please try again.\n");
            continue;
        }
        while (getchar() != '\n');

        switch (choice)
        {
        case 1:
            start_as_server();
            break;

        case 2:
            start_as_client();
            break;

        case 3:
            handle_user_transaction();
            break;

        case 4:
            if (!running_as_miner) {
                printf("[Error] You have no right or ability to mine.\n");
                break;
            }
            build_and_mine_block();
            break;

        case 5:
            blockchain_print(blockchain);
            break;

        case 6:
            print_utxo_set(utxo_set);
            break;

        case 7:
            tx_pool_print(&mempool);
            break;

        case 8: {
            uint64_t bal = get_balance(utxo_set, addr);
            //printf("%s", addr);
            printf("Balance = %" PRIu64 "\n",  bal);
            break;
        }

        case 9:
            printf("Exiting the system.\n");
            p2pstop();
            exit(0);

        default:
            printf("Invalid option, Please try again!\n");
        }
    }
}


int main()
{
    // 初始化全局变量
    global_init();
    tx_pool_init(&mempool);

    // 生成私钥、公钥、地址
    generate_privkey(priv);
    privkey_to_pubkey_and_addr(priv, pub, &publen, addr, sizeof(addr), 1);
    privkey_to_WIF(priv, 32, 1, wif, sizeof(wif));

    printf("Wallet Address: %s\n", addr);
    printf("WIF: %s\n\n", wif);

    set_node_address(addr, pub);

    // 进入菜单
    bitcoin_menu();
    return 0;
}


/*
// ===== 主菜单 =====
void bitcoin_menu()
{
    while (1) {
        printf("\n=============================\n");
        printf("----bitcoin---- \n");
        printf("=============================\n");
        printf("1. Create Transaction\n");
        printf("2. Show Balance\n");
        printf("3. Mine Block\n");
        printf("4. Show Blockchain\n");
        printf("5. Show UTXO\n");
        printf("6. Show tx_pool\n");
        printf("7. Exit\n");
        printf("=============================\n");
        printf("Enter choice: ");

        int choice = 0;
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n');
            printf("Invalid input\n");
            continue;
        }
        while (getchar() != '\n');

        
        switch (choice)
        {
        case 1:
            handle_user_transaction();
            break;

        case 2: {
            uint64_t bal = get_balance(utxo_set, addr);
            printf("Balance(%s) = %" PRIu64 "\n", addr, bal);
            break;
        }

        case 3:
            if (!running_as_miner) { printf("Only server can mine.\n"); break; }
            build_and_mine_block();
            break;

        case 4:
            blockchain_print(blockchain);
            break;

        case 5:
            print_utxo_set(utxo_set);
            break;

        case 6:
            tx_pool_print(&mempool);
            break;

        case 7:
            printf("Exiting...\n");
            p2pstop();
            exit(0);

        default:
            printf("Invalid option.\n");
        }
    }
}

int main()
{
    //初始化全局变量
    global_init();

    tx_pool_init(&mempool);

    // 使用wallet功能生成私钥
    generate_privkey(priv);
    // 私钥生成公钥和地址
    privkey_to_pubkey_and_addr(priv, pub, &publen, addr, sizeof(addr), 1);
    // 私钥生成WIF
    privkey_to_WIF(priv, 32, 1, wif, sizeof(wif));

    printf("Wallet Address: %s\n", addr);
    printf("WIF: %s\n\n", wif);
    set_node_address(addr, pub);

    int mode = 0;
    printf("Select mode: 1 = server, 2 = client: ");
    if (scanf("%d", &mode) != 1) return 1;
    while (getchar() != '\n');

    if (mode == 1) {
        running_as_miner = 1;

        // 创建并挖掘创世区块
        Block* genesis = create_genesis_block(addr);
        mine_block(genesis, 1);
        blockchain = blockchain_add(NULL, genesis);

        unsigned char genesis_txid[32];
        tx_hash(&genesis->txs[0], genesis_txid);

        // 创世区块给 addr 一个 UTXO，并给50作为初始
        add_utxo(&utxo_set, genesis_txid, 0, addr, 50);

        printf("[Server] Genesis block created.\n");

        int port = 0;
        printf("Enter listen port: ");
        if (scanf("%d", &port) != 1) return 1;
        while (getchar() != '\n');

        start_server(port);
        broadcast_addresss(addr, pub);
    }
    else {
        running_as_miner = 0;

        char ip[64];
        int port = 0;
        printf("Enter server IP: "); if (scanf("%63s", ip) != 1) return 1;
        printf("Enter server port: "); if (scanf("%d", &port) != 1) return 1;
        while (getchar() != '\n');

        connect_to_peer(ip, port);
        broadcast_addresss(addr, pub);
    }

    // 菜单循环（主线程）
    bitcoin_menu();

    return 0;
}*/
