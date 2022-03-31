#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "net.h"

int getentropy(unsigned char *b, size_t sz) {
    unsigned char type;
    unsigned char *buffer;
    uint16_t len;
    int rv;

    buffer = eos_net_alloc();

    type = EOS_NET_MTYPE_RNG;
    len = sizeof(uint16_t);
    buffer[0] = sz >> 8;
    buffer[1] = sz;

    rv = eos_net_xchg(&type, buffer, &len);
    if (rv || (len != sz)) rv = -1;

    if (!rv) memcpy(b, buffer, sz);
    eos_net_free(buffer, 1);

    return rv;
}
