#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include <openssl/curve25519.h>

#define KEY_LEN     32
#define SIG_LEN     64

int main(int argc, char *argv[]) {
    unsigned char msg[1024];
    size_t msg_len;
    int rv;
    unsigned char public[KEY_LEN];
    unsigned char private[KEY_LEN * 2];
    unsigned char signature[SIG_LEN];

    strcpy((char *)msg, "PERA JE CAR!");
    msg_len = strlen((char *)msg) + 1;

    int fd = open("msg.sig", O_RDONLY);
    read(fd, public, KEY_LEN);
    read(fd, signature, SIG_LEN);
    close(fd);

    rv = ED25519_verify(msg, msg_len, signature, public);
    printf("OPEN rv:%d\n", rv);
}