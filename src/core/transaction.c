#include <stdlib.h>
#include <string.h>
#include "core/transaction.h"
//#include "crypto/crypto_tools.h"


//extern void sha256(const uint8_t* data, size_t len, uint8_t out32[32]);

/*static void sha256d(const uint8_t* data, size_t len, uint8_t out[TXID_LEN]) {
    uint8_t tmp[32];
    sha256(data, len, tmp);
    sha256(tmp, 32, out);
}
*/
/*
 * 初始化交易对象，将输入输出数组清空。
 */
void transaction_init(Tx* t) {
    t->inputs = NULL;
    t->input_count = 0;
    t->outputs = NULL;
    t->output_count = 0;
}

/*
 * 自动 realloc 扩容 inputs 数组。
 */
void add_txin(Tx* tx, const unsigned char txid[32], uint32_t index) {

    tx->inputs = realloc(tx->inputs, sizeof(TxIn) * (tx->input_count + 1));

    // 填充输入字段
    memcpy(tx->inputs[tx->input_count].txid, txid, 32);
    tx->inputs[tx->input_count].output_index = index;

    // 签名与公钥先为空，由钱包模块填写
    tx->inputs[tx->input_count].sig_len = 0;
    tx->inputs[tx->input_count].pubkey_len = 0;

    tx->input_count++;
}

/*
 * 使用 realloc 自动扩容 outputs 数组。
 */
void add_txout(Tx* tx, const char* addr, uint32_t amount) {

    tx->outputs = realloc(tx->outputs, sizeof(TxOut) * (tx->output_count + 1));

    // 安全复制地址（保证以 '\0' 结尾）
    strncpy(tx->outputs[tx->output_count].addr, addr, sizeof(tx->outputs[tx->output_count].addr)-1);
    tx->outputs[tx->output_count].addr[sizeof(tx->outputs[tx->output_count].addr)-1] = '\0';

    tx->outputs[tx->output_count].amount = amount;

    tx->output_count++;
}

// 释放内存
void free_tx(Tx* tx) {
    if (tx->inputs) {
        free(tx->inputs);
        tx->inputs = NULL;
    }
    if (tx->outputs) {
        free(tx->outputs);
        tx->outputs = NULL;
    }

    tx->input_count = 0;
    tx->output_count = 0;

}


/*
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
*/