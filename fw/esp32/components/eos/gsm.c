#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "gsm.h"

#define DIVC(x,y)                   ((x) / (y) + ((x) % (y) != 0))

uint8_t pdu_getc(char *pdu) {
    int ch;
    sscanf(pdu, "%2X", &ch);
    return ch;
}

void pdu_putc(uint8_t ch, char *pdu) {
    sprintf(pdu, "%.2X", ch);
}

void pdu_gets(char *pdu, uint8_t *s, int s_len) {
    int i, ch;

    for (i=0; i<s_len; i++) {
        sscanf(pdu + 2 * i, "%2X", &ch);
        s[i] = ch;
    }
}

void pdu_puts(uint8_t *s, int s_len, char *pdu) {
    int i;

    for (i=0; i<s_len; i++) {
        sprintf(pdu + 2 * i, "%.2X", s[i]);
    }
}

void gsm_dcs_dec(uint8_t dcs, uint8_t *enc, uint16_t *flags) {
    if ((dcs & GSM_DCS_GENERAL_IND) == 0) {
        *enc = dcs & GSM_DCS_ENC;
        if (dcs & GSM_DCS_CLASS_IND) {
            *flags |= GSM_FLAG_CLASS;
            *flags |= (uint16_t)(dcs & GSM_DCS_CLASS) << 8;
        }
        if (dcs & GSM_DCS_COMPRESS_IND) *flags |= GSM_FLAG_COMPRESS;
        if (dcs & GSM_DCS_DELETE_IND) *flags |= GSM_FLAG_DELETE;
    } else {
        uint8_t group = dcs & GSM_DCS_GROUP;

        switch (group) {
            case GSM_DCS_MWI_DISCARD:
            case GSM_DCS_MWI_STORE_GSM7:
            case GSM_DCS_MWI_STORE_UCS2:
                if (group == GSM_DCS_MWI_STORE_UCS2) {
                    *enc = GSM_ENC_UCS2;
                } else {
                    *enc = GSM_ENC_7BIT;
                }
                if (GSM_DCS_MWI_DISCARD) *flags |= GSM_FLAG_DISCARD;
                *flags |= GSM_FLAG_MWI;
                *flags |= (uint16_t)(dcs & (GSM_DCS_MWI_SENSE | GSM_DCS_MWI_TYPE)) << 12;
                break;

            case GSM_DCS_ENCLASS:
                *flags |= GSM_FLAG_CLASS;
                *flags |= (uint16_t)(dcs & GSM_DCS_CLASS) << 8;
                *enc = dcs & GSM_DCS_ENCLASS_ENC ? GSM_ENC_8BIT : GSM_ENC_7BIT;
                break;
        }
    }
}

void gsm_dcs_enc(uint8_t enc, uint16_t flags, uint8_t *dcs) {
    *dcs = enc;
    if (flags & GSM_FLAG_CLASS) {
        *dcs |= GSM_DCS_CLASS_IND;
        *dcs |= (flags >> 8) & GSM_DCS_CLASS;
    }
    if (flags & GSM_FLAG_COMPRESS) *dcs |= GSM_DCS_COMPRESS_IND;
    if (flags & GSM_FLAG_DELETE) *dcs |= GSM_DCS_DELETE_IND;
}

int gsm_ts_enc(char *ts, char *pdu, int pdu_size) {
    uint8_t tz;
    int tz_hh, tz_mm;

    if (pdu_size < 14) return GSM_ERR_SIZE;

    pdu[1]  = ts[2];    // YY
    pdu[0]  = ts[3];

    pdu[3]  = ts[5];    // MM
    pdu[2]  = ts[6];

    pdu[5]  = ts[8];    // DD
    pdu[4]  = ts[9];

    pdu[7]  = ts[11];   // hh
    pdu[6]  = ts[12];

    pdu[9]  = ts[14];   // mm
    pdu[8]  = ts[15];

    pdu[11] = ts[17];   // ss
    pdu[10] = ts[18];

    sscanf(ts + 20, "%2d:%2d", &tz_hh, &tz_mm);
    tz = tz_hh * 4 + tz_mm / 15;
    tz = (tz / 10) | ((tz % 10) << 4);
    if (ts[19] == '-') tz |= 0x08;

    pdu_putc(tz, pdu + 12);

    return 14;
}

