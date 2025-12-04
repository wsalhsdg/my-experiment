#pragma once
#include <stdbool.h>
#include <p2p/p2p.h>
#include "core/tx_pool.h"
#include "core/utxo_set.h"
#include "core/block.h"
#include "wallet/wallet.h"
#define MAX_PENDING_TX 1024  // 根据需要，可改成更大或更小


extern P2PNetwork g_net;
extern TxPool g_txpool;
extern UTXOSet g_utxos;
extern Blockchain g_chain;
extern Wallet g_wallet;
extern bool g_wallet_loaded;


typedef struct {
    Transaction txs[MAX_PENDING_TX];
    size_t count;
} PendingTxPool;

extern PendingTxPool g_pending_txpool;