#ifndef P2P_H
#define P2P_H

#include <stdint.h>
#include <stdint.h>
#include <stddef.h>
#include "core/transaction.h"
#include "core/block/block.h"
#include "core/utxo_set.h"
#include "core/tx_pool.h"
#include "core/block/blockchain.h"
#include "global/global.h"

//extern Mempool mempool;
//extern UTXO* utxo_set;
//extern Blockchain* blockchain;

// ----创建交易----
Tx* create_transaction(
    UTXO** utxo_set,                //全局 UTXO 集
    Mempool* mempool,               //交易池
    const char* from_addr,          //发送地址
    const char* to_addr,            //接收地址
    uint64_t amount,                //价格
    const unsigned char* privkey);  //签名私钥
#endif

// ----启动 P2P 服务器监听----
void start_server(int port);

// ----发起连接到其他节点----
int connect_to_peer(const char* ip, int port);

// ----关闭所有网络连接----
void p2pstop(void);

// ----广播区块 ----
void broadcast_block(Block* b);

// ----广播交易----
void broadcast_tx(Tx* tx);

// ----把交易产生的UTXO保存的本地----
void block_utxo_update(Block* block);

// ----广播钱包地址----
void broadcast_addresss(const char* addr, const unsigned char* pubkey);

// ----设置钱包的地址----
void set_node_address(const char* addr, const unsigned char* pubkey);


