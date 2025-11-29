#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "core/block.h"
#include "core/transaction.h"
#include "core/utxo_set.h"
#include "utils/hex.h"
#include "crypto/sha256.h"
#include "crypto/double_sha256.h"
#include "crypto/ripemd160.h"
#include "crypto/base58.h"
#include "crypto/crypto_tools.h"

int main() {

    printf("=== My Bitcoin Node (Minimal) ===\n");

    // 初始化 UTXO 集
    UTXOSet set;
    utxo_init(&set);



    uint8_t pubkeys[2][33] = {
        {0x03,0x1d,0xb2,0x7a,0xe3,0x3a,0x4f,0x89,0x57,0x1a,0x2b,0x7c,0x9d,0x0f,0x4e,0xac,
         0x3b,0xd0,0xe7,0x11,0x2a,0x7f,0xc1,0x45,0x98,0x5e,0xab,0x23,0x61,0x0f,0x9d,0x7c,0x01},
        {0x02,0x1c,0xa2,0x5b,0xf3,0x4a,0x7c,0x91,0x47,0x2a,0x3d,0x6c,0x8d,0x1f,0x2e,0xbc,
         0x4b,0xc0,0xd7,0x01,0x3a,0x6f,0xd1,0x35,0x88,0x4f,0xbb,0x33,0x51,0x1f,0x8d,0x6c,0x02}
    };

    uint64_t values[2] = { 5000000000ULL, 2000000000ULL }; // 50 BTC + 20 BTC

    Transaction tx;
    transaction_init(&tx, 1, values, pubkeys, 2);

    for (size_t i = 0; i < tx.outputCount; i++) {
        printf("Output %zu:\n", i);
        printf("  Value: %llu\n", (unsigned long long)tx.outputs[i].value);
        printf("  Address: %s\n", tx.outputs[i].address);
        printf("  scriptPubKey: ");
        for (size_t j = 0; j < tx.outputs[i].scriptLen; j++) printf("%02x", tx.outputs[i].scriptPubKey[j]);
        printf("\n");
    }

    uint8_t txid[32];
    tx_hash(&tx, txid);

    char hex_txid[65] = { 0 };
    for (int i = 0; i < 32; i++) sprintf(hex_txid + i * 2, "%02x", txid[i]);
    printf("Transaction hash (TxID) = %s\n", hex_txid);




    // 添加到 UTXO 集
    utxo_add(&set, &tx);

    // 生成区块
    Block blk = { 0 };
    blk.version = 1;
    blk.timestamp = (uint32_t)time(NULL);
    blk.tx = tx;

    uint8_t blk_hash[32];
    block_hash(&blk, blk_hash); 

    char hex_blk[65] = { 0 };
    bytes_to_hex(blk_hash, 32, hex_blk);
    printf("Block hash = %s\n", hex_blk);

    return 0;
}
