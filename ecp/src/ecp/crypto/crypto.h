#define CURVE25519_SIZE_KEY     32
#define CHACHA20_SIZE_KEY       32
#define POLY1305_SIZE_TAG       16
#define CHACHA20_SIZE_NONCE     12

#define ECP_SIZE_ECDH_PUB       32
#define ECP_SIZE_ECDH_SEC       32
#define ECP_SIZE_AEAD_KEY       32
#define ECP_SIZE_AEAD_TAG       16
#define ECP_SIZE_AEAD_NONCE     12

#define ECP_SIZE_ECDSA_PUB      32
#define ECP_SIZE_ECDSA_SEC      64
#define ECP_SIZE_ECDSA_SIG      32

typedef uint8_t ecp_ecdh_public_t[ECP_SIZE_ECDH_PUB];
typedef uint8_t ecp_ecdh_private_t[ECP_SIZE_ECDH_SEC];
typedef uint8_t ecp_aead_key_t[ECP_SIZE_AEAD_KEY];
typedef uint8_t ecp_aead_nonce_t[ECP_SIZE_AEAD_NONCE];
typedef uint8_t ecp_ecdsa_public_t[ECP_SIZE_ECDSA_PUB];
typedef uint8_t ecp_ecdsa_private_t[ECP_SIZE_ECDSA_SEC];
typedef uint8_t ecp_ecdsa_signature_t[ECP_SIZE_ECDSA_SIG];

int
aead_chacha20_poly1305_seal(unsigned char key[32], unsigned char tag_len,
    unsigned char *out, size_t *out_len, size_t max_out_len,
    const unsigned char *nonce, size_t nonce_len,
    const unsigned char *in, size_t in_len,
    const unsigned char *ad, size_t ad_len);

int
aead_chacha20_poly1305_open(unsigned char key[32], unsigned char tag_len,
    unsigned char *out, size_t *out_len, size_t max_out_len,
    const unsigned char *nonce, size_t nonce_len,
    const unsigned char *in, size_t in_len,
    const unsigned char *ad, size_t ad_len);

int
aead_xchacha20_poly1305_seal(unsigned char key[32], unsigned char tag_len,
    unsigned char *out, size_t *out_len, size_t max_out_len,
    const unsigned char *nonce, size_t nonce_len,
    const unsigned char *in, size_t in_len,
    const unsigned char *ad, size_t ad_len);

int
aead_xchacha20_poly1305_open(unsigned char key[32], unsigned char tag_len,
    unsigned char *out, size_t *out_len, size_t max_out_len,
    const unsigned char *nonce, size_t nonce_len,
    const unsigned char *in, size_t in_len,
    const unsigned char *ad, size_t ad_len);