int gsm_ts_dec(char *pdu, int pdu_len, char *ts) {
    uint8_t tz;

    if (pdu_len < 14) return GSM_ERR_SIZE;

    ts[0]  = '2';
    ts[1]  = '0';
    ts[2]  = pdu[1];    // YY
    ts[3]  = pdu[0];
    ts[4]  = '-';
    ts[5]  = pdu[3];    // MM
    ts[6]  = pdu[2];
    ts[7]  = '-';
    ts[8]  = pdu[5];    // DD
    ts[9]  = pdu[4];
    ts[10] = 'T';
    ts[11] = pdu[7];    // hh
    ts[12] = pdu[6];
    ts[13] = ':';
    ts[14] = pdu[9];    // mm
    ts[15] = pdu[8];
    ts[16] = ':';
    ts[17] = pdu[11];   // ss
    ts[18] = pdu[10];

    tz = pdu_getc(pdu + 12);
    if (tz & 0x08) {
        ts[19] = '-';
        tz = tz & ~0x08;
    } else {
        ts[19] = '+';
    }
    tz = (tz & 0x0f) * 10 + (tz >> 4);
    sprintf(ts + 20, "%.2d:%.2d", tz / 4, (tz % 4) * 15);

    return 14;
}

int gsm_7bit_enc(char *text, int text_len, char *pdu, int padb) {
    uint8_t carry = 0;
    int i = 0, pdu_len = 0, shc = 0;

    if (!text_len) return 0;

    if (padb) {
        shc = 7 - padb;
    } else {
        carry = *text;
        i++;
    }

    while (i < text_len) {
        pdu_putc(carry | (*(text + i) << (7 - shc)), pdu + pdu_len);
        pdu_len += 2;

        shc++;
        shc = shc % 7;
        if (!shc) {
            i++;
            if (i == text_len) return pdu_len;
        }

        carry = *(text + i) >> shc;
        i++;
    }
    pdu_putc(carry, pdu + pdu_len);
    pdu_len += 2;

    return pdu_len;
}

int gsm_7bit_dec(char *pdu, char *text, int text_len, int padb) {
    uint8_t ch;
    uint8_t carry = 0;
    int i = 0, pdu_len = 0, shc = 0;

    if (!text_len) return 0;

    if (padb) {
        ch = pdu_getc(pdu);
        pdu_len += 2;
        if (padb == 1) {
            *text = ch >> 1;
            i++;
        } else {
            carry = ch >> padb;
            shc = 8 - padb;
        }
    }

    while (i < text_len) {
        ch = pdu_getc(pdu + pdu_len);
        pdu_len += 2;

        *(text + i) = ((ch << shc) | carry) & 0x7f;
        carry = ch >> (7 - shc);
        i++;

        shc++;
        shc = shc % 7;
        if (!shc && (i < text_len)) {
            *(text + i) = carry;
            carry = 0;
            i++;
        }
    }

    return pdu_len;
}

int gsm_addr_enc(char *addr, int addr_len, uint8_t addr_type, char *pdu, int pdu_size) {
    int _pdu_len;

    addr_type |= GSM_EXT;

    if ((addr_type & GSM_TON) == GSM_TON_ALPHANUMERIC) {
        int _addr_len = DIVC(addr_len * 7, 4);

        _pdu_len = 4 + DIVC(_addr_len, 2) * 2;
        if (pdu_size < _pdu_len) return GSM_ERR_SIZE;

        pdu_putc(_addr_len, pdu);
        pdu_putc(addr_type, pdu + 2);
        gsm_7bit_enc(addr, addr_len, pdu, 0);
    } else {
        int i;

        _pdu_len = 4 + DIVC(addr_len, 2) * 2;
        if (pdu_size < _pdu_len) return GSM_ERR_SIZE;

        pdu_putc(addr_len, pdu);
        pdu_putc(addr_type, pdu + 2);
        for (i=0; i<addr_len / 2; i++) {
            pdu[4 + 2 * i] = addr[2 * i + 1];
            pdu[4 + 2 * i + 1] = addr[2 * i];
        }
        if (addr_len % 2 != 0) {
            pdu[4 + 2 * i] = 'F';
            pdu[4 + 2 * i + 1] = addr[2 * i];
        }
    }

    return _pdu_len;
}

