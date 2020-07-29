#include "unicode.h"

uint8_t utf8_enc(utf32_t ch, utf8_t *str) {
    if (ch <= 0x7f) {
        str[0] = ch;
        return 1;
    } else if (ch <= 0x7ff) {
        str[0] = 0xc0 | (ch >> 6);
        str[1] = 0x80 | (ch & 0x3f);
        return 2;
    } else if (ch <= 0xffff) {
        if ((ch >= 0xd800) && (ch <= 0xdfff)) return 0;
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
        return 0;
    }
}

uint8_t utf8_dec(utf8_t *str, utf32_t *ch) {
    if ((str[0] & 0x80) == 0x00) {
        *ch = str[0];
        return 1;
    } else if ((str[0] & 0xe0) == 0xc0) {
        if ((str[1] & 0xc0) != 0x80) return 0;
        *ch  = (utf32_t)(str[0] & 0x1f) << 6;
        *ch |= (utf32_t)(str[1] & 0x3f);
        if (*ch < 0x80) return 0;
        return 2;
    } else if ((str[0] & 0xf0) == 0xe0) {
        if (((str[1] & 0xc0) != 0x80) || ((str[2] & 0xc0) != 0x80)) return 0;
        *ch  = (utf32_t)(str[0] & 0x0f) << 12;
        *ch |= (utf32_t)(str[1] & 0x3f) << 6;
        *ch |= (utf32_t)(str[2] & 0x3f);
        if ((*ch >= 0xd800) && (*ch <= 0xdfff)) return 0;
        if (*ch < 0x800) return 0;
        return 3;
    } else if ((str[0] & 0xf8) == 0xf0) {
        if (((str[1] & 0xc0) != 0x80) || ((str[2] & 0xc0) != 0x80) || ((str[3] & 0xc0) != 0x80)) return 0;
        *ch  = (utf32_t)(str[0] & 0x07) << 18;
        *ch |= (utf32_t)(str[1] & 0x0f) << 12;
        *ch |= (utf32_t)(str[2] & 0x3f) << 6;
        *ch |= (utf32_t)(str[3] & 0x3f);
        if (*ch < 0x010000) return 0;
        if (*ch > 0x10ffff) return 0;
        return 4;
    } else {
        return 0;
    }
}

int utf8_seek(utf8_t *str, int off, utf32_t *ch) {
    int i;
    int len = 0;

    if (off < 0) {
        off = -off;
        for (i=0; i<off; i++) {
            len--;
            while ((*(str + len) & 0xc0) == 0x80) len--;
        }
        utf8_dec(str + len, ch);
    } else {
        for (i=0; i<off; i++) {
            len += utf8_dec(str + len, ch);
        }
    }
    return len;
}

int utf8_verify(utf8_t *str, int sz) {
    utf32_t ch;
    uint8_t ch_l;
    int len = 0;

    while (len < sz) {
        if (sz - len < 4) {
            if (((str[len] & 0xf8) == 0xf0) ||
               (((str[len] & 0xf0) == 0xe0) && (sz - len < 3)) ||
               (((str[len] & 0xe0) == 0xc0) && (sz - len < 2))) {
                   str[len] = '\0';
                   break;
               }
        }
        ch_l = utf8_dec(str + len, &ch);
        if (ch_l) {
            if (ch == 0) break;
            len += ch_l;
        } else {
            str[len] = '\0';
            break;
        }
    }
    return len;
}
