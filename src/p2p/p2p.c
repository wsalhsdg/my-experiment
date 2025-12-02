#include "p2p.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

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
        case MSG_PONG:{
            printf("[P2P] Received PONG from %s:%d\n", peer->ip, peer->port);
            break;
        case MSG_TX:
            printf("[P2P] Received TX payload from %s:%d\n", peer->ip, peer->port);
            break;
        case MSG_BLOCK:
            printf("[P2P] Received BLOCK payload from %s:%d\n", peer->ip, peer->port);
            break;
        default:
            printf("[P2P] Unknown message type %d from %s:%d\n", msg.type, peer->ip, peer->port);
            break;
        }
        
        }
    }

    close(peer->sockfd);
    return NULL;
}
