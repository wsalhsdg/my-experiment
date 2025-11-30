#include "transaction.h"
#include <string.h>
#include <stdio.h>
#include <openssl/sha.h>


static void sha256d(const uint8_t* data, size_t len, uint8_t out[TXID_LEN]) {
    uint8_t tmp[TXID_LEN];
    SHA256(data, len, tmp);
    SHA256(tmp, TXID_LEN, out);
}

// 初始化交易
void transaction_init(Transaction* tx,
    const uint8_t prev_txid[TXID_LEN],
    uint32_t vout,
    const TxOut* outputs,
    size_t output_count) {
    memset(tx, 0, sizeof(Transaction));
    if (prev_txid) {
        memcpy(tx->prev_txid, prev_txid, TXID_LEN);
    }
    tx->vout = vout;

    if (outputs && output_count <= MAX_TX_OUTPUTS) {
        tx->output_count = output_count;
        for (size_t i = 0; i < output_count; i++) {
            tx->outputs[i] = outputs[i];
        }
    }

    // 计算交易ID
    transaction_compute_txid(tx);
}

// 计算 TxID
void transaction_compute_txid(Transaction* tx) {
    uint8_t buf[1024] = { 0 };
    size_t offset = 0;

    memcpy(buf + offset, tx->prev_txid, TXID_LEN);
    offset += TXID_LEN;

    memcpy(buf + offset, &tx->vout, sizeof(tx->vout));
    offset += sizeof(tx->vout);

    for (size_t i = 0; i < tx->output_count; i++) {
        memcpy(buf + offset, &tx->outputs[i].value, sizeof(uint64_t));
        offset += sizeof(uint64_t);
        memcpy(buf + offset, tx->outputs[i].address, BTC_ADDRESS_MAXLEN);
        offset += BTC_ADDRESS_MAXLEN;
    }

    sha256d(buf, offset, tx->txid);
}

// 打印交易
void transaction_print(const Transaction* tx) {
    printf("Transaction:\n");
    printf("  Input: prev_txid=");
    for (int i = 0; i < TXID_LEN; i++) printf("%02x", tx->prev_txid[i]);
    printf(", vout=%u\n", tx->vout);

    printf("  Outputs (%zu):\n", tx->output_count);
    for (size_t i = 0; i < tx->output_count; i++) {
        printf("    [%zu] value=%llu, address=%s\n",
            i, (unsigned long long)tx->outputs[i].value, tx->outputs[i].address);
    }

    printf("  TxID: ");
    for (int i = 0; i < TXID_LEN; i++) printf("%02x", tx->txid[i]);
    printf("\n");
}
