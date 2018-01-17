#define CURVE25519_SIZE_KEY     32
#define CHACHA20_SIZE_KEY       32
#define POLY1305_SIZE_TAG       16
#define CHACHA20_SIZE_NONCE     8

#define ECP_ECDH_SIZE_KEY       32
#define ECP_AEAD_SIZE_KEY       32
#define ECP_AEAD_SIZE_TAG       16
#define ECP_AEAD_SIZE_NONCE     8

#define ECP_DSA_SIZE_KEY        32

typedef uint8_t ecp_dh_public_t[ECP_ECDH_SIZE_KEY];
typedef uint8_t ecp_dh_private_t[ECP_ECDH_SIZE_KEY];
typedef uint8_t ecp_aead_key_t[ECP_AEAD_SIZE_KEY];
typedef uint8_t ecp_dsa_public_t[ECP_DSA_SIZE_KEY];
typedef uint8_t ecp_dsa_private_t[ECP_DSA_SIZE_KEY];

int
aead_chacha20_poly1305_seal(unsigned char *out, size_t *out_len, 
    size_t max_out_len, unsigned char key[32], unsigned char tag_len,
    const unsigned char *nonce, size_t nonce_len,
    const unsigned char *in, size_t in_len,
    const unsigned char *ad, size_t ad_len);

int
aead_chacha20_poly1305_open(unsigned char *out, size_t *out_len, 
    size_t max_out_len, unsigned char key[32], unsigned char tag_len,
    const unsigned char *nonce, size_t nonce_len,
    const unsigned char *in, size_t in_len,
    const unsigned char *ad, size_t ad_len);
