// Standard SHA-256, HMAC-SHA256 and PBKDF2-HMAC-SHA256 for the password layer.
//
// Working state is kept in file-scope statics to keep stack use small on the
// MCU; nothing here is reentrant. All working state is wiped before returning.

#include "pwkdf.h"
#include <string.h>

typedef struct {
    uint32_t state[8];
    uint32_t total_len; // bytes hashed so far (inputs here are tiny)
    uint8_t  buf[64];
    uint8_t  buf_len;
} pw_sha256_ctx;

typedef struct {
    pw_sha256_ctx inner; // state after absorbing key ^ ipad
    pw_sha256_ctx outer; // state after absorbing key ^ opad
} pw_hmac_key;

static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

void pw_wipe(void *p, size_t n) {
    volatile uint8_t *v = (volatile uint8_t *)p;
    while (n--) {
        *v++ = 0;
    }
}

static void sha256_compress(uint32_t state[8], const uint8_t block[64]) {
    static uint32_t w[16]; // rolling message schedule
    uint32_t        s[8];

    for (uint8_t i = 0; i < 16; i++) {
        w[i] = ((uint32_t)block[i * 4] << 24) | ((uint32_t)block[i * 4 + 1] << 16) | ((uint32_t)block[i * 4 + 2] << 8) | (uint32_t)block[i * 4 + 3];
    }
    memcpy(s, state, sizeof(s));

    for (uint8_t i = 0; i < 64; i++) {
        if (i >= 16) {
            uint32_t w15 = w[(i + 1) & 15];
            uint32_t w2  = w[(i + 14) & 15];
            uint32_t s0  = ROTR(w15, 7) ^ ROTR(w15, 18) ^ (w15 >> 3);
            uint32_t s1  = ROTR(w2, 17) ^ ROTR(w2, 19) ^ (w2 >> 10);
            w[i & 15] += s0 + w[(i + 9) & 15] + s1;
        }
        uint32_t S1 = ROTR(s[4], 6) ^ ROTR(s[4], 11) ^ ROTR(s[4], 25);
        uint32_t ch = (s[4] & s[5]) ^ (~s[4] & s[6]);
        uint32_t t1 = s[7] + S1 + ch + K[i] + w[i & 15];
        uint32_t S0 = ROTR(s[0], 2) ^ ROTR(s[0], 13) ^ ROTR(s[0], 22);
        uint32_t mj = (s[0] & s[1]) ^ (s[0] & s[2]) ^ (s[1] & s[2]);
        uint32_t t2 = S0 + mj;
        s[7]        = s[6];
        s[6]        = s[5];
        s[5]        = s[4];
        s[4]        = s[3] + t1;
        s[3]        = s[2];
        s[2]        = s[1];
        s[1]        = s[0];
        s[0]        = t1 + t2;
    }

    for (uint8_t i = 0; i < 8; i++) {
        state[i] += s[i];
    }
    pw_wipe(s, sizeof(s));
    pw_wipe(w, sizeof(w));
}

