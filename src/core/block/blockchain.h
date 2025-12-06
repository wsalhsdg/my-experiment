#ifndef BLOCKCHAIN_H
#define BLOCKCHAIN_H
#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include "core/block/block.h"


// -----------------------------
// 区块链
// -----------------------------
typedef struct BlockchainNode {
    Block* block;
    struct BlockchainNode* next;
} Blockchain;

/**
 * 添加区块到区块链
 */
Blockchain* blockchain_add(Blockchain* chain, Block* b);

// ----------------------------
// 验证区块链
// ----------------------------
int verify_chain(const Blockchain* chain);


// ----------------------------
// 验证区块
// ----------------------------
int verify_block(const Block* block, const Block* prev);


/**
 * 打印整个区块链信息（调试用）
 */
void blockchain_print(const Blockchain* chain);

#endif
