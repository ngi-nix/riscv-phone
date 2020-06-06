#include <core.h>
#include <cr.h>

#include <curve25519.h>

int ecp_cr_dh_mkpair(ecp_dh_public_t *pub, ecp_dh_private_t *priv, ecp_rng_t rand_buf) {
    int rv = X25519_keypair(*pub, *priv, rand_buf);
    if (!rv) return ECP_ERR;
    return ECP_OK;
}

int ecp_cr_dh_shsec(ecp_aead_key_t *shared, ecp_dh_public_t *pub, ecp_dh_private_t *priv) {
    int rv = X25519(*shared, *priv, *pub);
    if (!rv) return ECP_ERR;
    return ECP_OK;
}

unsigned char *ecp_cr_dh_pub_get_buf(ecp_dh_public_t *p) {
    return (unsigned char *)p;
}

void ecp_cr_dh_pub_to_buf(unsigned char *b, ecp_dh_public_t *p) {
    memcpy(b, p, ECP_ECDH_SIZE_KEY);
}

void ecp_cr_dh_pub_from_buf(ecp_dh_public_t *p, unsigned char *b) {
    memcpy(p, b, ECP_ECDH_SIZE_KEY);
}

int ecp_cr_dh_pub_eq(unsigned char *p1, ecp_dh_public_t *p2) {
    return !memcmp(p1, p2, ECP_ECDH_SIZE_KEY);
}

unsigned int ecp_cr_dh_pub_hash_fn(unsigned char *p) {
    return *((unsigned int *)p);
}

int ecp_cr_dh_pub_hash_eq(unsigned char *p1, unsigned char *p2) {
    return !memcmp(p1, p2, ECP_ECDH_SIZE_KEY);
}

ssize_t ecp_cr_aead_enc(unsigned char *ct, size_t cl, unsigned char *pt, size_t pl, ecp_aead_key_t *k, unsigned char *n) {
    size_t ol;
    int rv = aead_chacha20_poly1305_seal(ct, &ol, cl, *k, ECP_AEAD_SIZE_TAG, n, ECP_AEAD_SIZE_NONCE, pt, pl, NULL, 0);
    if (!rv) return ECP_ERR;
    return ol;
}

ssize_t ecp_cr_aead_dec(unsigned char *pt, size_t pl, unsigned char *ct, size_t cl, ecp_aead_key_t *k, unsigned char *n) {
    size_t ol;
    int rv = aead_chacha20_poly1305_open(pt, &ol, pl, *k, ECP_AEAD_SIZE_TAG, n, ECP_AEAD_SIZE_NONCE, ct, cl, NULL, 0);
    if (!rv) return ECP_ERR;
    return ol;
}

int ecp_cr_dsa_mkpair(ecp_dsa_public_t *pub, ecp_dsa_private_t *priv, ecp_rng_t rand_buf) {
    unsigned char key[2*ECP_DSA_SIZE_KEY];    

    int rv = ED25519_keypair(*pub, key, rand_buf);
    if (!rv) return ECP_ERR;
    memcpy(priv, key, ECP_DSA_SIZE_KEY);
    return ECP_OK;
}

int ecp_cr_dsa_sign(unsigned char *sig, unsigned char *m, size_t ml, ecp_dsa_public_t *p, ecp_dsa_private_t *s) {
    unsigned char key[2*ECP_DSA_SIZE_KEY];
    memcpy(key, s, ECP_DSA_SIZE_KEY);
    memcpy(key+ECP_DSA_SIZE_KEY, p, ECP_DSA_SIZE_KEY);

    int rv = ED25519_sign(sig, m, ml, key);
    if (!rv) return ECP_ERR;
    return ECP_OK;
}

int ecp_cr_dsa_verify(unsigned char *m, size_t ml, unsigned char *sig, ecp_dsa_public_t *p) {
    return ED25519_verify(m, ml, sig, *p);
}
