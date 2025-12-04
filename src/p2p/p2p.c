#include "p2p.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <core/tx_pool.h>
#include <global/global.h>



extern int txpool_add(TxPool* pool, const Transaction* tx, const UTXOSet* utxos);

// 检查交易输入是否存在于本地 UTXO
int utxo_exist(const UTXOSet* utxos, const TxIn* inputs, size_t input_count);

// 同步缺失 UTXO
int sync_utxos(Peer* peer, const Transaction* tx);

int validate_block(const Block* blk);

void p2p_init(P2PNetwork* net) {
    if (!net) return;
    memset(net, 0, sizeof(P2PNetwork));
}

// Server thread: accept incoming connections
void* p2p_server_thread(void* arg) {
    P2PNetwork* net = (P2PNetwork*)arg;
    if (!net) return NULL;

    int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0) {
        perror("socket");
        return NULL;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(net->listen_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listen_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(listen_sock);
        return NULL;
    }

    if (listen(listen_sock, 5) < 0) {
        perror("listen");
        close(listen_sock);
        return NULL;
    }

    printf("[P2P] Listening on port %d...\n", net->listen_port);

    while (1) {
        int client_sock = accept(listen_sock, NULL, NULL);
        if (client_sock < 0) {
            perror("accept");
            continue;
        }

        if (net->peer_count >= MAX_PEERS) {
            printf("[P2P] Max peers reached, rejecting connection\n");
            close(client_sock);
            continue;
        }

        Peer* peer = &net->peers[net->peer_count++];
        peer->sockfd = client_sock;
        peer->port = 0;
        strncpy(peer->ip, "incoming", sizeof(peer->ip) - 1);

        pthread_t tid;
        pthread_create(&tid, NULL, p2p_handle_incoming, peer);
        pthread_detach(tid);

        printf("[P2P] Accepted incoming connection, total peers=%zu\n", net->peer_count);
    }

    close(listen_sock);
    return NULL;
}
//client connect to peer
int p2p_add_peer(P2PNetwork* net, const char* ip, uint16_t port) {
    if (!net || net->peer_count >= MAX_PEERS) return -1;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return -1;
    }

    Peer* peer = &net->peers[net->peer_count++];
    peer->sockfd = sock;
    peer->port = port;
    strncpy(peer->ip, ip, sizeof(peer->ip) - 1);

    pthread_t tid;
    pthread_create(&tid, NULL, p2p_handle_incoming, peer);
    pthread_detach(tid);


    printf("[P2P] Connected to peer %s:%d\n", ip, port);
    return 0;
}

int p2p_send_message(Peer* peer, const Message* msg) {
    if (!peer || !msg) return -1;
    // 先发送消息长度
    uint32_t len = htonl(sizeof(MessageType) + sizeof(size_t) + msg->payload_len);
    if (write(peer->sockfd, &len, sizeof(len)) != sizeof(len)) return -1;
    // 发送完整消息
    ssize_t n = write(peer->sockfd, msg, sizeof(MessageType) + sizeof(size_t) + msg->payload_len);
    if (n != (ssize_t)(sizeof(MessageType) + sizeof(size_t) + msg->payload_len))
        return -1;
    return 0;
}

void p2p_broadcast(P2PNetwork* net, const Message* msg) {
    for (size_t i = 0; i < net->peer_count; i++) {
        p2p_send_message(&net->peers[i], msg);
    }
}

int utxo_exist(const UTXOSet* utxos, const TxIn* inputs, size_t input_count) {
    for (size_t i = 0; i < input_count; i++) {
        if (!utxo_set_find((UTXOSet*)utxos, inputs[i].txid, inputs[i].vout)) { 
            return 0;
        }
    }
    return 1;
}

// 简单同步逻辑：向发送交易的 peer 请求缺失的区块或交易
/*int sync_utxos(Peer* peer, const Transaction* tx) {
    printf("[P2P] Syncing UTXOs for txid=");
    for (int i = 0; i < 4; i++) printf("%02x", tx->txid[i]);
    printf(" from peer %s:%d\n", peer->ip, peer->port);

    // 这里我们可以发送一个 MSG_GETUTXO 消息给 peer
    // 假设 MessageType MSG_GETUTXO = 10
    Message msg = { 0 };
    msg.type = MSG_GETUTXO;
    msg.payload_len = sizeof(Transaction);
    memcpy(msg.payload, tx, sizeof(Transaction));

    if (p2p_send_message(peer, &msg) < 0) {
        printf("[P2P] Failed to request UTXO sync\n");
        return 0;
    }

    // 等待同步完成（简单占位，实际可以做异步更新）
    // 在 demo 中先返回 0 表示还未同步完成
    return 0;
}*/

int validate_block(const Block* blk) {
    if (!blk) return 0;
    // 可以检查：前哈希是否存在、Merkle Root 是否正确、时间戳、大小等
    // 实验/简化版可以直接返回 1
    return 1;
}

int pending_txpool_add(PendingTxPool* pool, const Transaction* tx) {
    if (!pool || !tx || pool->count >= MAX_PENDING_TX) return -1;
    pool->txs[pool->count++] = *tx;
    return 0;
}

