#include "p2p.h"

#include <arpa/inet.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "wallet/wallet.h"
#include "core/utxo.h"
#include "global/global.h"


#define MAX_PEERS 16

// P2P协议消息类型
#define MSG_TX     1
#define MSG_BLOCK  2
#define MSG_EXIT   3
#define MSG_ADDR   4 
//extern char addr[128];
//extern Mempool mempool;
//extern UTXO* utxo_set;
//extern Blockchain* blockchain;
//extern unsigned char priv[32];

typedef struct {
    uint8_t type;
    uint32_t length;
} MsgHeader;

// ----全局状态----
char serveraddr[128];
int peers[MAX_PEERS];
int peer_count = 0;

// 为每个 peer 保存 wallet address 和 pubkey（与 peers[] 索引一一对应）
char peer_addrs[MAX_PEERS][128];
unsigned char peer_pubkeys[MAX_PEERS][65];

volatile int p2p_running = 1;
int listenfd_global = -1;
int is_server = 0;
//char node_addr[128] = { 0 };
//unsigned char node_pubkey[65] = { 0 };
//size_t node_pubkey_len = 65;
Tx* tx = NULL;
pthread_mutex_t peers_lock = PTHREAD_MUTEX_INITIALIZER;


//extern Mempool mempool;
//extern UTXO* utxo_set;
//extern Blockchain* blockchain;


// 同步缺失 UTXO
//int sync_utxos(Peer* peer, const Transaction* tx);


// ----peer查询----
static int find_peer(int sock) {
    int idx = -1;
    pthread_mutex_lock(&peers_lock);
    for (int i = 0; i < peer_count; i++) {
        if (peers[i] == sock) { idx = i; break; }
    }
    pthread_mutex_unlock(&peers_lock);
    return idx;
}

// ---- Tx 序列化 ----
unsigned char* serialize_tx(Tx* tx, size_t* out_len) {
    // 预估序列化后的长度
    size_t len = 
        sizeof(uint32_t) 
        + tx->input_count * (32 + sizeof(uint32_t) + 64 + sizeof(size_t) + 65 + sizeof(size_t))
        + sizeof(uint32_t) 
        + tx->output_count * (35 + sizeof(uint32_t)) 
        + 32;

    unsigned char* buf = malloc(len);
    if (!buf) return NULL;
    memset(buf, 0, len);  // 清零缓冲区

    unsigned char* p = buf;

    // 写入输入
    memcpy(p, &tx->input_count, sizeof(uint32_t)); 
    p += sizeof(uint32_t);

    for (uint32_t i = 0; i < tx->input_count; i++) {
        TxIn* in = &tx->inputs[i];
        memcpy(p, in->txid, 32); 
        p += 32;
        memcpy(p, &in->output_index, sizeof(uint32_t)); 
        p += sizeof(uint32_t);
        memcpy(p, in->signature, 64); 
        p += 64;
        memcpy(p, &in->sig_len, sizeof(size_t)); 
        p += sizeof(size_t);
        memcpy(p, in->pubkey, 65); 
        p += 65;
        memcpy(p, &in->pubkey_len, sizeof(size_t)); 
        p += sizeof(size_t);
    }

    // 写入输出
    memcpy(p, &tx->output_count, sizeof(uint32_t)); 
    p += sizeof(uint32_t);

    for (uint32_t i = 0; i < tx->output_count; i++) {
        TxOut* out = &tx->outputs[i];
        memcpy(p, out->addr, 35); 
        p += 35;
        memcpy(p, &out->amount, sizeof(uint32_t)); 
        p += sizeof(uint32_t);
    }

    // 写入 txid
    memcpy(p, tx->txid, 32); 
    p += 32;

    *out_len = len;
    return buf;
}

