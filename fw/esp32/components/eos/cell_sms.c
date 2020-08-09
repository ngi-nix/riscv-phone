#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <esp_log.h>

#include "eos.h"
#include "net.h"
#include "unicode.h"
#include "gsm.h"
#include "at_cmd.h"
#include "cell.h"

#define CTRL_Z      0x1a

static const char *TAG = "EOS SMS";

static char cmd[256];

static char pdu[4096];
static int pdu_len;

static uint8_t udh[GSM_UDH_SIZE];
static int udh_len;

static uint8_t msg[GSM_MSG_SIZE];
static int msg_len;
static uint8_t msg_enc;

static char orig_addr[GSM_ADDR_SIZE];
static int orig_addr_len;
static uint8_t orig_addr_type;

static char smsc_addr[GSM_ADDR_SIZE];
static int smsc_addr_len;
static uint8_t smsc_addr_type;

static char smsc_timestamp[GSM_TS_SIZE];

uint16_t flags;

static int sms_decode(unsigned char *buf, uint16_t *_len) {
    int i, j, rv;
    uint16_t len = 0;
    uint8_t smsc_info, smsc_info_len;

    if (pdu_len < 2) return GSM_ERR_SIZE;
    smsc_info = pdu_getc(pdu);
    smsc_info_len = 2 * (smsc_info + 1);
    if (pdu_len < smsc_info_len) return GSM_ERR_SIZE;

    if (smsc_info > 1) {
        pdu_putc((smsc_info - 1) * 2, pdu);
        rv = gsm_addr_dec(pdu, pdu_len, smsc_addr, &smsc_addr_len, &smsc_addr_type);
        if (rv < 0) smsc_addr_len = 0;
    }

    rv = gsm_sms_dec(pdu + smsc_info_len, pdu_len - smsc_info_len, orig_addr, &orig_addr_len, &orig_addr_type, udh, &udh_len, msg, &msg_len, smsc_timestamp, &msg_enc, &flags);
    if (rv == GSM_ERR_NOT_SUPPORTED) ESP_LOGE(TAG, "Message not supported: %s", pdu);
    if (rv < 0) return rv;
    if (msg_enc == GSM_ENC_8BIT) return EOS_ERR;

    buf[0] = flags >> 8;
    buf[1] = flags;
    len += 2;

    memcpy(buf + len, smsc_timestamp, GSM_TS_SIZE);
    len += GSM_TS_SIZE;

    if ((orig_addr_type & GSM_TON) == GSM_TON_ALPHANUMERIC) {
        buf[len] = EOS_CELL_SMS_ADDRTYPE_ALPHA;
        buf[len + 1] = 0;
        len += 2;

        i = 0;
        j = 0;
        while (i < orig_addr_len) {
            uint16_t ch;

            rv = gsm_7bit_to_ucs2((char *)orig_addr + i, orig_addr_len - i, &ch);
            if (rv < 0) return EOS_ERR;
            i += rv;
            rv = utf8_enc(ch, buf + len + j);
            if (rv < 0) return EOS_ERR;
            j += rv;
        }
        buf[len - 1] = j;
        len += j;
    } else {
        buf[len] = ((orig_addr_type & GSM_TON) == GSM_TON_INTERNATIONAL) ? EOS_CELL_SMS_ADDRTYPE_INTL : EOS_CELL_SMS_ADDRTYPE_OTHER;
        buf[len + 1] = orig_addr_len;
        len += 2;
        memcpy(buf + len, orig_addr, orig_addr_len);
        len += orig_addr_len;
    }

    i = 0;
    j = 0;
    while (i < msg_len) {
        utf32_t ch;

        if (msg_enc == GSM_ENC_7BIT) {
            uint16_t _ch;

            rv = gsm_7bit_to_ucs2((char *)msg + i, msg_len - i, &_ch);
            ch = _ch;
        } else {
            if (((msg_len - i) < 4) && (utf16_len(msg + i) > 2)) {
                rv = EOS_ERR;
            } else {
                rv = utf16_dec(msg + i, &ch);
            }
        }
        if (rv < 0) return EOS_ERR;
        i += rv;

        rv = utf8_enc(ch, buf + len + j);
        if (rv < 0) return EOS_ERR;
        j += rv;
    }
    buf[len + j] = '\0';
    len += j + 1;
    *_len = len;

    return EOS_OK;
}

