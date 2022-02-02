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
#define MSG_LEN     29

int main(int argc, char *argv[]) {
    unsigned char in_msg[1024];
    unsigned char out_msg[1024];
    size_t in_msg_len;
    int rv;
    unsigned char public[KEY_LEN];
    unsigned char private[KEY_LEN];
    unsigned char key[KEY_LEN];
    unsigned char nonce[NONCE_LEN];

    int fd = open("msg.enc", O_RDONLY);
    read(fd, private, KEY_LEN);
    read(fd, public, KEY_LEN);
    read(fd, nonce, NONCE_LEN);
    read(fd, out_msg, MSG_LEN);
    close(fd);

    X25519(key, private, public);
    rv = aead_chacha20_poly1305_open(key, TAG_LEN, in_msg, &in_msg_len, 1024, nonce, NONCE_LEN, out_msg, MSG_LEN, NULL, 0);
    printf("OPEN RV:%d ILEN:%d OLEN:%lu\n", rv, MSG_LEN, in_msg_len);
    printf("MSG: %s\n", in_msg);
}