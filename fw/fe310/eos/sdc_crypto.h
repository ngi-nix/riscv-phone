#include <stddef.h>
#include <stdint.h>

typedef void (*eve_sdcc_init_t) (void *, uint8_t *);
typedef void (*eve_sdcc_crypt_t) (void *, uint8_t *, uint8_t *, size_t);
typedef void (*eve_sdcc_essiv_t) (void *, uint8_t *);

typedef struct EOSSDCCrypto {
    void *ctx;
    eve_sdcc_crypt_t enc;
    eve_sdcc_crypt_t dec;
    void *ctx_essiv;
    eve_sdcc_essiv_t enc_essiv;
} EOSSDCCrypto;

void eos_sdcc_init(EOSSDCCrypto *crypto, uint8_t *key, void *ctx, eve_sdcc_init_t init, eve_sdcc_crypt_t enc, eve_sdcc_crypt_t dec, void *ctx_essiv, eve_sdcc_init_t init_essiv, eve_sdcc_essiv_t enc_essiv);
void eos_sdcc_encrypt(uint32_t sect, uint8_t *buffer);
void eos_sdcc_decrypt(uint32_t sect, uint8_t *buffer);