static int sms_encode(unsigned char *buffer, uint16_t size) {
    utf32_t ch;
    int i, rv;
    char *addr;
    uint8_t addr_type;
    int addr_len;

    if (size == 0) return EOS_ERR;

    flags = buffer[0] << 8;
    flags |= buffer[1];
    buffer += 2;
    size -= 2;

    if (size < 2) return EOS_ERR;
    switch(buffer[0]) {
        case EOS_CELL_SMS_ADDRTYPE_INTL:
            addr_type = GSM_EXT | GSM_TON_INTERNATIONAL | GSM_NPI_TELEPHONE;
            break;

        case EOS_CELL_SMS_ADDRTYPE_OTHER:
            addr_type = GSM_EXT | GSM_TON_UNKNOWN | GSM_NPI_TELEPHONE;
            break;

        default: return EOS_ERR;
    }
    addr_len = buffer[1];
    addr = (char *)buffer + 2;

    if (size < 2 + addr_len) return EOS_ERR;
    buffer += 2 + addr_len;
    size -= 2 + addr_len;

    i = 0;
    msg_len = 0;
    while (i < size) {
        rv = utf8_dec(buffer + i, &ch);
        if (rv < 0) return EOS_ERR;
        if (ch >= 0xffff) return EOS_ERR;
        i += rv;

        rv = gsm_ucs2_to_7bit(ch, (char *)msg + msg_len, sizeof(msg) - msg_len);
        if (rv < 0) return EOS_ERR;
        msg_len += rv;
    }

    pdu_putc(0, pdu);
    rv = gsm_sms_enc(addr, addr_len, addr_type, NULL, 0, msg, msg_len, GSM_ENC_7BIT, flags, pdu + 2, sizeof(pdu) - 4);
    if (rv < 0) return EOS_ERR;
    pdu_len = rv;
    pdu[pdu_len + 2] = CTRL_Z;
    pdu[pdu_len + 3] = '\0';

    return EOS_OK;
}

void eos_cell_sms_handler(unsigned char mtype, unsigned char *buffer, uint16_t size) {
    int rv;
    char b[4];

    buffer += 1;
    size -= 1;
    switch (mtype) {
        case EOS_CELL_MTYPE_SMS_LIST:
            if (size == 0) return;
            snprintf(cmd, sizeof(cmd), "AT+CMGL=%d\r", buffer[0]);

            rv = eos_modem_take(1000);
            if (rv) return;
            at_cmd(cmd);
            do {
                unsigned char *buf;
                uint16_t len;

                rv = at_expect("^\\+CMGL: [0-9]+,[0-9],.*,[0-9]+$", "^OK", 1000);
                if (rv != 1) break;

                rv = eos_modem_readln(pdu, sizeof(pdu), 1000);
                if (rv) break;

                pdu_len = strlen(pdu);

                buf = eos_net_alloc();
                buf[0] = EOS_CELL_MTYPE_SMS | EOS_CELL_MTYPE_SMS_MSG_ITEM;
                rv = sms_decode(buf + 1, &len);
                if (rv) {
                    eos_net_free(buf);
                } else {
                    eos_net_send(EOS_NET_MTYPE_CELL, buf, len + 1);
                }
            } while (1);
            eos_modem_give();

            break;

        case EOS_CELL_MTYPE_SMS_SEND:
            rv = sms_encode(buffer, size);
            if (rv) return;

            snprintf(cmd, sizeof(cmd), "AT+CMGS=%d\r", pdu_len / 2);

            rv = eos_modem_take(1000);
            if (rv) return;
            at_cmd(cmd);
            // wait for: '> ' (0d 0a 3e 20)
            eos_modem_read(b, 4, 1000);
            at_cmd(pdu);
            rv = at_expect("^\\+CMGS: [0-9]+", "^ERROR", 5000);
            if (rv == 1) rv = at_expect("^OK", "^ERROR", 1000);
            eos_modem_give();

            break;
    }
}

static void sms_received_handler(char *urc, regmatch_t m[]) {
    int ref, rv;

    sscanf(urc + m[1].rm_so, "%6d", &ref);

    snprintf(cmd, sizeof(cmd), "AT+CMGR=%d\r", ref);
    at_cmd(cmd);

    rv = at_expect("^\\+CMGR: [0-9],.*,[0-9]+$", "^ERROR", 1000);
    if (rv == 1) {
        unsigned char *buf;
        uint16_t len;

        rv = eos_modem_readln(pdu, sizeof(pdu), 1000);
        if (rv) return;
        pdu_len = strlen(pdu);

        rv = at_expect("^OK", NULL, 1000);

        buf = eos_net_alloc();
        buf[0] = EOS_CELL_MTYPE_SMS | EOS_CELL_MTYPE_SMS_MSG_NEW;
        rv = sms_decode(buf + 1, &len);
        if (rv) {
            eos_net_free(buf);
        } else {
            len++;
            eos_net_send(EOS_NET_MTYPE_CELL, buf, len);
        }
    }
}

void eos_cell_sms_init(void) {
    at_urc_insert("^\\+CMTI: .*,([0-9]+)$", sms_received_handler, REG_EXTENDED);
}
