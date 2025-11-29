#pragma once
#include "transaction.h"

typedef struct {
    Transaction last_tx;
    int has_tx;
} UTXOSet;

void utxo_init(UTXOSet* set);
void utxo_add(UTXOSet* set, const Transaction* tx);
