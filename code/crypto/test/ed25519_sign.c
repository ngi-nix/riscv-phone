#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include <curve25519.h>

#define KEY_LEN     32
#define SIG_LEN     64

static int v_rng(void *buf, size_t bufsize) {
    int fd;
    
    if((fd = open("/dev/urandom", O_RDONLY)) < 0) return -1;
    size_t nb = read(fd, buf, bufsize);
    close(fd);
    if (nb != bufsize) return -1;
    return 0;
}

int main(int argc, char *argv[]) {
    unsigned char msg[1024];
    size_t msg_len;
    unsigned char public[KEY_LEN];
    unsigned char private[KEY_LEN * 2];
    unsigned char signature[SIG_LEN];

    strcpy((char *)msg, "PERA JE CAR!");
    msg_len = strlen((char *)msg) + 1;
    
    ED25519_keypair(public, private, v_rng);
    ED25519_sign(signature, msg, msg_len, private);

    unlink("msg.sig");
    int fd = open("msg.sig", O_WRONLY | O_CREAT);
    write(fd, public, KEY_LEN);
    write(fd, signature, SIG_LEN);
    close(fd);
}