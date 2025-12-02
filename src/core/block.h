#pragma once
#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include "core/transaction.h"


#define BLOCK_HASH_LEN 32
#define MAX_BLOCKS 1024  // 最多存储区块数量
#define MAX_TXS_PER_BLOCK 256

// -----------------------------
// 区块头
// -----------------------------
typedef struct {
    uint32_t version;
    uint8_t prev_block[BLOCK_HASH_LEN]; // 前一个区块哈希
    uint8_t merkle_root[BLOCK_HASH_LEN];// merkle root of transactions
    uint32_t timestamp;                 // Unix 时间戳
    uint32_t bits;                      // 难度目标
    uint32_t nonce;                     // 随机数（用于挖矿）
} BlockHeader;

// -----------------------------
// 区块
// -----------------------------
typedef struct {
    BlockHeader header;
    uint8_t block_hash[BLOCK_HASH_LEN]; // 区块自身哈希
    size_t tx_count;
    Transaction txs[MAX_TXS_PER_BLOCK];
} Block;

// -----------------------------
// 区块链
// -----------------------------
typedef struct {
    Block blocks[MAX_BLOCKS];
    size_t block_count;
} Blockchain;

// -----------------------------
// 区块函数接口
// -----------------------------
/* 计算区块的 Merkle Root（sha256d 节点哈希，复制最后一个在节点数为奇数时） */
void compute_merkle_root(const Block* block, uint8_t out_merkle[BLOCK_HASH_LEN]);

/**
 * 计算区块哈希（最简单版本：SHA256 双哈希区块头）
 */
void block_compute_hash(Block* block);

/**
 * 添加区块到区块链
 * 返回 0 成功，非0失败
 */
int blockchain_add_block(Blockchain* chain, Block* block);

/**
 * 初始化区块链（创建创世块）
 */
void blockchain_init(Blockchain* chain);

/**
 * 打印区块信息（调试用）
 */
void block_print(const Block* block);

/**
 * 打印整个区块链信息（调试用）
 */
void blockchain_print(const Blockchain* chain);

// 挖矿函数
// 找到一个满足 difficulty 的 nonce，使得 block_hash 前 difficulty 个字节为 0
void block_mine(Block* block, uint32_t difficulty);
