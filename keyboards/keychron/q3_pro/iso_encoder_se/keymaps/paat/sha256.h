#ifndef _SHA256_H
#define _SHA256_H

#include <stdint.h>
#include <stddef.h>

void sha256_init(uint32_t state[8]);
void sha256_update(uint32_t state[8], const uint8_t data[], size_t len);
void sha256_final(uint8_t hash[], uint32_t state[8]);

#endif
