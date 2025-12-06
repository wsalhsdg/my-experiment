#include "global.h"
#include <string.h>
#include <core/block/blockchain.h>
#include <core/tx_pool.h>
#include <core/utxo_set.h>


// =======================
//   全局变量定义区
// =======================

// 钱包地址
char addr[128] = { 0 };

// 钱包私钥（32 字节）
unsigned char priv[32] = { 0 };

// 节点在 P2P 网络中的地址
char node_addr[128] = { 0 };

// P2P 模块使用的节点公钥
unsigned char node_pubkey[65] = { 0 };
size_t node_pubkey_len = 0;

// 全局区块链指针
Blockchain* blockchain = NULL;

// 全局 UTXO 集
UTXO* utxo_set = NULL;

// 全局内存池
Mempool mempool;


// =======================
//   全局初始化
// =======================
void global_init() {
    memset(addr, 0, sizeof(addr));
    memset(priv, 0, sizeof(priv));
    memset(node_addr, 0, sizeof(node_addr));
    memset(node_pubkey, 0, sizeof(node_pubkey));
    node_pubkey_len = 0;

    blockchain = NULL;
    utxo_set = NULL;
    memset(&mempool, 0, sizeof(mempool));
}