// ----反序列化 Tx 结构----
Tx* deserialize_tx(unsigned char* buf, size_t len) {

    if (!buf || len < sizeof(uint32_t)) return NULL;
    unsigned char* p = buf;
    Tx* tx = malloc(sizeof(Tx));
    if (!tx) return NULL;
    memset(tx, 0, sizeof(Tx));

    memcpy(&tx->input_count, p, sizeof(uint32_t)); 
    p += sizeof(uint32_t);

    if (tx->input_count > 0) {
        tx->inputs = malloc(sizeof(TxIn) * tx->input_count);
        memset(tx->inputs, 0, sizeof(TxIn) * tx->input_count);
    }
    else tx->inputs = NULL;

    for (uint32_t i = 0; i < tx->input_count; i++) {
        TxIn* in = &tx->inputs[i];

        memcpy(in->txid, p, 32); p += 32;
        memcpy(&in->output_index, p, sizeof(uint32_t)); p += sizeof(uint32_t);
        memcpy(in->signature, p, 64); p += 64;
        memcpy(&in->sig_len, p, sizeof(size_t)); p += sizeof(size_t);
        memcpy(in->pubkey, p, 65); p += 65;
        memcpy(&in->pubkey_len, p, sizeof(size_t)); p += sizeof(size_t);
    }

    memcpy(&tx->output_count, p, sizeof(uint32_t));
    p += sizeof(uint32_t);

    if (tx->output_count > 0) {
        tx->outputs = malloc(sizeof(TxOut) * tx->output_count);
        memset(tx->outputs, 0, sizeof(TxOut) * tx->output_count);
    }
    else tx->outputs = NULL;

    for (uint32_t i = 0; i < tx->output_count; i++) {
        TxOut* out = &tx->outputs[i];
        memcpy(out->addr, p, 35); p += 35;
        memcpy(&out->amount, p, sizeof(uint32_t)); p += sizeof(uint32_t);
    }

    memcpy(tx->txid, p, 32); p += 32;
    return tx;
}

// ---- 区块序列化/反序列化 ----
unsigned char* serialize_block(Block* b, size_t* out_len) 
{
    size_t len = sizeof(BlockHeader);

    // 预计算区块所有交易所需的空间
    for (uint32_t i = 0; i < b->tx_count; i++) {
        size_t tx_size;
        unsigned char* tx_buf = serialize_tx(&b->txs[i], &tx_size);
        len += sizeof(uint32_t) + tx_size;
        free(tx_buf);
    }

    unsigned char* buf = malloc(len);
    if (!buf) return NULL;
    memset(buf, 0, len);  // 清零缓冲区
    unsigned char* p = buf;

    memcpy(p, &b->header, sizeof(BlockHeader)); p += sizeof(BlockHeader);
    for (uint32_t i = 0; i < b->tx_count; i++) {
        size_t tx_size;
        unsigned char* tx_buf = serialize_tx(&b->txs[i], &tx_size);
        memcpy(p, &tx_size, sizeof(uint32_t)); p += sizeof(uint32_t);
        memcpy(p, tx_buf, tx_size); p += tx_size;
        free(tx_buf);
    }

    *out_len = len;
    return buf;
}

// 反序列化区块
Block* deserialize_block(unsigned char* buf, size_t len) {
    if (!buf || len < sizeof(BlockHeader)) return NULL;
    
    Block* b = malloc(sizeof(Block));
    unsigned char* p = buf;

    if (!b) return NULL;
    memset(b, 0, sizeof(Block));
    // 区块头
    memcpy(&b->header, p, sizeof(BlockHeader)); 
    p += sizeof(BlockHeader);

    b->tx_count = 0;
    b->txs = NULL;

    // ---- 计算交易数量 ----
    unsigned char* q = p;
    while (q < buf + len) {
        uint32_t tx_size;
        memcpy(&tx_size, q, sizeof(uint32_t));
        q += sizeof(uint32_t) + tx_size;
        b->tx_count++;
    }

    //申请空间
    if (b->tx_count > 0) {
        b->txs = malloc(sizeof(Tx) * b->tx_count);
        memset(b->txs, 0, sizeof(Tx) * b->tx_count);
    }
    else b->txs = NULL;

    // ---- 解析每笔交易 ----
    for (uint32_t i = 0; i < b->tx_count; i++) {
        uint32_t tx_size;
        memcpy(&tx_size, p, sizeof(uint32_t)); p += sizeof(uint32_t);
        Tx* tx = deserialize_tx(p, tx_size);
        if (tx) {
            b->txs[i] = *tx; 
            free(tx);
        }
        p += tx_size;
    }

    return b;
}

