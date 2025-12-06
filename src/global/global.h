#pragma once
#include <stdint.h>
#include <stddef.h>
#include <core/block/blockchain.h>
#include <core/tx_pool.h>
#include <core/utxo_set.h>


// =======================
//   全局节点状态（对外）
// =======================

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

// =======================
//   初始化函数（可选）
// =======================
void global_init();