int pending_txpool_remove(PendingTxPool* pool, const Transaction* tx) {
    if (!pool || !tx) return -1;
    for (size_t i = 0; i < pool->count; i++) {
        if (memcmp(pool->txs[i].txid, tx->txid, TXID_LEN) == 0) {
            pool->txs[i] = pool->txs[pool->count - 1];
            pool->count--;
            return 0;
        }
    }
    return -1;
}


// 简单消息处理（可在独立线程中运行）
void* p2p_handle_incoming(void* arg) {
    Peer* peer = (Peer*)arg;
    if (!peer) return NULL;

    printf("[Thread %lu] Start handling incoming messages from %s:%d\n",
        pthread_self(), peer->ip, peer->port);

    while (1) {
        uint32_t len_net;
        ssize_t n = read(peer->sockfd, &len_net, sizeof(len_net));
        if (n <= 0) {
            printf("[P2P] Peer disconnected %s:%d\n", peer->ip, peer->port);
            break;
        }
        uint32_t len = ntohl(len_net);
        if (len > MSG_BUF_SIZE + sizeof(MessageType) + sizeof(size_t)) {
            printf("[P2P] Message too large from %s:%d\n", peer->ip, peer->port);
            break;
        }

        Message msg;
        n = read(peer->sockfd, &msg, len);
        if (n <= 0) {
            printf("[P2P] Failed to read full message from %s:%d\n", peer->ip, peer->port);
            break;
        }

        printf("[Thread %lu] Received message type %d, payload_len=%zu from %s:%d\n",
            pthread_self(), msg.type, msg.payload_len, peer->ip, peer->port);

        // 根据消息类型处理
        switch (msg.type) {
        case MSG_PING: {
            printf("[P2P] Responding PONG to %s:%d\n", peer->ip, peer->port);
            Message pong = { .type = MSG_PONG, .payload_len = 0 };
            p2p_send_message(peer, &pong);
            break;
        }
        case MSG_PONG: {
            printf("[P2P] Received PONG from %s:%d\n", peer->ip, peer->port);
            break;
        }
        case MSG_TX: {
            if (msg.payload_len != sizeof(Transaction)) break;
            Transaction tx_recv;
            memcpy(&tx_recv, msg.payload, sizeof(Transaction));

            // 检查本地 UTXO 是否满足交易输入
            if (utxo_exist(&g_utxos, tx_recv.inputs, tx_recv.input_count)) {
                // 输入存在，加入 mempool
                if (txpool_add(&g_txpool, &tx_recv, &g_utxos)) {
                    printf("[P2P] TX added to mempool (from %s:%d). txid=", peer->ip, peer->port);
                    for (int k = 0; k < 4; k++) printf("%02x", tx_recv.txid[k]);
                    printf("...\n");

                    // 广播给其他节点
                    p2p_broadcast(&g_net, &msg);
                }
                else {
                    printf("[P2P] TX rejected or already in mempool\n");
                }
            }
            else {
                // 输入缺失 → 暂存到 pending_tx_pool 等待区块同步
                printf("[P2P] TX input missing in local UTXO, storing in pending_tx_pool, waiting for block sync...\n");
                pending_txpool_add(&g_pending_txpool, &tx_recv);
            }
            break;
        }
        
        case MSG_BLOCK: {
            Block blk;
            memcpy(&blk, msg.payload, sizeof(Block));

            // 1. 验证区块前哈希和时间戳等
            if (!validate_block(&blk)) break;

            // 2. 遍历区块里的交易更新 UTXO
            for (size_t i = 0; i < blk.tx_count; i++) {
                utxo_set_update_from_tx(&g_utxos, &blk.txs[i]);

                // 3. 如果交易在本地 mempool，也要移除
                txpool_remove(&g_txpool, blk.txs[i].txid);
            }
            // 遍历 pending_tx_pool，尝试加入 mempool
            for (size_t i = 0; i < g_pending_txpool.count; i++) {
                Transaction* pending_tx = &g_pending_txpool.txs[i];
                if (utxo_exist(&g_utxos, pending_tx->inputs, pending_tx->input_count)) {
                    if (txpool_add(&g_txpool, pending_tx, &g_utxos)) {
                        printf("[P2P] Pending TX now valid, added to mempool. txid=");
                        for (int k = 0; k < 4; k++) printf("%02x", pending_tx->txid[k]);
                        printf("...\n");

                        // 广播给其他节点
                        Message tx_msg = { 0 };
                        tx_msg.type = MSG_TX;
                        tx_msg.payload_len = sizeof(Transaction);
                        memcpy(tx_msg.payload, pending_tx, sizeof(Transaction));
                        p2p_broadcast(&g_net, &tx_msg);

                        // 从 pending_tx_pool 删除已处理的交易
                        pending_txpool_remove(&g_pending_txpool, pending_tx);
                        i--; // 调整索引
                    }
                }
            }
            // 4. 将区块加入本地区块链
            blockchain_add_block(&g_chain, &blk);

            printf("[P2P] Block processed, UTXO set and mempool updated\n");

            break;
        }
        


        }
    }
    close(peer->sockfd);
    return NULL;
}