static void sha256_init(pw_sha256_ctx *ctx) {
    static const uint32_t iv[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    memcpy(ctx->state, iv, sizeof(iv));
    ctx->total_len = 0;
    ctx->buf_len   = 0;
}

static void sha256_update(pw_sha256_ctx *ctx, const uint8_t *data, size_t len) {
    ctx->total_len += len;
    while (len > 0) {
        size_t take = 64 - ctx->buf_len;
        if (take > len) {
            take = len;
        }
        memcpy(ctx->buf + ctx->buf_len, data, take);
        ctx->buf_len += take;
        data += take;
        len -= take;
        if (ctx->buf_len == 64) {
            sha256_compress(ctx->state, ctx->buf);
            ctx->buf_len = 0;
        }
    }
}

static void sha256_final(pw_sha256_ctx *ctx, uint8_t out[PW_HASH_LEN]) {
    uint64_t bit_len = (uint64_t)ctx->total_len * 8;

    ctx->buf[ctx->buf_len++] = 0x80;
    if (ctx->buf_len > 56) {
        memset(ctx->buf + ctx->buf_len, 0, 64 - ctx->buf_len);
        sha256_compress(ctx->state, ctx->buf);
        ctx->buf_len = 0;
    }
    memset(ctx->buf + ctx->buf_len, 0, 56 - ctx->buf_len);
    for (uint8_t i = 0; i < 8; i++) {
        ctx->buf[56 + i] = (uint8_t)(bit_len >> (56 - 8 * i));
    }
    sha256_compress(ctx->state, ctx->buf);

    for (uint8_t i = 0; i < 8; i++) {
        out[i * 4]     = (uint8_t)(ctx->state[i] >> 24);
        out[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        out[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        out[i * 4 + 3] = (uint8_t)(ctx->state[i]);
    }
    pw_wipe(ctx, sizeof(*ctx));
}

static void hmac_key_init(pw_hmac_key *hk, const uint8_t *key, size_t key_len) {
    static uint8_t block[64];

    memset(block, 0, sizeof(block));
    if (key_len > 64) {
        sha256_init(&hk->inner);
        sha256_update(&hk->inner, key, key_len);
        sha256_final(&hk->inner, block);
    } else {
        memcpy(block, key, key_len);
    }

    for (uint8_t i = 0; i < 64; i++) {
        block[i] ^= 0x36;
    }
    sha256_init(&hk->inner);
    sha256_update(&hk->inner, block, 64);

    for (uint8_t i = 0; i < 64; i++) {
        block[i] ^= 0x36 ^ 0x5c;
    }
    sha256_init(&hk->outer);
    sha256_update(&hk->outer, block, 64);

    pw_wipe(block, sizeof(block));
}

// out = HMAC(key, msg1 || msg2) using a prepared key; out may alias msg1.
static void hmac_compute(const pw_hmac_key *hk, const uint8_t *msg1, size_t len1, const uint8_t *msg2, size_t len2, uint8_t out[PW_HASH_LEN]) {
    static pw_sha256_ctx ctx;
    uint8_t              inner_hash[PW_HASH_LEN];

    ctx = hk->inner;
    sha256_update(&ctx, msg1, len1);
    if (len2 > 0) {
        sha256_update(&ctx, msg2, len2);
    }
    sha256_final(&ctx, inner_hash);

    ctx = hk->outer;
    sha256_update(&ctx, inner_hash, PW_HASH_LEN);
    sha256_final(&ctx, out);

    pw_wipe(inner_hash, sizeof(inner_hash));
}

static pw_hmac_key hmac_key;

void pw_hmac_sha256(const uint8_t *key, size_t key_len, const uint8_t *msg, size_t msg_len, uint8_t out[PW_HASH_LEN]) {
    hmac_key_init(&hmac_key, key, key_len);
    hmac_compute(&hmac_key, msg, msg_len, NULL, 0, out);
    pw_wipe(&hmac_key, sizeof(hmac_key));
}

void pw_pbkdf2_sha256(const uint8_t *password, size_t password_len, const uint8_t *salt, size_t salt_len, uint32_t iterations, uint8_t out[PW_HASH_LEN]) {
    static const uint8_t block_index[4] = {0, 0, 0, 1}; // only the first 32-byte block is needed
    uint8_t              u[PW_HASH_LEN];

    hmac_key_init(&hmac_key, password, password_len);

    hmac_compute(&hmac_key, salt, salt_len, block_index, sizeof(block_index), u);
    memcpy(out, u, PW_HASH_LEN);
    for (uint32_t i = 1; i < iterations; i++) {
        hmac_compute(&hmac_key, u, PW_HASH_LEN, NULL, 0, u);
        for (uint8_t j = 0; j < PW_HASH_LEN; j++) {
            out[j] ^= u[j];
        }
    }

    pw_wipe(u, sizeof(u));
    pw_wipe(&hmac_key, sizeof(hmac_key));
}
