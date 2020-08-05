#include "unicode.h"

int utf8_enc(utf32_t ch, utf8_t *str) {
    if (ch <= 0x7f) {
        str[0] = ch;
        return 1;
    } else if (ch <= 0x7ff) {
        str[0] = 0xc0 | (ch >> 6);
        str[1] = 0x80 | (ch & 0x3f);
        return 2;
    } else if (ch <= 0xffff) {
        if ((ch >= 0xd800) && (ch <= 0xdfff)) return UTF_ERR;
        str[0] = 0xe0 | (ch >> 12);
        str[1] = 0x80 | ((ch >> 6) & 0x3f);
        str[2] = 0x80 | (ch & 0x3f);
        return 3;
    } else if (ch <= 0x10ffff) {
        str[0] = 0xf0 | (ch >> 18);
        str[1] = 0x80 | ((ch >> 12) & 0x3f);
        str[2] = 0x80 | ((ch >> 6) & 0x3f);
        str[3] = 0x80 | (ch & 0x3f);
        return 4;
    } else {
        return UTF_ERR;
    }
}

int utf8_dec(utf8_t *str, utf32_t *ch) {
    if ((str[0] & 0x80) == 0x00) {
        *ch = str[0];
        return 1;
    } else if ((str[0] & 0xe0) == 0xc0) {
        if ((str[1] & 0xc0) != 0x80) return UTF_ERR;
        *ch  = (utf32_t)(str[0] & 0x1f) << 6;
        *ch |= (utf32_t)(str[1] & 0x3f);
        if (*ch < 0x80) return UTF_ERR;
        return 2;
    } else if ((str[0] & 0xf0) == 0xe0) {
        if (((str[1] & 0xc0) != 0x80) || ((str[2] & 0xc0) != 0x80)) return UTF_ERR;
        *ch  = (utf32_t)(str[0] & 0x0f) << 12;
        *ch |= (utf32_t)(str[1] & 0x3f) << 6;
        *ch |= (utf32_t)(str[2] & 0x3f);
        if ((*ch >= 0xd800) && (*ch <= 0xdfff)) return UTF_ERR;
        if (*ch < 0x800) return UTF_ERR;
        return 3;
    } else if ((str[0] & 0xf8) == 0xf0) {
        if (((str[1] & 0xc0) != 0x80) || ((str[2] & 0xc0) != 0x80) || ((str[3] & 0xc0) != 0x80)) return UTF_ERR;
        *ch  = (utf32_t)(str[0] & 0x07) << 18;
        *ch |= (utf32_t)(str[1] & 0x0f) << 12;
        *ch |= (utf32_t)(str[2] & 0x3f) << 6;
        *ch |= (utf32_t)(str[3] & 0x3f);
        if (*ch < 0x010000) return UTF_ERR;
        if (*ch > 0x10ffff) return UTF_ERR;
        return 4;
    } else {
        return UTF_ERR;
    }
}

int utf8_seek(utf8_t *str, int off, utf32_t *ch) {
    int i;
    int len = 0;

    if (off < 0) {
        off = -off;
        for (i=0; i<off; i++) {
            len--;
            while ((str[len] & 0xc0) == 0x80) len--;
        }
    } else {
        for (i=0; i<off; i++) {
            if ((str[len] & 0x80) == 0x00) {
                len += 1;
            } else if ((str[0] & 0xe0) == 0xc0) {
                len += 2;
            } else if ((str[0] & 0xf0) == 0xe0) {
                len += 3;
            } else if ((str[0] & 0xf8) == 0xf0) {
                len += 4;
            }
        }
    }
    utf8_dec(str + len, ch);
    return len;
}

int utf8_verify(utf8_t *str, int str_size, int *str_len) {
    utf32_t ch;
    uint8_t ch_l;
    int len = 0;

    while (len < str_size) {
        if (str_size - len < 4) {
            if (((str[len] & 0xf8) == 0xf0) ||
               (((str[len] & 0xf0) == 0xe0) && (str_size - len < 3)) ||
               (((str[len] & 0xe0) == 0xc0) && (str_size - len < 2))) {
                   break;
               }
        }
        ch_l = utf8_dec(str + len, &ch);
        if (ch_l > 0) {
            if (ch == 0) {
                *str_len = len;
                return UTF_OK;
            }
            len += ch_l;
        } else {
            break;
        }
    }
    *str_len = len;
    return UTF_ERR;
}

int utf16_enc(utf32_t ch, uint8_t *str) {
    if (ch <= 0xffff) {
        if ((ch >= 0xd800) && (ch <= 0xdfff)) return UTF_ERR;
        str[0] = ch >> 8;
        str[1] = ch & 0xff;
        return 2;
    } else if (ch <= 0x10ffff) {
        uint16_t hi;
        uint16_t lo;

        ch -= 0x10000;
        hi = (ch >> 10) + 0xd800;
        lo = (ch & 0x3ff) + 0xdc00;
        str[0] = hi >> 8;
        str[1] = hi & 0xff;
        str[2] = lo >> 8;
        str[3] = lo & 0xff;
        return 4;
    } else {
        return UTF_ERR;
    }
}

int utf16_dec(uint8_t *str, utf32_t *ch) {
    *ch = (str[0] << 8) | str[1];
    if ((*ch >= 0xd800) && (*ch <= 0xdfff)) {
        uint16_t hi = *ch;
        uint16_t lo;

        if (hi > 0xdbff) return UTF_ERR;
        lo = (str[2] << 8) | str[3];
        if ((lo < 0xdc00) || (lo > 0xdfff)) return UTF_ERR;
        *ch = (((hi - 0xd800) << 10) | (lo - 0xdc00)) + 0x10000;
        return 4;
    } else {
        return 2;
    }
}

int utf16_seek(uint8_t *str, int off, utf32_t *ch) {
    int i;
    int len = 0;
    uint16_t cu;

    if (off < 0) {
        off = -off;
        for (i=0; i<off; i++) {
            len -= 2;
            cu = (str[len] << 8) | str[len + 1];
            if ((cu >= 0xdc00) && (cu <= 0xdfff)) {
                len -= 2;
            }
        }
    } else {
        for (i=0; i<off; i++) {
            cu = (str[len] << 8) | str[len + 1];
            if ((cu >= 0xd800) && (cu <= 0xdbff)) {
                len += 4;
            } else {
                len += 2;
            }
        }
    }
    utf16_dec(str + len, ch);
    return len;
}
