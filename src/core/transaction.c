#include "transaction.h"
#include <string.h>
#include <stdio.h>
//#include <openssl/sha.h>
#include "crypto/crypto_tools.h"


static void sha256d(const uint8_t* data, size_t len, uint8_t out[TXID_LEN]) {
    uint8_t tmp[TXID_LEN];
    sha256(data, len, tmp);
    sha256(tmp, TXID_LEN, out);
}

// 初始化交易
void transaction_init(Transaction* tx,
    const uint8_t prev_txid[TXID_LEN],
    uint32_t vout,
    const TxOut* outputs,
    size_t output_count) 
    {
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

void transaction_init_with_change(Transaction* tx,
    const uint8_t prev_txid[TXID_LEN],
    uint32_t vout,
    const TxOut* outputs,
    size_t output_count,
    uint64_t total_input_value,
    const char* change_address)
{
    // 先调用原来的初始化
    transaction_init(tx, prev_txid, vout, outputs, output_count);

    // 计算找零金额
    uint64_t total_output = 0;
    for (size_t i = 0; i < output_count; i++) {
        total_output += outputs[i].value;
    }

    if (total_input_value > total_output) {
        TxOut change;
        change.value = total_input_value - total_output;
        snprintf(change.address, BTC_ADDRESS_MAXLEN, "%s", change_address);

        if (tx->output_count < MAX_TX_OUTPUTS) {
            tx->outputs[tx->output_count++] = change;
        }
    }

    // 重新计算 TxID
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

static void transaction_hash_for_sign(const Transaction* tx, uint8_t out32[32]) {
    memcpy(out32, tx->txid, TXID_LEN);
}


/*
    签名交易（privkey32: 32 字节私钥）
*/
int transaction_sign(Transaction* tx, const uint8_t* privkey32)
{

    if (!tx || !privkey32) {
        printf("transaction_sign error: NULL pointer\n");
        return 0;
    }

    uint8_t hash32[32];
    transaction_hash_for_sign(tx, hash32);

    // ========== 使用 crypto_tools.h 的接口 ==========

    int ret = crypto_secp_get_pubkey(privkey32, tx->pubkey);
    if (!ret) {
        printf("transaction_sign error: crypto_secp_get_pubkey failed\n");
        return 0;
    }

    // 签名
    ret = crypto_secp_sign(privkey32, hash32, tx->signature, &tx->sig_len);
    if (!ret) {
        printf("transaction_sign error: crypto_secp_sign failed\n");
        return 0;
    }

    printf("transaction_sign success: sig_len=%zu\n", tx->sig_len);
    return 1;
}


/*
    验证交易签名
*/
int transaction_verify(const Transaction* tx)
{
    uint8_t hash32[32];
    transaction_hash_for_sign(tx, hash32);

    // 使用 secp256k1 验证
    return crypto_secp_verify(
        tx->pubkey,
        hash32,
        tx->signature,
        tx->sig_len
    );
}