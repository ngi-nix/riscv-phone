#include <stdlib.h>
#include <string.h>

#include <esp_log.h>

#include "eos.h"
#include "cell.h"

void eos_cell_pdp_handler(unsigned char mtype, unsigned char *buffer, uint16_t size) {
    char *apn, *user, *pass;

    buffer += 1;
    size -= 1;
    switch (mtype) {
        case EOS_CELL_MTYPE_PDP_CONFIG:
            apn = (char *)buffer;
            user = apn + strlen(apn) + 1;
            pass = user + strlen(user) + 1;
            eos_ppp_set_apn(apn);
            eos_ppp_set_auth(user, pass);
            break;

        case EOS_CELL_MTYPE_PDP_CONNECT:
            eos_ppp_connect();
            break;

        case EOS_CELL_MTYPE_PDP_DISCONNECT:
            eos_ppp_disconnect();
            break;
    }
}
