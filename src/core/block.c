#include "block.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h> 
#include "crypto/sha256.h"  // 用于 SHA256

// -----------------------------
// 辅助函数：SHA256 双哈希
// -----------------------------
static void sha256d(const uint8_t* data, size_t len, uint8_t out[BLOCK_HASH_LEN]) {
    uint8_t tmp[BLOCK_HASH_LEN];
    sha256(data, len, tmp);
    sha256(tmp, BLOCK_HASH_LEN, out);
}

/* 初始化区块（清空交易、header、hash） */
void block_init(Block* block) {
    if (!block) return;
    memset(block, 0, sizeof(Block));
    block->header.version = 1;
    block->header.timestamp = (uint32_t)time(NULL);
    block->header.bits = 0x1f00ffff;
    block->header.nonce = 0;
    block->tx_count = 0;
    memset(block->header.prev_block, 0, BLOCK_HASH_LEN);
    memset(block->header.merkle_root, 0, BLOCK_HASH_LEN);
    memset(block->block_hash, 0, BLOCK_HASH_LEN);
}

/* Append transaction to block (returns 0 on success, -1 on failure) */
int block_add_transaction(Block* block, const Transaction* tx) {
    if (!block || !tx) return -1;
    if (block->tx_count >= MAX_TXS_PER_BLOCK) return -1;

    /* Ensure txid is computed — transaction_compute_txid should exist */
    Transaction copy = *tx;
    //transaction_compute_txid(&copy);

    block->txs[block->tx_count++] = copy;
    return 0;
}

/* Compute merkle root from block->txs (result in out_merkle) */
void compute_merkle_root(const Block* block, uint8_t out_merkle[BLOCK_HASH_LEN]) {

    if (block->tx_count == 0) {
        memset(out_merkle, 0, BLOCK_HASH_LEN);
        return;
    }

    uint8_t hashes[MAX_TXS_PER_BLOCK][BLOCK_HASH_LEN];
    // 先把每个 txid 拷贝到 hashes
    for (size_t i = 0; i < block->tx_count; i++) {
        memcpy(hashes[i], block->txs[i].txid, BLOCK_HASH_LEN);
    }

    size_t count = block->tx_count;

    while (count > 1) {
        size_t new_count = (count + 1) / 2;

        for (size_t i = 0; i < new_count; i++) {
            uint8_t tmp[BLOCK_HASH_LEN * 2];
            memcpy(tmp, hashes[i * 2], BLOCK_HASH_LEN);
            if (i * 2 + 1 < count)
                memcpy(tmp + BLOCK_HASH_LEN, hashes[i * 2 + 1], BLOCK_HASH_LEN);
            else
                memcpy(tmp + BLOCK_HASH_LEN, hashes[i * 2], BLOCK_HASH_LEN); // 复制最后一个

            sha256d(tmp, BLOCK_HASH_LEN * 2, hashes[i]);
        }
        count = new_count;
    }

    memcpy(out_merkle, hashes[0], BLOCK_HASH_LEN);
}

// -----------------------------
// 计算区块哈希
// -----------------------------
void block_compute_hash(Block* block) {
    uint8_t buffer[sizeof(BlockHeader)];
    memcpy(buffer, &block->header, sizeof(BlockHeader));
    sha256d(buffer, sizeof(BlockHeader), block->block_hash);

}

// 将 difficulty 转换成目标值（越高 difficulty 越难）
static void compute_target(uint32_t difficulty, uint8_t target[32]) {
    memset(target, 0xff, 32);
    for (uint32_t i = 0; i < difficulty; i++) {
        target[i] = 0x00;  // 前 difficulty 字节设为 0
    }
}

// 比较 hash 是否小于 target
static int hash_meets_target(const uint8_t hash[32], const uint8_t target[32]) {
    for (int i = 0; i < 32; i++) {
        if (hash[i] < target[i]) return 1;
        if (hash[i] > target[i]) return 0;
    }
    return 1;
}

// -----------------------------
// 挖矿函数
// -----------------------------
void block_mine(Block* block, uint32_t difficulty) {
    if (!block) return;
    if (difficulty > 32) difficulty = 32; // 32 bytes = 256 bits

    uint8_t target[32];
    compute_target(difficulty, target);

    block->header.nonce = 0;
    block->header.timestamp = (uint32_t)time(NULL);

    printf(" Mining start, difficulty=%u\n", difficulty);

    while (1) {
        block_compute_hash(block);

        if (hash_meets_target(block->block_hash, target)) {
            break;  // 找到合法 nonce
        }

        block->header.nonce++;
        if (block->header.nonce % 100000 == 0) {
            block->header.timestamp = (uint32_t)time(NULL);
            printf(" nonce=%u, timestamp=%u\n", block->header.nonce, block->header.timestamp);
        }
    }

    printf("Mining success! nonce=%u\n", block->header.nonce);
    block_print(block);
}

