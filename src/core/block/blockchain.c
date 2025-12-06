#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "core/block/blockchain.h"


// ----在链上添加节点----
Blockchain* blockchain_add(Blockchain* chain, Block* b) {

    Blockchain* new_node = malloc(sizeof(Blockchain));
    new_node->block = b;
    new_node->next = NULL;

    // 如果链为空，直接作为头结点
    if (chain == NULL) 
        return new_node;

    // 找到链尾并追加
    Blockchain* cursor = chain;
    while (cursor->next) 
        cursor = cursor->next;

    cursor->next = new_node;
    return chain;
}





// ----验证区块----
int verify_block(const Block* block, const Block* prev) {

    /* --------------------------
     * 1. 检查 prev_hash
     * --------------------------*/
    if (prev != NULL) {
        unsigned char prev_hash_calc[32];
        compute_block_hash(&prev->header, prev_hash_calc);

        if (memcmp(block->header.prev_hash, prev_hash_calc, 32) != 0) {
            printf("Block validation failed: prev_hash does not match.\n");
            return 0;
        }
    }

    /* --------------------------
     * 2. 检查 Merkle Root
     * --------------------------*/
    unsigned char merkle_calc[32];
    compute_merkle_root(block->txs, block->tx_count, merkle_calc);

    if (memcmp(merkle_calc, block->header.merkle_root, 32) != 0) {
        printf(" Block validation failed: merkle_root does not match.\n");
        return 0;
    }

    /* --------------------------
     * 3. 验证 POW 难度
     * --------------------------*/
    unsigned char block_hash[32];
    compute_block_hash(&block->header, block_hash);

    for (uint32_t i = 0; i < block->header.difficulty; i++) {
        if (block_hash[i] != 0x00) {
            printf(" Pow invalid (Difficulty byte %u)\n", i);
            return 0;
        }
    }

    return 1;
}


// ----验证链----
int verify_chain(const Blockchain* chain) {
    if (chain == NULL) return 0;

    const Blockchain* cur = chain;
    const Blockchain* prev = NULL;
    int height = 0;

    while (cur) {
        if (!verify_block(cur->block, prev ? prev->block : NULL)) {
            printf("Blockchain verification failed at block index %d\n", height);
            return 0;
        }

        prev = cur;
        cur = cur->next;
        height++;
    }

    printf("Blockchain verification successful! - total blocks: %d\n\n", height);
    return 1;
}


// ----打印链----
void blockchain_print(const Blockchain* chain) {

    const Blockchain* cursor = chain;
    int height = 0;
    while (cursor) {
        const Block* blk = cursor->block;

        printf("Block %d:\n", height);
        printf("  timestamp  = %u\n", blk->header.timestamp);
        printf("  nonce      = %u\n", blk->header.nonce);
        printf("  tx_count   = %u\n", blk->tx_count);
        printf("  difficulty = %u\n", blk->header.difficulty);

        cursor = cursor->next;
        height++;
    }
}
