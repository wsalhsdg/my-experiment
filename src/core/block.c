#include "block.h"
#include "../crypto/double_sha256.h"

void block_hash(const Block* blk, uint8_t out[32]) {
    double_sha256((uint8_t*)blk, sizeof(Block), out);
}
