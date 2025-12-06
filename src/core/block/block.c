#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <openssl/sha.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

#include "wallet/wallet.h"
#include "core/block/block.h"
#include <arpa/inet.h>

// ----双重 SHA256----
static void sha256d(const unsigned char* data, size_t len, unsigned char* out) {
    unsigned char tmp[32];
    SHA256(data, len, tmp);    // 第一次 SHA256
    SHA256(tmp, 32, out);      // 第二次 SHA256
}
// 计算merkle根
void compute_merkle_root(const Tx* txs, int tx_count, unsigned char* out) {
    if (tx_count == 0) {
        memset(out, 0, 32);
        return;
    }

    // 临时保存每笔交易的哈希
    unsigned char layer_hash[tx_count][32];

    // 先计算每个交易的 txid
    for (int i = 0; i < tx_count; i++) {
        tx_hash(&txs[i], layer_hash[i]);
    }

    int count = tx_count;
    // 不断向上合并，直到只剩一个根
    while (count > 1) 
    {
        for (int i = 0; i < count / 2; i++)
        {
            unsigned char concat[64];

            memcpy(concat, layer_hash[2 * i], 32);
            memcpy(concat + 32, layer_hash[2 * i + 1], 32);

            // 父节点 = SHA256D(左子 + 右子)
            sha256d(concat, 64, layer_hash[i]);
        }

        // 若为奇数个节点：最后一个直接复制到下一层
        if (count % 2 == 1) {
            memcpy(layer_hash[count / 2], layer_hash[count - 1], 32);
            count = count / 2 + 1;
        }
        else {
            count = count / 2;
        }
    }

    memcpy(out, layer_hash[0], 32);   // Merkle Root
}

// ----计算区块头哈希----
void compute_block_hash(const BlockHeader* h, unsigned char* out)
{
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    // 前一区块哈希
    SHA256_Update(&ctx, h->prev_hash, 32);
    // merkle 根
    SHA256_Update(&ctx, h->merkle_root, 32);
    // 转成网络序，保证跨平台一致性
    uint32_t t = htonl(h->timestamp);
    uint32_t n = htonl(h->nonce);
    uint32_t d = htonl(h->difficulty);
    SHA256_Update(&ctx, &t, sizeof(t));
    SHA256_Update(&ctx, &n, sizeof(n));
    SHA256_Update(&ctx, &d, sizeof(d));
    SHA256_Final(out, &ctx);
}

// ----挖矿功能----
void mine_block(Block* b, uint32_t difficulty)
{
    unsigned char hash[32];

    b->header.nonce = 0;
    b->header.difficulty = difficulty;   //难度设置

    while (1)
    {
        compute_block_hash(&b->header, hash);

        int valid = 1;
        for (uint32_t i = 0; i < difficulty; i++)
        {
            if (hash[i] != 0)
            {
                valid = 0;
                break;
            }
        }

        if (valid)
            break;

        b->header.nonce++;
    }

    memcpy(b->header.block_hash, hash, 32);

    printf("Block mined successfully, nonce = %u\n", b->header.nonce);
}


// ----创建创世区块----
Block* create_genesis_block(const char* addr)
{
    Tx coinbase;
    transaction_init(&coinbase);
    add_txout(&coinbase, addr, 50);

    unsigned char zero[32] = { 0 };

    Block* genesis = create_block(zero, &coinbase, 1);
    return genesis;
}



// ----创建普通区块（非挖矿）----
Block* create_block(const unsigned char prev_hash[32], const Tx* txs, uint32_t tx_count)
{
    Block* a = malloc(sizeof(Block));    //动态分配block
    memset(a, 0, sizeof(Block));

    memcpy(a->header.prev_hash, prev_hash, 32);
    a->tx_count = tx_count;

    // 分配并复制交易
    a->txs = malloc(sizeof(Tx) * tx_count);
    for (uint32_t i = 0; i < tx_count; i++)
    {
        memcpy(&a->txs[i], &txs[i], sizeof(Tx));
        unsigned char txid[32];
        tx_hash(&a->txs[i], txid);
        memcpy(a->txs[i].txid, txid, 32);   // 保存 txid
    }

    //  merkle 根
    compute_merkle_root(a->txs, tx_count, a->header.merkle_root);
    // 区块时间、难度、初始 nonce
    a->header.timestamp = (uint32_t)time(NULL);
    a->header.nonce = 0;
    a->header.difficulty = 1;

    // 区块头哈希
    compute_block_hash(&a->header, a->header.block_hash);

    return a;
}



// ----释放区块所占内存----
void free_block(Block* a)
{
    if (!a) return;

    if (a->txs)
    {
        for (uint32_t i = 0; i < a->tx_count; i++)
            free_tx(&a->txs[i]);
        free(a->txs);
    }
    free(a);
}
