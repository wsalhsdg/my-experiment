#include "transaction.h"
#include <string.h>
#include <stdio.h>
//#include <openssl/sha.h>
#include "crypto/crypto_tools.h"


extern void sha256(const uint8_t* data, size_t len, uint8_t out32[32]);

static void sha256d(const uint8_t* data, size_t len, uint8_t out[TXID_LEN]) {
    uint8_t tmp[32];
    sha256(data, len, tmp);
    sha256(tmp, 32, out);
}

void transaction_init(Transaction* tx,
    const TxIn* inputs,
    size_t input_count,
    const TxOut* outputs,
    size_t output_count)
{
    if (!tx) return;
    memset(tx, 0, sizeof(Transaction));

    if (inputs && input_count > 0) {
        if (input_count > MAX_TX_INPUTS) input_count = MAX_TX_INPUTS;
        tx->input_count = input_count;
        for (size_t i = 0; i < input_count; i++) {
            tx->inputs[i] = inputs[i];
        }
    }

    if (outputs && output_count > 0) {
        if (output_count > MAX_TX_OUTPUTS) output_count = MAX_TX_OUTPUTS;
        tx->output_count = output_count;
        for (size_t i = 0; i < output_count; i++) {
            tx->outputs[i] = outputs[i];
        }
    }

    transaction_compute_txid(tx);
}

void transaction_init_with_change(Transaction* tx,
    const TxIn* inputs,
    size_t input_count,
    const TxOut* outputs,
    size_t output_count,
    uint64_t total_input_value,
    const char* change_address)
{
    transaction_init(tx, inputs, input_count, outputs, output_count);

    uint64_t total_output = 0;
    for (size_t i = 0; i < tx->output_count; i++) {
        total_output += tx->outputs[i].value;
    }

    if (total_input_value > total_output && tx->output_count < MAX_TX_OUTPUTS) {
        TxOut change;
        memset(&change, 0, sizeof(change));
        change.value = total_input_value - total_output;
        if (change_address) {
            strncpy(change.address, change_address, BTC_ADDRESS_MAXLEN - 1);
            change.address[BTC_ADDRESS_MAXLEN - 1] = '\0';
        }
        tx->outputs[tx->output_count++] = change;
    }

    transaction_compute_txid(tx);
}

void transaction_compute_txid(Transaction* tx) {
    if (!tx) return;

    /* 将所有 inputs & outputs 序列化到 buffer，再做双 sha256 */
    uint8_t buf[4096];
    size_t off = 0;

    // inputs
    memcpy(buf + off, &tx->input_count, sizeof(tx->input_count));
    off += sizeof(tx->input_count);

    for (size_t i = 0; i < tx->input_count; i++) {
        memcpy(buf + off, tx->inputs[i].txid, TXID_LEN);
        off += TXID_LEN;
        memcpy(buf + off, &tx->inputs[i].vout, sizeof(tx->inputs[i].vout));
        off += sizeof(tx->inputs[i].vout);
    }

    // outputs
    memcpy(buf + off, &tx->output_count, sizeof(tx->output_count));
    off += sizeof(tx->output_count);

    for (size_t i = 0; i < tx->output_count; i++) {
        memcpy(buf + off, &tx->outputs[i].value, sizeof(tx->outputs[i].value));
        off += sizeof(tx->outputs[i].value);
        memcpy(buf + off, tx->outputs[i].address, BTC_ADDRESS_MAXLEN);
        off += BTC_ADDRESS_MAXLEN;
    }

    // 计算 double-sha256
    sha256d(buf, off, tx->txid);
}

