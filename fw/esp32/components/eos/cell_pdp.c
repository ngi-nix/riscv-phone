#include <stdlib.h>
#include <string.h>

#include <esp_log.h>

#include "eos.h"
#include "cell.h"

void eos_cell_pdp_handler(unsigned char mtype, unsigned char *buffer, uint16_t buf_len) {
    char *apn, *user, *pass, *_buf;
    uint16_t _buf_len;

    switch (mtype) {
        case EOS_CELL_MTYPE_PDP_CONFIG:
            _buf = (char *)buffer;
            _buf_len = 0;

            apn = _buf;
            _buf_len = strnlen(_buf, buf_len);
            if (_buf_len == buf_len) break;
            _buf += _buf_len + 1;
            buf_len -= _buf_len + 1;

            user = _buf;
            _buf_len = strnlen(_buf, buf_len);
            if (_buf_len == buf_len) break;
            _buf += _buf_len + 1;
            buf_len -= _buf_len + 1;

            pass = _buf;
            _buf_len = strnlen(_buf, buf_len);
            if (_buf_len == buf_len) break;
            _buf += _buf_len + 1;
            buf_len -= _buf_len + 1;

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
