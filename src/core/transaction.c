#include "transaction.h"
#include "../crypto/double_sha256.h"
#include <string.h>

// 构造 TxOut
void txout_init(TxOut* txout, uint64_t value, const uint8_t pubkey[33]) {
    txout->value = value;

    uint8_t h160[20];
    hash160(pubkey, 33, h160);

    uint8_t* p = txout->scriptPubKey;
    p[0] = 0x76;  // OP_DUP
    p[1] = 0xa9;  // OP_HASH160
    p[2] = 0x14;  // Push 20 bytes
    memcpy(p + 3, h160, 20);
    p[23] = 0x88; // OP_EQUALVERIFY
    p[24] = 0xac; // OP_CHECKSIG
    txout->scriptLen = 25;

    pubkey_to_address(pubkey, txout->address);
}

// 初始化 TxIn (占位)
void txin_init(TxIn* txin) {
    memset(txin->prev_txid, 0, 32);
    txin->vout = 0;
    txin->scriptSig[0] = 0x00;
    txin->scriptLen = 1;
    txin->sequence = 0xffffffff;
}

// 初始化 Transaction (单输入 + 多输出)
void transaction_init(Transaction* tx, uint32_t version, uint64_t* values, const uint8_t pubkeys[][33], size_t n_outputs) {
    tx->version = version;
    tx->inputCount = 1;
    txin_init(&tx->input);

    tx->outputCount = n_outputs;
    for (size_t i = 0; i < n_outputs; i++) {
        txout_init(&tx->outputs[i], values[i], pubkeys[i]);
    }

    tx->locktime = 0;
}

// 序列化并计算 TxID
void tx_hash(const Transaction* tx, uint8_t out[32]) {
    uint8_t buf[2048]; // 足够存储多输出
    size_t offset = 0;

    // version
    memcpy(buf + offset, &tx->version, 4);
    offset += 4;

    // input count
    buf[offset++] = (uint8_t)tx->inputCount;

    // TxIn
    memcpy(buf + offset, tx->input.prev_txid, 32);
    offset += 32;
    memcpy(buf + offset, &tx->input.vout, 4);
    offset += 4;
    buf[offset++] = (uint8_t)tx->input.scriptLen;
    memcpy(buf + offset, tx->input.scriptSig, tx->input.scriptLen);
    offset += tx->input.scriptLen;
    memcpy(buf + offset, &tx->input.sequence, 4);
    offset += 4;

    // output count
    buf[offset++] = (uint8_t)tx->outputCount;

    // TxOuts
    for (size_t i = 0; i < tx->outputCount; i++) {
        memcpy(buf + offset, &tx->outputs[i].value, 8);
        offset += 8;
        buf[offset++] = (uint8_t)tx->outputs[i].scriptLen;
        memcpy(buf + offset, tx->outputs[i].scriptPubKey, tx->outputs[i].scriptLen);
        offset += tx->outputs[i].scriptLen;
    }

    // locktime
    memcpy(buf + offset, &tx->locktime, 4);
    offset += 4;

    double_sha256(buf, offset, out);
}