// ---- 发送消息（头 + payload） ----
void send_message(int sock, uint8_t type, const unsigned char* data, uint32_t len) 
{
    MsgHeader hdr = { type, len };
    send(sock, &hdr, sizeof(hdr), 0);
    if (len > 0 && data) {
        send(sock, data, len, 0); 
    }
}

// ----广播节点身份----
void broadcast_addresss(const char* addr, const unsigned char* pubkey) {
    if (!addr || !pubkey) return;

    const uint32_t payload_len = 128 + 65;
    unsigned char* p = malloc(payload_len);

    if (!p) return;

    memset(p, 0, payload_len);
    strncpy((char*)p, addr, 127); 
    memcpy(p + 128, pubkey, 65);

    pthread_mutex_lock(&peers_lock);
    for (int i = 0; i < peer_count; i++) {
        send_message(peers[i], MSG_ADDR, p, payload_len);
    }
    pthread_mutex_unlock(&peers_lock);

    free(p);
}



// ----区块->UTXO更新----
void block_utxo_update(Block* block)
{
    for (uint32_t i = 0; i < block->tx_count; i++) {

        Tx* tx = &block->txs[i];

        // 移除已被花费的 UTXO
        for (uint32_t j = 0; j < tx->input_count; j++) {

            UTXO* prev = NULL, * cur = utxo_set;

            while (cur) {
                if (memcmp(cur->txid, tx->inputs[j].txid, 32) == 0 &&
                    cur->output_index == tx->inputs[j].output_index)
                {
                    if (prev) prev->next = cur->next;
                    else utxo_set = cur->next;

                    free(cur);
                    break;
                }
                prev = cur;
                cur = cur->next;
            }
        }

        // 添加当前 tx 的输出为新的 UTXO
        for (uint32_t m = 0; m < tx->output_count; m++) {
            add_utxo(&utxo_set, tx->txid, m,
                tx->outputs[m].addr,
                tx->outputs[m].amount);
        }
    }
}

