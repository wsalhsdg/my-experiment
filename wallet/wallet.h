#ifndef WALLET_H
#define WALLET_H
#include <stddef.h>
#include <stdint.h>
#include "core/block.h"
#include "core/transaction.h"


//----随机生成私钥----//
void generate_privkey(unsigned char* priv);

//----私钥生成 WIF----//
void privkey_to_WIF(
	unsigned char* priv, 
	int priv_len, 
	int compressed, 
	char* out, 
	int outlen
);

//----私钥生成公钥和地址----//
int privkey_to_pubkey_and_addr(
	const unsigned char* priv, 
	unsigned char* pubout, 
	size_t* puboutlen, 
	char* addrout, 
	int addroutlen, 
	int compressed
);

//----签名交易----//
int sign_tx(Tx* tx, const unsigned char* priv);
void tx_hash(const Tx* tx, unsigned char out[32]);

//----验证交易签名----//
int verify_tx(const Tx* tx);

#endif 
