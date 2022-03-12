uint32_t arc4random(void);
void arc4random_buf(void *_buf, size_t n);
uint32_t arc4random_uniform(uint32_t upper_bound);

int ecp_ecdh_mkpair(ecp_ecdh_public_t *pub, ecp_ecdh_private_t *priv);
int ecp_ecdh_shkey(ecp_aead_key_t *shared, ecp_ecdh_public_t *pub, ecp_ecdh_private_t *priv);

unsigned int ecp_ecdh_pub_hash(ecp_ecdh_public_t *p);
int ecp_ecdh_pub_eq(ecp_ecdh_public_t *p1, ecp_ecdh_public_t *p2);

ssize_t ecp_aead_enc(unsigned char *ct, size_t cl, unsigned char *pt, size_t pl, ecp_aead_key_t *k, ecp_nonce_t *n, unsigned char nt);
ssize_t ecp_aead_dec(unsigned char *pt, size_t pl, unsigned char *ct, size_t cl, ecp_aead_key_t *k, ecp_nonce_t *n, unsigned char nt);

int ecp_ecdsa_mkpair(ecp_ecdsa_public_t *pub, ecp_ecdsa_private_t *priv);
int ecp_ecdsa_sign(ecp_ecdsa_signature_t *sig, unsigned char *m, size_t ml, ecp_ecdsa_private_t *k);
int ecp_ecdsa_verify(unsigned char *m, size_t ml, ecp_ecdsa_signature_t *sig, ecp_ecdsa_public_t *p);