void transaction_print(const Transaction* tx) {
    if (!tx) return;

    printf("Transaction:\n");
    printf("  Inputs (%zu):\n", tx->input_count);
    for (size_t i = 0; i < tx->input_count; i++) {
        printf("    [%zu] txid=", i);
        for (int j = 0; j < TXID_LEN; j++) printf("%02x", tx->inputs[i].txid[j]);
        printf(", vout=%u\n", tx->inputs[i].vout);
    }

    printf("  Outputs (%zu):\n", tx->output_count);
    for (size_t i = 0; i < tx->output_count; i++) {
        printf("    [%zu] value=%llu, addr=%s\n", i, (unsigned long long)tx->outputs[i].value, tx->outputs[i].address);
    }

    printf("  TxID: ");
    for (int i = 0; i < TXID_LEN; i++) printf("%02x", tx->txid[i]);
    printf("\n");

    // 简要打印每个输入的签名长度
    for (size_t i = 0; i < tx->input_count; i++) {
        printf("  Input[%zu] sig_len=%zu\n", i, tx->sig_lens[i]);
    }
}

void transaction_hash_for_sign(const Transaction* tx, size_t in_index, uint8_t out32[32]) {
    /* 简化的签名哈希：将整个 txid + 输入索引序列化并 sha256
       实际比特币使用更复杂的 sighash 算法，这里为简化方便测试实现 */
    if (!tx || in_index >= tx->input_count) {
        memset(out32, 0, 32);
        return;
    }

    uint8_t buf[128];
    size_t off = 0;
    // 使用 txid（已包含 inputs/outputs）+ 输入索引
    memcpy(buf + off, tx->txid, TXID_LEN);
    off += TXID_LEN;
    memcpy(buf + off, &in_index, sizeof(in_index));
    off += sizeof(in_index);

    sha256d(buf, off, out32);
}

int transaction_sign(Transaction* tx, const uint8_t* privkey32) {
    if (!tx || !privkey32) return 0;

    // 为每个输入生成签名
    for (size_t i = 0; i < tx->input_count; i++) {
        // 计算哈希
        uint8_t hash32[32];
        transaction_hash_for_sign(tx, i, hash32);

        // 获取公钥（压缩33字节）
        if (!crypto_secp_get_pubkey(privkey32, tx->pubkeys[i])) {
            printf("transaction_sign_all: failed to get pubkey for input %zu\n", i);
            return 0;
        }

        size_t sig_len = 0;
        if (!crypto_secp_sign(privkey32, hash32, tx->signatures[i], &sig_len)) {
            printf("transaction_sign_all: failed to sign input %zu\n", i);
            return 0;
        }
        if (sig_len > MAX_SIG_LEN) {
            printf("transaction_sign_all: signature too long for input %zu\n", i);
            return 0;
        }
        tx->sig_lens[i] = sig_len;
    }

    return 1;
}

int transaction_verify(const Transaction* tx) {
    if (!tx) return 0;

    for (size_t i = 0; i < tx->input_count; i++) {
        uint8_t hash32[32];
        transaction_hash_for_sign(tx, i, hash32);

        const uint8_t* pub = tx->pubkeys[i];
        const uint8_t* sig = tx->signatures[i];
        size_t siglen = tx->sig_lens[i];

        if (siglen == 0) {
            printf("transaction_verify: missing signature for input %zu\n", i);
            return 0;
        }

        if (!crypto_secp_verify(pub, hash32, sig, siglen)) {
            printf("transaction_verify: signature verify failed for input %zu\n", i);
            return 0;
        }
    }

    return 1;
}