/* Print block header + basic info */
void block_print(const Block* block) {
    if (!block) return;
    printf("Block:\n");
    printf("  Version: %u\n", block->header.version);
    printf("  Timestamp: %u\n", block->header.timestamp);
    printf("  Bits: %u\n", block->header.bits);
    printf("  Nonce: %u\n", block->header.nonce);

    printf("  Prev Hash: ");
    for (int i = 0; i < BLOCK_HASH_LEN; i++) printf("%02x", block->header.prev_block[i]);
    printf("\n");

    printf("  Merkle Root: ");
    for (int i = 0; i < BLOCK_HASH_LEN; i++) printf("%02x", block->header.merkle_root[i]);
    printf("\n");

    printf("  Block Hash: ");
    for (int i = 0; i < BLOCK_HASH_LEN; i++) printf("%02x", block->block_hash[i]);
    printf("\n");

    printf("  Tx Count: %zu\n", block->tx_count);
    for (size_t i = 0; i < block->tx_count; i++) {
        printf("    Tx[%zu] id=", i);
        for (int j = 0; j < TXID_LEN; j++) printf("%02x", block->txs[i].txid[j]);
        printf("\n");
    }
}

/* Blockchain init: create empty chain with optional genesis */
void blockchain_init(Blockchain* chain) {
    if (!chain) return;

    chain->block_count = 0;
}

/* Add block to chain (set prev_block from last block, compute hash, append) */
int blockchain_add_block(Blockchain* chain, Block* block) {
    if (!chain || !block) return -1;
    if (chain->block_count >= MAX_BLOCKS) return -1;

    /* 如果不是创世块，则设置 prev_block */
    if (chain->block_count > 0) {
        memcpy(block->header.prev_block,
            chain->blocks[chain->block_count - 1].block_hash,
            BLOCK_HASH_LEN);
    }
    else {
        memset(block->header.prev_block, 0, BLOCK_HASH_LEN); // 创世块 prev_block 全零
    }

    /* 如果没有 merkle_root，需要计算 */
    if (memcmp(block->header.merkle_root, "\0", BLOCK_HASH_LEN) == 0) {
        compute_merkle_root(block, block->header.merkle_root);
    }

    /* 计算区块哈希 */
    block_compute_hash(block);

    /* 安全地复制 block 到链中 */
    chain->blocks[chain->block_count] = *block;
    chain->block_count++;


    return 0;
}

/* Print entire chain */
void blockchain_print(const Blockchain* chain) {
    if (!chain) return;
    printf("Blockchain: %zu blocks\n", chain->block_count);
    for (size_t i = 0; i < chain->block_count; i++) {
        printf("=== Block %zu ===\n", i);
        block_print(&chain->blocks[i]);
    }
}

void create_coinbase_tx(Transaction* tx, const char* miner_address, uint64_t reward) {
    memset(tx, 0, sizeof(Transaction));

    // Coinbase has zero inputs
    tx->input_count = 0;

    // One output: reward to miner
    tx->output_count = 1;
    tx->outputs[0].value = reward;

    snprintf(tx->outputs[0].address, BTC_ADDRESS_MAXLEN, "%s", miner_address);

    // Compute txid
    transaction_compute_txid(tx);
}

/* Mining (proof-of-work): find nonce s.t. first `difficulty` bytes == 0 */
/*void block_mine(Block* block, uint32_t difficulty) {
    if (!block) return;
    if (difficulty > BLOCK_HASH_LEN) difficulty = BLOCK_HASH_LEN;

    printf("start pow, difficulty(bytes)=%u...\n", difficulty);

    block->header.timestamp = (uint32_t)time(NULL);
    block->header.nonce = 0;

    while (1) {
        block_compute_hash(block);

        int ok = 1;
        for (uint32_t i = 0; i < difficulty; i++) {
            if (block->block_hash[i] != 0x00) { ok = 0; break; }
        }

        if (ok) break;

        block->header.nonce++;
        if ((block->header.nonce & 0xFFFFF) == 0) {
            block->header.timestamp = (uint32_t)time(NULL);
        }
    }

    printf("pow complete! nonce=%u\n", block->header.nonce);
    block_print(block);
}*/

/*
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
}*/