#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <esp_log.h>

#include "eos.h"
#include "net.h"
#include "at_cmd.h"
#include "cell.h"

static const char *TAG = "EOS USSD";

static char cmd[256];
static int cmd_len;

void eos_cell_ussd_handler(unsigned char mtype, unsigned char *buffer, uint16_t buf_len) {
    int rv;

    switch (mtype) {
        case EOS_CELL_MTYPE_USSD_REQUEST:
            if (buf_len > EOS_CELL_MAX_USSD_STR) return;

            buffer[buf_len] = '\0';
            cmd_len = snprintf(cmd, sizeof(cmd), "AT+CUSD=1,\"%s\",15\r", buffer);
            if ((cmd_len < 0) || (cmd_len >= sizeof(cmd))) return;

            rv = eos_modem_take(1000);
            if (rv) return;

            at_cmd(cmd);
            rv = at_expect("^OK", "^ERROR", 1000);

            eos_modem_give();
            break;

        case EOS_CELL_MTYPE_USSD_CANCEL:
            rv = eos_modem_take(1000);
            if (rv) return;

            at_cmd("AT+CUSD=2\r");
            rv = at_expect("^OK", "^ERROR", 1000);

            eos_modem_give();
            break;
    }

}

static void ussd_reply_handler(char *urc, regmatch_t m[]) {
    int rep, rv;
    unsigned char *buf;
    uint16_t len;
    char *_buf;
    size_t _len;
    regex_t re;
    regmatch_t match[2];

    rv = regcomp(&re, ".*(\",[0-9]+)$", REG_EXTENDED);
    if (rv) return;

    sscanf(urc + m[1].rm_so, "%1d", &rep);

    buf = eos_net_alloc();
    buf[0] = EOS_CELL_MTYPE_USSD | EOS_CELL_MTYPE_USSD_REPLY;
    buf[1] = rep;
    len = 2;

    rv = EOS_OK;
    _buf = (char *)buf + len;
    strcpy(_buf, urc + m[2].rm_so);
    do {
        if (regexec(&re, _buf, 2, match, 0) == 0) {
            ESP_LOGI(TAG, "MATCH:%ld %s", match[1].rm_so, _buf);
            _buf[match[1].rm_so] = '\0';
            _len = strlen(_buf);
            len += _len + 1;
            break;
        } else {
            _len = strlen(_buf);
            _buf[_len] = '\n';
            _buf += _len + 1;
            len += _len + 1;
        }
        rv = eos_modem_readln(_buf, EOS_NET_MTU - len, 1000);
        if (rv) break;
    } while (1);

    if (rv) {
        ESP_LOGE(TAG, "error");
        eos_net_free(buf);
    } else {
        eos_net_send(EOS_NET_MTYPE_CELL, buf, len);
    }
    regfree(&re);
}

void eos_cell_ussd_init(void) {
    at_urc_insert("\\+CUSD: ([0-9]),\"(.*)", ussd_reply_handler, REG_EXTENDED);

}