size_t transaction_serialize(const Transaction* tx, uint8_t* out, size_t maxlen)
{
    if (!tx || !out) return 0;

    size_t off = 0;

    // ---- 写 input_count ----
    if (off + 1 > maxlen) return 0;
    out[off++] = (uint8_t)tx->input_count;

    // ---- 写 inputs ----
    for (size_t i = 0; i < tx->input_count; i++) {

        if (off + TXID_LEN + sizeof(uint32_t) > maxlen) return 0;

        memcpy(out + off, tx->inputs[i].txid, TXID_LEN);
        off += TXID_LEN;

        memcpy(out + off, &tx->inputs[i].vout, sizeof(uint32_t));
        off += sizeof(uint32_t);
    }

    // ---- 写 output_count ----
    if (off + 1 > maxlen) return 0;
    out[off++] = (uint8_t)tx->output_count;

    // ---- 写 outputs ----
    for (size_t i = 0; i < tx->output_count; i++) {

        if (off + sizeof(uint64_t) + BTC_ADDRESS_MAXLEN > maxlen) return 0;

        memcpy(out + off, &tx->outputs[i].value, sizeof(uint64_t));
        off += sizeof(uint64_t);

        memcpy(out + off, tx->outputs[i].address, BTC_ADDRESS_MAXLEN);
        off += BTC_ADDRESS_MAXLEN;
    }

    // ---- 写每个输入的 pubkey + signature ----
    for (size_t i = 0; i < tx->input_count; i++) {

        uint16_t siglen = tx->sig_lens[i];

        if (siglen > MAX_SIG_LEN) return 0;

        if (off + MAX_PUBKEY_LEN + sizeof(uint16_t) + siglen > maxlen)
            return 0;

        memcpy(out + off, tx->pubkeys[i], MAX_PUBKEY_LEN);
        off += MAX_PUBKEY_LEN;

        memcpy(out + off, &siglen, sizeof(uint16_t));
        off += sizeof(uint16_t);

        memcpy(out + off, tx->signatures[i], siglen);
        off += siglen;
    }

    return off;
}


/* 反序列化：按照同样顺序读取 */
int transaction_deserialize(Transaction* tx, const uint8_t* data, size_t len)
{
    if (!tx || !data) return 0;

    memset(tx, 0, sizeof(Transaction));

    size_t off = 0;

    // ---- input_count ----
    if (off + 1 > len) return 0;
    tx->input_count = data[off++];

    if (tx->input_count > MAX_TX_INPUTS) return 0;

    // ---- inputs ----
    for (size_t i = 0; i < tx->input_count; i++) {

        if (off + TXID_LEN + sizeof(uint32_t) > len) return 0;

        memcpy(tx->inputs[i].txid, data + off, TXID_LEN);
        off += TXID_LEN;

        memcpy(&tx->inputs[i].vout, data + off, sizeof(uint32_t));
        off += sizeof(uint32_t);
    }

    // ---- output_count ----
    if (off + 1 > len) return 0;
    tx->output_count = data[off++];

    if (tx->output_count > MAX_TX_OUTPUTS) return 0;

    // ---- outputs ----
    for (size_t i = 0; i < tx->output_count; i++) {

        if (off + sizeof(uint64_t) + BTC_ADDRESS_MAXLEN > len)
            return 0;

        memcpy(&tx->outputs[i].value, data + off, sizeof(uint64_t));
        off += sizeof(uint64_t);

        memcpy(tx->outputs[i].address, data + off, BTC_ADDRESS_MAXLEN);
        off += BTC_ADDRESS_MAXLEN;
    }

    // ---- 公钥 + 签名 ----
    for (size_t i = 0; i < tx->input_count; i++) {

        if (off + MAX_PUBKEY_LEN + sizeof(uint16_t) > len) return 0;

        memcpy(tx->pubkeys[i], data + off, MAX_PUBKEY_LEN);
        off += MAX_PUBKEY_LEN;

        uint16_t siglen = 0;
        memcpy(&siglen, data + off, sizeof(uint16_t));
        off += sizeof(uint16_t);

        if (siglen > MAX_SIG_LEN) return 0;

        if (off + siglen > len) return 0;

        memcpy(tx->signatures[i], data + off, siglen);
        off += siglen;

        tx->sig_lens[i] = siglen;
    }

    // 最后重新计算 txid（保持一致）
    transaction_compute_txid(tx);

    return 1;
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


//      签名交易（privkey32: 32 字节私钥）
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


//      验证交易签名
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
*/