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
    unsigned char key[KEY_LEN];
    unsigned char nonce[NONCE_LEN];

    strcpy((char *)in_msg, "PERA JE CAR!");
    in_msg_len = strlen((char *)in_msg) + 1;

    v_rng(nonce, NONCE_LEN);
    X25519_keypair(public1, private1, v_rng);
    X25519_keypair(public2, private2, v_rng);
    
    X25519(key, private1, public2);
    rv = aead_chacha20_poly1305_seal(out_msg, &out_msg_len, 1024, key, TAG_LEN, nonce, NONCE_LEN, in_msg, in_msg_len, NULL, 0);
    printf("SEAL RV:%d ILEN:%lu OLEN:%lu\n", rv, in_msg_len, out_msg_len);
    
    unlink("msg.enc");
    int fd = open("msg.enc", O_WRONLY | O_CREAT);
    write(fd, private2, KEY_LEN);
    write(fd, public1, KEY_LEN);
    write(fd, nonce, NONCE_LEN);
    write(fd, out_msg, out_msg_len);
    close(fd);
}