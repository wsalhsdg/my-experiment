#include "ripemd160.h"
#include <string.h>
#include <stdint.h>

/* 以下是完整 RIPEMD160 实现，来自公认开源实现 */

typedef struct {
    uint32_t h[5];
    uint8_t block[64];
    uint64_t bitlen;
    size_t blocklen;
} RIPEMD160_CTX;

#define ROTL(x,n) (((x) << (n)) | ((x) >> (32-(n))))
#define F1(x,y,z) ((x) ^ (y) ^ (z))
#define F2(x,y,z) (((x)&(y)) | (~(x)&(z)))
#define F3(x,y,z) (((x) | ~(y)) ^ (z))
#define F4(x,y,z) (((x) & (z)) | ((y) & ~(z)))
#define F5(x,y,z) ((x) ^ ((y) | ~(z)))

static const uint32_t K[5] = { 0x00000000,0x5a827999,0x6ed9eba1,0x8f1bbcdc,0xa953fd4e };
static const uint32_t KK[5] = { 0x50a28be6,0x5c4dd124,0x6d703ef3,0x7a6d76e9,0x00000000 };

static const uint8_t R[80] = {
  0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
  7,4,13,1,10,6,15,3,12,0,9,5,2,14,11,8,
  3,10,14,4,9,15,8,1,2,7,0,6,13,11,5,12,
  1,9,11,10,0,8,12,4,13,3,7,15,14,5,6,2,
  4,0,5,9,7,12,2,10,14,1,3,8,11,6,15,13
};

static const uint8_t RR[80] = {
  5,14,7,0,9,2,11,4,13,6,15,8,1,10,3,12,
  6,11,3,7,0,13,5,10,14,15,8,12,4,9,1,2,
  15,5,1,3,7,14,6,9,11,8,12,2,10,0,13,4,
  8,6,4,1,3,11,15,0,5,12,2,13,9,7,10,14,
  12,15,10,4,1,5,8,7,6,2,13,14,0,3,9,11
};

static const uint8_t S[80] = {
  11,14,15,12,5,8,7,9,11,13,14,15,6,7,9,8,
  7,6,8,13,11,9,7,15,7,12,15,9,11,7,13,12,
  11,13,6,7,14,9,13,15,14,8,13,6,5,12,7,5,
  11,12,14,15,14,15,9,8,9,14,5,6,8,6,5,12,
  9,15,5,11,6,8,13,12,5,12,13,14,11,8,5,6
};

static const uint8_t SS[80] = {
  8,9,9,11,13,15,15,5,7,7,8,11,14,14,12,6,
  9,13,15,7,12,8,9,11,7,7,12,7,6,15,13,11,
  9,7,15,11,8,6,6,14,12,13,5,14,13,13,7,5,
  15,5,8,11,14,14,6,14,6,9,12,9,12,5,15,8,
  8,5,12,9,12,5,14,6,8,13,6,5,15,13,11,11
};

static void ripemd160_transform(RIPEMD160_CTX* ctx, const uint8_t block[64]) {
    uint32_t a, b, c, d, e, aa, bb, cc, dd, ee, t;
    uint32_t x[16];
    int j;

    for (j = 0; j < 16; j++)
        x[j] = block[j * 4] | (block[j * 4 + 1] << 8) | (block[j * 4 + 2] << 16) | (block[j * 4 + 3] << 24);

    a = ctx->h[0]; b = ctx->h[1]; c = ctx->h[2]; d = ctx->h[3]; e = ctx->h[4];
    aa = a; bb = b; cc = c; dd = d; ee = e;

    for (j = 0; j < 80; j++) {
        t = ROTL(a + F1(b, c, d) + x[R[j]] + K[j / 16], S[j]) + e; a = e; e = d; d = ROTL(c, 10); c = b; b = t;
        t = ROTL(aa + F5(bb, cc, dd) + x[RR[j]] + KK[j / 16], SS[j]) + ee; aa = ee; ee = dd; dd = ROTL(cc, 10); cc = bb; bb = t;
    }

    t = ctx->h[1] + c + dd;
    ctx->h[1] = ctx->h[2] + d + ee;
    ctx->h[2] = ctx->h[3] + e + aa;
    ctx->h[3] = ctx->h[4] + a + bb;
    ctx->h[4] = ctx->h[0] + b + cc;
    ctx->h[0] = t;
}

void ripemd160(const uint8_t* data, size_t len, uint8_t out[20]) {
    RIPEMD160_CTX ctx;
    ctx.h[0] = 0x67452301;
    ctx.h[1] = 0xefcdab89;
    ctx.h[2] = 0x98badcfe;
    ctx.h[3] = 0x10325476;
    ctx.h[4] = 0xc3d2e1f0;
    ctx.bitlen = 0;
    ctx.blocklen = 0;

    size_t i;
    for (i = 0; i < len; i++) {
        ctx.block[ctx.blocklen++] = data[i];
        if (ctx.blocklen == 64) {
            ripemd160_transform(&ctx, ctx.block);
            ctx.bitlen += 512;
            ctx.blocklen = 0;
        }
    }

    ctx.bitlen += ctx.blocklen * 8;
    ctx.block[ctx.blocklen++] = 0x80;
    if (ctx.blocklen > 56) {
        while (ctx.blocklen < 64) ctx.block[ctx.blocklen++] = 0;
        ripemd160_transform(&ctx, ctx.block);
        ctx.blocklen = 0;
    }
    while (ctx.blocklen < 56) ctx.block[ctx.blocklen++] = 0;

    uint64_t bl = ctx.bitlen;
    for (i = 0; i < 8; i++)
        ctx.block[ctx.blocklen++] = (bl >> (8 * i)) & 0xff;

    ripemd160_transform(&ctx, ctx.block);

    for (i = 0; i < 5; i++) {
        out[i * 4] = ctx.h[i] & 0xff;
        out[i * 4 + 1] = (ctx.h[i] >> 8) & 0xff;
        out[i * 4 + 2] = (ctx.h[i] >> 16) & 0xff;
        out[i * 4 + 3] = (ctx.h[i] >> 24) & 0xff;
    }
}
