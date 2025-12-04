#include "global.h"

// 真正定义变量
P2PNetwork g_net;
TxPool g_txpool;
UTXOSet g_utxos;
Blockchain g_chain;
Wallet g_wallet;
bool g_wallet_loaded = false;
PendingTxPool g_pending_txpool = { 0 };