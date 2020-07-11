#include <string.h>

#include "eve.h"
#include "clipb.h"

static char _clipb[EVE_CLIPB_SIZE_BUF];

int eve_clipb_push(char *str, uint16_t len) {
    if (len >= EVE_CLIPB_SIZE_BUF) return EVE_ERR;

    memcpy(_clipb, str, len);
    _clipb[len] = '\0';

    return EVE_OK;
}

char *eve_clipb_get(void) {
    return _clipb;
}