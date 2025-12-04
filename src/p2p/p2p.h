#pragma once
#include <stdint.h>
#include <stddef.h>
#include "core/block.h"
#include "core/transaction.h"

#define MAX_PEERS 16
#define MSG_BUF_SIZE 4096

// 消息类型
typedef enum {
    MSG_PING = 1,
    MSG_PONG = 2,
    MSG_TX = 3,
    MSG_BLOCK = 4,
    MSG_INV = 5,
    
} MessageType;

// P2P 消息
typedef struct {
    MessageType type;
    size_t payload_len;
    uint8_t payload[MSG_BUF_SIZE];
} Message;

// 节点信息
typedef struct {
    char ip[64];
    uint16_t port;
    int sockfd;
} Peer;

// P2P 系统状态
typedef struct {
    Peer peers[MAX_PEERS];
    size_t peer_count;
    uint16_t listen_port;
} P2PNetwork;

// -----------------------------
// 接口函数
// -----------------------------
void p2p_init(P2PNetwork* net);
int p2p_add_peer(P2PNetwork* net, const char* ip, uint16_t port);
int p2p_send_message(Peer* peer, const Message* msg);
void p2p_broadcast(P2PNetwork* net, const Message* msg);
void* p2p_server_thread(void* arg);
// 线程入口函数
void* p2p_handle_incoming(void* arg);

