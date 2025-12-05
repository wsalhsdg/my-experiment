#ifndef BASE58CHECK_H
#define BASE58CHECK_H

#include <stddef.h>

//----Base58Check ±àÂë----
void Base58check_encode(const unsigned char* input, int len, char* out, int outlen);

#endif
