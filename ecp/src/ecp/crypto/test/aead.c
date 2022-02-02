#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include <crypto.h>
#include <openssl/curve25519.h>

#define NONCE_LEN   12
#define TAG_LEN     16
#define KEY_LEN     32

static int v_rng(void *buf, size_t bufsize) {
    int fd;

    if((fd = open("/dev/urandom", O_RDONLY)) < 0) return -1;
    size_t nb = read(fd, buf, bufsize);
    close(fd);
    if (nb != bufsize) return -1;
    return 0;
}

int main(int argc, char *argv[]) {
    unsigned char in_msg[1024];
    unsigned char out_msg[1024];
    size_t in_msg_len;
    size_t out_msg_len;
    int rv;
    unsigned char public1[KEY_LEN];
    unsigned char private1[KEY_LEN];
    unsigned char public2[KEY_LEN];
    unsigned char private2[KEY_LEN];
    unsigned char key1[KEY_LEN];
    unsigned char key2[KEY_LEN];
    unsigned char nonce[NONCE_LEN];

    strcpy((char *)in_msg, "PERA JE CAR!");
    in_msg_len = strlen((char *)in_msg) + 1;

    v_rng(nonce, NONCE_LEN);
    X25519_keypair(public1, private1);
    X25519_keypair(public2, private2);

    X25519(key1, private1, public2);
    rv = aead_chacha20_poly1305_seal(key1, TAG_LEN, out_msg, &out_msg_len, 1024, nonce, NONCE_LEN, in_msg, in_msg_len, NULL, 0);
    printf("SEAL RV:%d ILEN:%lu OLEN:%lu\n", rv, in_msg_len, out_msg_len);

    memset(in_msg, 0, sizeof(in_msg));

    X25519(key2, private2, public1);
    rv = aead_chacha20_poly1305_open(key2, TAG_LEN, in_msg, &in_msg_len, 1024, nonce, NONCE_LEN, out_msg, out_msg_len, NULL, 0);
    printf("OPEN RV:%d ILEN:%lu OLEN:%lu\n", rv, out_msg_len, in_msg_len);
    printf("MSG: %s\n", in_msg);
}