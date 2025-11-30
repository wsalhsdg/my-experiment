#include "block.h"
#include <stdio.h>
#include <string.h>
#include <openssl/sha.h>  // 用于 SHA256

// -----------------------------
// 辅助函数：SHA256 双哈希
// -----------------------------
static void sha256d(const uint8_t* data, size_t len, uint8_t out[BLOCK_HASH_LEN]) {
    uint8_t tmp[BLOCK_HASH_LEN];
    SHA256(data, len, tmp);
    SHA256(tmp, BLOCK_HASH_LEN, out);
}

// -----------------------------
// 计算区块哈希
// -----------------------------
void block_compute_hash(Block* block) {
    uint8_t buffer[sizeof(BlockHeader)];
    memcpy(buffer, &block->header, sizeof(BlockHeader));
    sha256d(buffer, sizeof(BlockHeader), block->block_hash);
}

// -----------------------------
// 初始化区块链，创建创世块
// -----------------------------
void blockchain_init(Blockchain* chain) {
    memset(chain, 0, sizeof(Blockchain));

    Block* genesis = &chain->blocks[0];
    genesis->header.version = 1;
    memset(genesis->header.prev_block, 0, BLOCK_HASH_LEN); // 创世块前哈希全 0
    genesis->header.timestamp = (uint32_t)time(NULL);
    genesis->header.bits = 0x1f00ffff; // 随便设一个难度
    genesis->header.nonce = 0;

    block_compute_hash(genesis);
    chain->block_count = 1;
}

// -----------------------------
// 添加区块到区块链
// -----------------------------
int blockchain_add_block(Blockchain* chain, Block* block) {
    if (chain->block_count >= MAX_BLOCKS) {
        return -1; // 区块链已满
    }

    Block* prev = &chain->blocks[chain->block_count - 1];
    memcpy(block->header.prev_block, prev->block_hash, BLOCK_HASH_LEN);

    block->header.timestamp = (uint32_t)time(NULL);
    block_compute_hash(block);

    chain->blocks[chain->block_count] = *block;
    chain->block_count++;
    return 0;
}

// -----------------------------
// 打印区块信息
// -----------------------------
void block_print(const Block* block) {
    printf("Block:\n");
    printf("  Version: %u\n", block->header.version);
    printf("  Timestamp: %u\n", block->header.timestamp);
    printf("  Bits: %u\n", block->header.bits);
    printf("  Nonce: %u\n", block->header.nonce);

    printf("  Prev Hash: ");
    for (int i = 0; i < BLOCK_HASH_LEN; i++) printf("%02x", block->header.prev_block[i]);
    printf("\n");

    printf("  Block Hash: ");
    for (int i = 0; i < BLOCK_HASH_LEN; i++) printf("%02x", block->block_hash[i]);
    printf("\n");
}

// -----------------------------
// 打印整个区块链
// -----------------------------
void blockchain_print(const Blockchain* chain) {
    printf("Blockchain: %zu blocks\n", chain->block_count);
    for (size_t i = 0; i < chain->block_count; i++) {
        printf("=== Block %zu ===\n", i);
        block_print(&chain->blocks[i]);
    }
}

// 挖矿函数
void block_mine(Block* block, uint32_t difficulty) {
    if (difficulty > BLOCK_HASH_LEN) difficulty = BLOCK_HASH_LEN;

    printf("start pow,difficulty=%u...\n", difficulty);

    block->header.timestamp = (uint32_t)time(NULL);
    block->header.nonce = 0;

    while (1) {
        block_compute_hash(block);

        int ok = 1;
        for (uint32_t i = 0; i < difficulty; i++) {
            if (block->block_hash[i] != 0x00) {
                ok = 0;
                break;
            }
        }

        if (ok) break;

        block->header.nonce++;
        if (block->header.nonce % 1000000 == 0) {
            block->header.timestamp = (uint32_t)time(NULL);
        }
    }

    printf("pow complete! nonce=%u\n", block->header.nonce);
    block_print(block);
}