// ----peer线程----
void* peer_thread(void* arg) 
{
    int sock = *(int*)arg;
    free(arg);

    int temp = 0;

    // 刚连接就给对方发送我们的地址信息
    broadcast_addresss(node_addr, node_pubkey);

    while (p2p_running) {

        MsgHeader hdr;
        int r = recv(sock, &hdr, sizeof(hdr), 0);
        if (r <= 0) break;

        unsigned char* payload = NULL;

        if (hdr.length > 0) {
            payload = malloc(hdr.length);
            if (!payload) break;
            r = recv(sock, payload, hdr.length, 0);
            if (r <= 0) {
                free(payload);
                break;
            }
        }
        //处理消息
        //对方退出 
        if (hdr.type == MSG_EXIT) {
            if (is_server) {
                pthread_mutex_lock(&peers_lock);

                for (int i = 0; i < peer_count; i++) {
                    if (peers[i] == sock) {

                        // 删除 peer：把最后一个挪过来覆盖
                        peers[i] = peers[peer_count - 1];

                        memcpy(peer_addrs[i], peer_addrs[peer_count - 1], 128);
                        memcpy(peer_pubkeys[i], peer_pubkeys[peer_count - 1], 65);

                        peer_count--;
                        break;
                    }
                }
                pthread_mutex_unlock(&peers_lock);
                printf("[P2P] Client %d disconnected.\n", sock);
            }
            else {
                printf("[P2P] Server exited.\n");
                free(payload);
                exit(0);
            }
            free(payload);
            break;
        }
        // 对方发送地址和公钥
        else if (hdr.type == MSG_ADDR) 
        {

            if (hdr.length >= 128 + 65 && payload) 
            {
                int idx = find_peer(sock);

                if (idx >= 0) 
                {
                    memcpy(peer_addrs[idx], payload, 128);
                    peer_addrs[idx][127] = '\0';
                    memcpy(peer_pubkeys[idx], payload + 128, 65);
                    
                    if (is_server) {
                        if (temp == 0) {
                            temp = 1;
                            printf("[P2P] Peer %d address: %s\n", sock, peer_addrs[idx]);

                        }
                    }

                    // 客户端拿到地址后可以开始构建区块
                    else 
                    {
                        if (temp == 0) 
                        {
                            temp = 1;
                            printf("[P2P] Server address: %s\n", peer_addrs[idx]);
                            MempoolTx* cur = mempool.head;
                            while (cur) {

                                // 获取链尾
                                Blockchain* cur_chain = blockchain;
                                while (cur_chain->next) 
                                    cur_chain = cur_chain->next;
                                Block* prev_block = cur_chain->block;

                                // 创建新区块（每个区块一个交易，可改为多个交易）
                                Block* b = create_block(prev_block->header.block_hash, cur->tx, 1);

                                blockchain = blockchain_add(blockchain, b);
                                cur = cur->next;
                            }
                        }
                    }
                }
            }
        }

        // 对方发送区块
        else if (hdr.type == MSG_BLOCK) {

            Block* blk = deserialize_block(payload, hdr.length);
            if (blk) {

                // 校验区块合法性
                Blockchain* last_chain = blockchain;
                while (last_chain && last_chain->next) 
                    last_chain = last_chain->next;

                Block* prev_block = last_chain ? last_chain->block : NULL;

                if (verify_block(blk, prev_block)) {
                    blockchain = blockchain_add(blockchain, blk);
                    block_utxo_update(blk);
                    printf("[P2P] Block added from peer %d.\n", sock);
                }
                else {
                    printf("[P2P] Invalid block received.\n");
                    free(blk);//释放区块
                }
            }
        }
        if (payload) free(payload);//释放
    }
    close(sock);
    return NULL;
}


// ----服务器线程：接受连接----
void* p2p_server_thread(void* arg) {
    int port = *(int*)arg;
    free(arg);

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    listenfd_global = listenfd;

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(listenfd, (struct sockaddr*)&addr, sizeof(addr));
    listen(listenfd, 10);

    printf("[P2P] Listening on port %d...\n", port);

    while (p2p_running) {
        int client = accept(listenfd, NULL, NULL);
        if (client < 0) continue;

        pthread_mutex_lock(&peers_lock);
        if (peer_count < MAX_PEERS) {
            peers[peer_count] = client;
            memset(peer_addrs[peer_count], 0, sizeof(peer_addrs[peer_count]));
            memset(peer_pubkeys[peer_count], 0, sizeof(peer_pubkeys[peer_count]));

            int* s = malloc(sizeof(int)); 
            *s = client;

            pthread_t tid; 
            pthread_create(&tid, NULL, peer_thread, s);
            pthread_detach(tid);

            peer_count++;
        }
        else {
            close(client);
        }
        pthread_mutex_unlock(&peers_lock);
    }

    if (listenfd_global != -1) close(listenfd_global);//关闭
    listenfd_global = -1;

    return NULL;
}

//----启动服务端----
void start_server(int port) 
{
    is_server = 1;

    int* x = malloc(sizeof(int));
    *x = port;

    pthread_t tid; 
    pthread_create(&tid, NULL, p2p_server_thread, x);
    pthread_detach(tid);
}

