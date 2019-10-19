#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include <ecp/core.h>
#include <ecp/vconn/vconn.h>

#include "encoding.h"
#include "platform.h"

#include "ecp.h"

int ecp_init(ECPContext *ctx) {
    int rv;

    rv = ecp_ctx_create_vconn(ctx);
    if (rv) return rv;

    return ECP_OK;
}
