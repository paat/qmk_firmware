// Standard SHA-256, HMAC-SHA256 and PBKDF2-HMAC-SHA256 for the password layer.
// Output matches any standard implementation (e.g. Python hashlib / hmac).
#pragma once

#include <stdint.h>
#include <stddef.h>

#define PW_HASH_LEN 32

// out = HMAC-SHA256(key, msg)
void pw_hmac_sha256(const uint8_t *key, size_t key_len, const uint8_t *msg, size_t msg_len, uint8_t out[PW_HASH_LEN]);

// out = PBKDF2-HMAC-SHA256(password, salt, iterations, dkLen = 32)
void pw_pbkdf2_sha256(const uint8_t *password, size_t password_len, const uint8_t *salt, size_t salt_len, uint32_t iterations, uint8_t out[PW_HASH_LEN]);

// Zero memory in a way the compiler will not optimise away.
void pw_wipe(void *p, size_t n);
