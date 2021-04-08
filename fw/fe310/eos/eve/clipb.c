#include <string.h>

#include "eve.h"
#include "clipb.h"

static uint8_t clipb[EVE_CLIPB_SIZE_BUF];

int eve_clipb_push(uint8_t *str, uint16_t len) {
    if (len >= EVE_CLIPB_SIZE_BUF) return EVE_ERR;

    memcpy(clipb, str, len);
    clipb[len] = '\0';

    return EVE_OK;
}

uint8_t *eve_clipb_get(void) {
    return clipb;
}