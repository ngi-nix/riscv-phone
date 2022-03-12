#include <stdlib.h>
#include <string.h>

#include <core.h>
#include <cr.h>

#include <openssl/curve25519.h>

int ecp_ecdh_mkpair(ecp_ecdh_public_t *pub, ecp_ecdh_private_t *priv) {
    X25519_keypair(*pub, *priv);
    return ECP_OK;
}

int ecp_ecdh_shkey(ecp_aead_key_t *shared, ecp_ecdh_public_t *pub, ecp_ecdh_private_t *priv) {
    int rv;

    rv = X25519(*shared, *priv, *pub);
    if (!rv) return ECP_ERR;
    return ECP_OK;
}

unsigned int ecp_ecdh_pub_hash(ecp_ecdh_public_t *p) {
    return *((unsigned int *)p);
}

int ecp_ecdh_pub_eq(ecp_ecdh_public_t *p1, ecp_ecdh_public_t *p2) {
    return !memcmp(p1, p2, sizeof(ecp_ecdh_public_t));
}

ssize_t ecp_aead_enc(unsigned char *ct, size_t cl, unsigned char *pt, size_t pl, ecp_aead_key_t *k, ecp_nonce_t *n, unsigned char nt) {
    ecp_aead_nonce_t nonce;
    size_t ol;
    int rv;

    switch (nt) {
        case ECP_NTYPE_INB:
            memcpy(&nonce, "SRVR", 4);
            break;

        case ECP_NTYPE_OUTB:
            memcpy(&nonce, "CLNT", 4);
            break;

        case ECP_NTYPE_VBOX:
            memcpy(&nonce, "VBOX", 4);
            break;

        default:
            return ECP_ERR_ENCRYPT;
    }
    ecp_nonce2buf((unsigned char *)&nonce + 4, n);

    rv = aead_chacha20_poly1305_seal(*k, ECP_SIZE_AEAD_TAG, ct, &ol, cl, (const unsigned char *)&nonce, sizeof(nonce), pt, pl, NULL, 0);
    if (!rv) return ECP_ERR_ENCRYPT;
    return ol;
}

ssize_t ecp_aead_dec(unsigned char *pt, size_t pl, unsigned char *ct, size_t cl, ecp_aead_key_t *k, ecp_nonce_t *n, unsigned char nt) {
    ecp_aead_nonce_t nonce;
    size_t ol;
    int rv;

    switch (nt) {
        case ECP_NTYPE_INB:
            memcpy(&nonce, "CLNT", 4);
            break;

        case ECP_NTYPE_OUTB:
            memcpy(&nonce, "SRVR", 4);
            break;

        case ECP_NTYPE_VBOX:
            memcpy(&nonce, "VBOX", 4);
            break;

        default:
            return ECP_ERR_DECRYPT;
    }
    ecp_nonce2buf((unsigned char *)&nonce + 4, n);

    rv = aead_chacha20_poly1305_open(*k, ECP_SIZE_AEAD_TAG, pt, &ol, pl, (const unsigned char *)&nonce, sizeof(nonce), ct, cl, NULL, 0);
    if (!rv) return ECP_ERR_DECRYPT;
    return ol;
}

int ecp_ecdsa_mkpair(ecp_ecdsa_public_t *pub, ecp_ecdsa_private_t *priv) {
    ED25519_keypair(*pub, *priv);
    return ECP_OK;
}

int ecp_ecdsa_sign(ecp_ecdsa_signature_t *sig, unsigned char *m, size_t ml, ecp_ecdsa_private_t *k) {
    int rv;

    rv = ED25519_sign(*sig, m, ml, *k);
    if (!rv) return ECP_ERR_SIGN;
    return ECP_OK;
}

int ecp_ecdsa_verify(unsigned char *m, size_t ml, ecp_ecdsa_signature_t *sig, ecp_ecdsa_public_t *p) {
    int rv;

    rv = ED25519_verify(m, ml, *sig, *p);
    if (rv == 1) return ECP_OK;
    return ECP_ERR_VERIFY;
}
