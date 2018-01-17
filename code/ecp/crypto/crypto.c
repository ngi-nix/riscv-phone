#include <core.h>

#include <curve25519.h>

static int dh_mkpair(ecp_dh_public_t *pub, ecp_dh_private_t *priv, ecp_rng_t *rand_buf) {
    int rv = X25519_keypair(*pub, *priv, rand_buf);
    if (!rv) return ECP_ERR;
    return ECP_OK;
}

static int dh_shsec(ecp_aead_key_t *shared, ecp_dh_public_t *pub, ecp_dh_private_t *priv) {
    int rv = X25519(*shared, *priv, *pub);
    if (!rv) return ECP_ERR;
    return ECP_OK;
}

static unsigned char *dh_pub_get_buf(ecp_dh_public_t *p) {
    return (unsigned char *)p;
}

static void dh_pub_to_buf(unsigned char *b, ecp_dh_public_t *p) {
    memcpy(b, p, ECP_ECDH_SIZE_KEY);
}

static void dh_pub_from_buf(ecp_dh_public_t *p, unsigned char *b) {
    memcpy(p, b, ECP_ECDH_SIZE_KEY);
}

static int dh_pub_eq(unsigned char *p1, ecp_dh_public_t *p2) {
    return !memcmp(p1, p2, ECP_ECDH_SIZE_KEY);
}

static unsigned int dh_pub_hash_fn(unsigned char *p) {
    return *((unsigned int *)p);
}

static int dh_pub_hash_eq(unsigned char *p1, unsigned char *p2) {
    return !memcmp(p1, p2, ECP_ECDH_SIZE_KEY);
}

static ssize_t aead_enc(unsigned char *ct, size_t cl, unsigned char *pt, size_t pl, ecp_aead_key_t *k, unsigned char *n) {
    size_t ol;
    int rv = aead_chacha20_poly1305_seal(ct, &ol, cl, *k, ECP_AEAD_SIZE_TAG, n, ECP_AEAD_SIZE_NONCE, pt, pl, NULL, 0);
    if (!rv) return ECP_ERR;
    return ol;
}

static ssize_t aead_dec(unsigned char *pt, size_t pl, unsigned char *ct, size_t cl, ecp_aead_key_t *k, unsigned char *n) {
    size_t ol;
    int rv = aead_chacha20_poly1305_open(pt, &ol, pl, *k, ECP_AEAD_SIZE_TAG, n, ECP_AEAD_SIZE_NONCE, ct, cl, NULL, 0);
    if (!rv) return ECP_ERR;
    return ol;
}

int dsa_mkpair(ecp_dsa_public_t *pub, ecp_dsa_private_t *priv, ecp_rng_t *rand_buf) {
    unsigned char key[2*ECP_DSA_SIZE_KEY];    

    int rv = ED25519_keypair(*pub, key, rand_buf);
    if (!rv) return ECP_ERR;
    memcpy(priv, key, ECP_DSA_SIZE_KEY);
    return ECP_OK;
}

int dsa_sign(unsigned char *sig, unsigned char *m, size_t ml, ecp_dsa_public_t *p, ecp_dsa_private_t *s) {
    unsigned char key[2*ECP_DSA_SIZE_KEY];
    memcpy(key, s, ECP_DSA_SIZE_KEY);
    memcpy(key+ECP_DSA_SIZE_KEY, p, ECP_DSA_SIZE_KEY);

    int rv = ED25519_sign(sig, m, ml, key);
    if (!rv) return ECP_ERR;
    return ECP_OK;
}

int dsa_verify(unsigned char *m, size_t ml, unsigned char *sig, ecp_dsa_public_t *p) {
    return ED25519_verify(m, ml, sig, *p);
}

int ecp_crypto_init(ECPCryptoIface *cr) {
    cr->init = 1;
    cr->dh_mkpair = dh_mkpair;
    cr->dh_shsec = dh_shsec;
    cr->dh_pub_get_buf = dh_pub_get_buf;
    cr->dh_pub_to_buf = dh_pub_to_buf;
    cr->dh_pub_from_buf = dh_pub_from_buf;
    cr->dh_pub_eq = dh_pub_eq;
    cr->dh_pub_hash_fn = dh_pub_hash_fn;
    cr->dh_pub_hash_eq = dh_pub_hash_eq;
    cr->aead_enc = aead_enc;
    cr->aead_dec = aead_dec;
    cr->dsa_mkpair = dsa_mkpair;
    cr->dsa_sign = dsa_sign;
    cr->dsa_verify = dsa_verify;
    return ECP_OK;
}
