#pragma once
#include <stdint.h>
#include <stddef.h>
typedef struct {uint32_t h[8];uint64_t count;uint8_t block[64];unsigned used;} Sha256;
void sha256_init(Sha256 *s);
void sha256_update(Sha256 *s,const void *data,size_t n);
void sha256_final(Sha256 *s,uint8_t out[32]);
