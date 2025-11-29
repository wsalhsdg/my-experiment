#include "utxo_set.h"
#include <string.h>

void utxo_init(UTXOSet* set) {
    set->has_tx = 0;
}

void utxo_add(UTXOSet* set, const Transaction* tx) {
    memcpy(&set->last_tx, tx, sizeof(Transaction));
    set->has_tx = 1;
}
