#ifndef BLOCK_H
#define BLOCK_H
#include <stdint.h>
#include <stddef.h>
#include <time.h>

#include <core/transaction.h>


// -----------------------------
// 区块头
// -----------------------------
typedef struct {
    unsigned char prev_hash[32];     // 前一个区块哈希
    unsigned char merkle_root[32];   // merkle root of transactions
    unsigned char block_hash[32];    // 区块哈希
    uint32_t timestamp;              // Unix 时间戳
    uint32_t difficulty;             //挖矿难度
    uint32_t nonce;                  // 随机数（用于挖矿）
 
} BlockHeader;


// -----------------------------
// 区块
// -----------------------------
typedef struct {
    BlockHeader header;               
    uint32_t tx_count;
    Tx* txs;

} Block;


//
// 打印区块信息（调试用）
//
//void block_print(const Block* block);

// -----------------------------
// 计算Merkle Root
// -----------------------------
void compute_merkle_root(const Tx* txs, int tx_count, unsigned char* out);


// -----------------------------
// 计算区块哈希
// -----------------------------
void compute_block_hash(const BlockHeader* header, unsigned char* out);

// -----------------------------
// 创建创世区块
// -----------------------------
Block* create_genesis_block(const char* addr);

// -----------------------------
// 创建区块
// -----------------------------
Block* create_block(const unsigned char prev_hash[32], const Tx* txs, uint32_t tx_count);


// -----------------------------
// 挖矿
// -----------------------------
void mine_block(Block* b, uint32_t difficulty);

// -----------------------------
// 释放
// -----------------------------
void free_block(Block* b);

#endif

