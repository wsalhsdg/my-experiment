#pragma once
#include <stdint.h>
#include "transaction.h"

typedef struct {
    uint32_t version;
    uint32_t timestamp;
    Transaction tx;    // 简化：每个区块只有一个交易
} Block;

void block_hash(const Block* blk, uint8_t out[32]);
