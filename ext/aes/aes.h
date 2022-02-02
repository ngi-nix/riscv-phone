#ifndef _AES_H_
#define _AES_H_

#include <stdint.h>
#include <stddef.h>

#define AES128 1
//#define AES192 1
//#define AES256 1

#define AES_BLOCKLEN 16 // Block length in bytes - AES is 128b block only

#if defined(AES256) && (AES256 == 1)
    #define AES_KEYLEN 32
    #define AES_KEYEXPSIZE 240
#elif defined(AES192) && (AES192 == 1)
    #define AES_KEYLEN 24
    #define AES_KEYEXPSIZE 208
#else
    #define AES_KEYLEN 16   // Key length in bytes
    #define AES_KEYEXPSIZE 176
#endif

typedef struct
{
  uint8_t RoundKey[AES_KEYEXPSIZE];
} AESCtx;

void aes_init(AESCtx *ctx, uint8_t *key);
// buffer size is exactly AES_BLOCKLEN bytes;
void aes_ecb_encrypt(AESCtx *ctx, uint8_t *buf);
void aes_ecb_decrypt(AESCtx *ctx, uint8_t *buf);
// buffer size MUST be mutile of AES_BLOCKLEN;
void aes_cbc_encrypt(AESCtx *ctx, uint8_t *iv, uint8_t *buf, size_t length);
void aes_cbc_decrypt(AESCtx *ctx, uint8_t *iv, uint8_t *buf, size_t length);

#endif