int gsm_addr_dec(char *pdu, int pdu_len, char *addr, int *addr_len, uint8_t *addr_type) {
    int _pdu_len;

    if (pdu_len < 4) return GSM_ERR_SIZE;

    *addr_len = pdu_getc(pdu);
    *addr_type = pdu_getc(pdu + 2);
    if (*addr_len > GSM_ADDR_SIZE) return GSM_ERR_SIZE;

    if (!(*addr_type & GSM_EXT)) return GSM_ERR;

    _pdu_len = 4 + DIVC(*addr_len, 2) * 2;
    if (pdu_len < _pdu_len) return GSM_ERR_SIZE;

    if ((*addr_type & GSM_TON) == GSM_TON_ALPHANUMERIC) {
        *addr_len = (*addr_len * 4) / 7;

        gsm_7bit_dec(pdu + 4, addr, *addr_len, 0);
    } else {
        int i;

        for (i=0; i<*addr_len / 2; i++) {
            addr[2 * i] = pdu[4 + 2 * i + 1];
            addr[2 * i + 1] = pdu[4 + 2 * i];
        }
        if (*addr_len % 2 != 0) {
            addr[2 * i] = pdu[4 + 2 * i + 1];
        }
    }

    return _pdu_len;
}

int gsm_sms_enc(uint8_t pid, char *addr, int addr_len, uint8_t addr_type, uint8_t *udh, int udh_len, uint8_t *msg, int msg_len, uint8_t enc, uint16_t flags, char *pdu, int pdu_size) {
    int rv, _pdu_len = 0;
    uint8_t mti = GSM_MTI_SUBMIT;
    uint8_t dcs = 0;
    uint8_t udl;

    if (udh_len) mti |= GSM_UDHI;

    if (pdu_size < 4) return GSM_ERR_SIZE;
    pdu_putc(mti, pdu);
    pdu_putc(00, pdu + 2);
    _pdu_len += 4;

    rv = gsm_addr_enc(addr, addr_len, addr_type, pdu + _pdu_len, pdu_size - _pdu_len);
    if (rv < 0) return rv;
    _pdu_len += rv;

    if (pdu_size < _pdu_len + 4) return GSM_ERR_SIZE;
    gsm_dcs_enc(enc, flags, &dcs);
    pdu_putc(pid, pdu + _pdu_len);
    pdu_putc(dcs, pdu + _pdu_len + 2);
    _pdu_len += 4;

    if (enc == GSM_ENC_7BIT) {
        int udh_blen = 0;
        int padb = 0;

        if (udh_len) {
            udh_blen = 8 * (udh_len + 1);
            padb = DIVC(udh_blen, 7) * 7 - udh_blen;
        }
        udl = DIVC(udh_blen, 7) + msg_len;

        if (pdu_size < _pdu_len + (DIVC(udl * 7, 8) + 1) * 2) return GSM_ERR_SIZE;
        pdu_putc(udl, pdu + _pdu_len);
        _pdu_len += 2;

        if (udh_len) {
            pdu_putc(udh_len, pdu + _pdu_len);
            pdu_puts(udh, udh_len, pdu + _pdu_len + 2);
            _pdu_len += (udh_len + 1) * 2;
        }

        rv = gsm_7bit_enc((char *)msg, msg_len, pdu + _pdu_len, padb);
        if (rv < 0) return rv;
        _pdu_len += rv;
    } else {
        udl = msg_len + (udh_len ? udh_len + 1 : 0);

        if (pdu_size < _pdu_len + (udl + 1) * 2) return GSM_ERR_SIZE;
        pdu_putc(udl, pdu + _pdu_len);
        _pdu_len += 2;

        if (udh_len) {
            pdu_putc(udh_len, pdu + _pdu_len);
            pdu_puts(udh, udh_len, pdu + _pdu_len + 2);
            _pdu_len += (udh_len + 1) * 2;
        }

        pdu_puts(msg, msg_len, pdu + _pdu_len);
        _pdu_len += msg_len * 2;
    }

    return _pdu_len;
}

