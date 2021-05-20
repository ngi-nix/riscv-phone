#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "sha/sha1.h"
#include "sdc_crypto.h"

#define SDC_CRYPTO_KEY_SIZE     16
#define SDC_CRYPTO_BLK_SIZE     16
#define SDC_CRYPTO_HASH_SIZE    20

EOSSDCCrypto *sdc_crypto;

void eos_sdcc_init(EOSSDCCrypto *crypto, uint8_t *key, void *ctx, eve_sdcc_init_t init, eve_sdcc_crypt_t enc, eve_sdcc_crypt_t dec, void *ctx_essiv, eve_sdcc_init_t init_essiv, eve_sdcc_essiv_t enc_essiv) {
    char key_essiv[SDC_CRYPTO_HASH_SIZE];

    sdc_crypto = crypto;

    memset(key_essiv, 0, SDC_CRYPTO_HASH_SIZE);
    init(ctx, key);
    sdc_crypto->ctx = ctx;
    sdc_crypto->enc = enc;
    sdc_crypto->dec = dec;

    SHA1(key_essiv, key, SDC_CRYPTO_KEY_SIZE);
    init_essiv(ctx_essiv, key_essiv);
    sdc_crypto->ctx_essiv = ctx_essiv;
    sdc_crypto->enc_essiv = enc_essiv;
}

void eos_sdcc_encrypt(uint32_t sect, uint8_t *buffer) {
    uint8_t iv[SDC_CRYPTO_BLK_SIZE];

    if (sdc_crypto == NULL) return;

    memset(iv, 0, SDC_CRYPTO_BLK_SIZE);
    memcpy(iv, &sect, sizeof(sect));
    sdc_crypto->enc_essiv(sdc_crypto->ctx_essiv, iv);
    sdc_crypto->enc(sdc_crypto->ctx, iv, buffer, 512);
}

void eos_sdcc_decrypt(uint32_t sect, uint8_t *buffer) {
    uint8_t iv[SDC_CRYPTO_BLK_SIZE];

    if (sdc_crypto == NULL) return;

    memset(iv, 0, SDC_CRYPTO_BLK_SIZE);
    memcpy(iv, &sect, sizeof(sect));
    sdc_crypto->enc_essiv(sdc_crypto->ctx_essiv, iv);
    sdc_crypto->dec(sdc_crypto->ctx, iv, buffer, 512);
}