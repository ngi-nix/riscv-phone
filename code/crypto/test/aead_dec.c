#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include <crypto.h>
#include <curve25519.h>

#define NONCE_LEN   8
#define TAG_LEN     16
#define KEY_LEN     32
#define MSG_LEN     29

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
    rv = aead_chacha20_poly1305_open(in_msg, &in_msg_len, 1024, key, TAG_LEN, nonce, NONCE_LEN, out_msg, MSG_LEN, NULL, 0);
    printf("OPEN RV:%d ILEN:%lu OLEN:%d\n", rv, in_msg_len, MSG_LEN);
    printf("MSG: %s\n", in_msg);
}