// ----客户端功能：连接到远程节点----
int connect_to_peer(const char* ip, int port) 
{
    //创建TCP socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    // 字符串 IP → 二进制
    if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0) {
        printf("[P2P] ERROR: Invalid IP address: %s\n", ip);
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("[P2P] Connect failed: %s:%d\n", ip, port);
        return -1;
    }
    //新节点加入peers
    pthread_mutex_lock(&peers_lock);

    if (peer_count < MAX_PEERS) {

        peers[peer_count] = sock;
        //清空记录
        memset(peer_addrs[peer_count], 0, sizeof(peer_addrs[peer_count]));
        memset(peer_pubkeys[peer_count], 0, sizeof(peer_pubkeys[peer_count]));

        int* arg_sock = malloc(sizeof(int)); 
        *arg_sock = sock;

        pthread_t tid; 
        pthread_create(&tid, NULL, peer_thread, arg_sock);
        pthread_detach(tid);

        peer_count++;
    }
    else {
        // 已达到最大上限，拒绝
        printf("[P2P] WARNING: peer limit reached. Closing connection.\n");
        close(sock);
        sock = -1;
    }

    pthread_mutex_unlock(&peers_lock);

    if (sock != -1)
        printf("[P2P] Connected to %s:%d\n", ip, port);
   
    return sock;
}

// ----退出 P2P：关闭所有连接----
void p2pstop() 
{
    p2p_running = 0;
    pthread_mutex_lock(&peers_lock);

    for (int i = 0; i < peer_count; i++) {
        send_message(peers[i], MSG_EXIT, NULL, 0);
    }

    for (int i = 0; i < peer_count; i++) {
        close(peers[i]);
    }

    peer_count = 0;

    pthread_mutex_unlock(&peers_lock);

    if (listenfd_global != -1) {
        close(listenfd_global);
        listenfd_global = -1;
    }
    printf("[P2P] shutdown complete.\n");
}

// ---- 广播交易 ----
void broadcast_tx(Tx* tx) {
    (void)tx;
}

// ----广播区块----
void broadcast_block(Block* b) {
    size_t len;
    unsigned char* buf = serialize_block(b, &len);
    if (!buf) return;

    pthread_mutex_lock(&peers_lock);
    for (int i = 0; i < peer_count; i++)
        send_message(peers[i], MSG_BLOCK, buf, len);
    pthread_mutex_unlock(&peers_lock);

    free(buf);//释放
}

// ----设置节点身份----
void set_node_address(const char* addr, const unsigned char* pubkey) {
    strncpy(node_addr, addr, 127);
    node_addr[127] = '\0';

    memcpy(node_pubkey, pubkey, 65);
    printf("[P2P] Node identity set：Address=%s\n", node_addr);
}

//创造交易
Tx* create_transaction(
    UTXO** utxo_set,
    Mempool* mempool,
    const char* from_addr,
    const char* to_addr,
    uint64_t amount,
    const unsigned char* privkey)
{

    CoinSelection a;
    a.count = 0;
    a.total = 0;

    if (!select_coins(*utxo_set, from_addr, amount, &a)) {
        printf("Create TX failed! Insufficient funds!\n");
        return NULL;
    }
    printf("Select success：%d UTXO，total: %llu\n",
        a.count, (unsigned long long)a.total);
    //分配结构体
    Tx* tx = malloc(sizeof(Tx));
    if (!tx) {
        printf("Memory allocation failed for Tx\n");
        return NULL;
    }
    //初始化
    init_tx(tx);

    for (int i = 0; i < a.count; i++) {
        add_txin(tx, a.utxos[i]->txid, a.utxos[i]->output_index);
    }
    add_txout(tx, to_addr, amount);
    //进行找零
    uint64_t changes = a.total - amount;
    if (changes > 0) {
        add_txout(tx, from_addr, changes);
    }
    sign_tx(tx, privkey);
    //生成交易ID
    tx_hash(tx, tx->txid);
    //添加交易池
    mempool_add_tx(mempool, tx, *utxo_set);
    //更新utxo集
    update_utxo_set(utxo_set, tx, tx->txid);

    printf("Transaction created successfully!\n");
    printf("  Output: %llu\n", (unsigned long long)amount);
    printf("  Change: %llu\n", (unsigned long long)changes);

    return tx;
}