int gsm_sms_dec(char *pdu, int pdu_len, uint8_t *pid, char *addr, int *addr_len, uint8_t *addr_type, uint8_t *udh, int *udh_len, uint8_t *msg, int *msg_len, char *ts, uint8_t *enc, uint16_t *flags) {
    int rv, _pdu_len = 0;
    uint8_t mti;
    uint8_t dcs;
    uint8_t udl;

    if (pdu_len < 2) return GSM_ERR_SIZE;
    mti = pdu_getc(pdu);
    _pdu_len += 2;
    if ((mti & GSM_MTI) != GSM_MTI_DELIVER) return GSM_ERR;

    rv = gsm_addr_dec(pdu + _pdu_len, pdu_len - _pdu_len, addr, addr_len, addr_type);
    if (rv < 0) return rv;
    _pdu_len += rv;

    if (pdu_len < _pdu_len + 4) return GSM_ERR_SIZE;
    *pid = pdu_getc(pdu + _pdu_len);
    dcs = pdu_getc(pdu + _pdu_len + 2);
    _pdu_len += 4;
    gsm_dcs_dec(dcs, enc, flags);

    rv = gsm_ts_dec(pdu + _pdu_len, pdu_len - _pdu_len, ts);
    if (rv < 0) return rv;
    _pdu_len += rv;

    if (pdu_len < _pdu_len + 2) return GSM_ERR_SIZE;
    udl = pdu_getc(pdu + _pdu_len);
    _pdu_len += 2;

    if ((mti & GSM_UDHI) && (udl == 0)) return GSM_ERR;
    *udh_len = 0;

    if (*enc == GSM_ENC_7BIT) {
        int udh_blen = 0;
        int padb = 0;

        if (pdu_len < _pdu_len + DIVC(udl * 7, 8) * 2) return GSM_ERR_SIZE;

        if (mti & GSM_UDHI) {
            *udh_len = pdu_getc(pdu + _pdu_len);
            udh_blen = 8 * (*udh_len + 1);
            padb = DIVC(udh_blen, 7) * 7 - udh_blen;

            if (udl * 7 < udh_blen) return GSM_ERR;
            if (*udh_len > GSM_UDH_SIZE) return GSM_ERR_SIZE;

            pdu_gets(pdu + _pdu_len + 2, udh, *udh_len);
            _pdu_len += (*udh_len + 1) * 2;
        } else {
            *udh_len = 0;
        }

        *msg_len = udl - DIVC(udh_blen, 7);
        if (*msg_len > GSM_UD_SIZE) return GSM_ERR_SIZE;

        rv = gsm_7bit_dec(pdu + _pdu_len, (char *)msg, *msg_len, padb);
        if (rv < 0) return rv;
        _pdu_len += rv;
    } else {
        if (pdu_len < _pdu_len + udl * 2) return GSM_ERR_SIZE;

        if (mti & GSM_UDHI) {
            *udh_len = pdu_getc(pdu + _pdu_len);

            if (udl < *udh_len + 1) return GSM_ERR;
            if (*udh_len > GSM_UDH_SIZE) return GSM_ERR_SIZE;

            pdu_gets(pdu + _pdu_len + 2, udh, *udh_len);
            _pdu_len += (*udh_len + 1) * 2;
        } else {
            *udh_len = 0;
        }

        *msg_len = udl - (*udh_len ? *udh_len + 1 : 0);
        if (*msg_len > GSM_UD_SIZE) return GSM_ERR_SIZE;
        if ((*enc == GSM_ENC_UCS2) && ((*msg_len % 2) != 0)) return GSM_ERR;

        pdu_gets(pdu + _pdu_len, msg, *msg_len);
        _pdu_len += *msg_len * 2;
    }

    return _pdu_len;
}
