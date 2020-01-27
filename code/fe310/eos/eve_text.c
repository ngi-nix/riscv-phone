#include "eve.h"
#include "eve_text.h"

#define TEXT_ATTR   0x0A
#define TEXT_CRSR   0x02DB

#define LINE_IDX_ADD(l1,l2,s)       (((((l1) + (l2)) % (s)) + (s)) % (s))
#define LINE_IDX_SUB(l1,l2,s)       (((((l1) - (l2)) % (s)) + (s)) % (s))

#define LINE_IDX_LTE(l1,l2,s,h)     (LINE_IDX_SUB(l2,l1,s) <= (s) - (h))
#define LINE_IDX_DIFF(l1,l2,s)      ((l1) > (l2) ? (l1) - (l2) : (s) - (l2) + (l1))

static void scroll1(EOSText *box) {
    box->line_idx = (box->line_idx + 1) % box->buf_line_h;
    eos_eve_cmd(CMD_MEMSET, "www", box->buf_addr + box->buf_idx, 0x0, box->w * 2);
    eos_eve_cmd_exec(1);
    box->dirty = 1;
}

void eos_text_init(EOSText *box, int x, int y, int w, int h, double scale_x, double scale_y, uint8_t tag, int buf_line_h, uint32_t mem_addr, uint32_t *mem_next) {
    box->x = x;
    box->y = y;
    box->w = w;
    box->h = h;
    box->tag = tag;
    box->transform_a = 256/scale_x;
    box->transform_e = 256/scale_y;
    box->ch_w = scale_x * 8;
    box->ch_h = scale_y * 16;
    box->dl_size = 17;
    box->buf_addr = mem_addr;
    box->buf_line_h = buf_line_h;
    box->buf_idx = 0;
    box->line_idx = 0;
    if (box->transform_a != 256) {
        box->dl_size += 1;
    }
    if (box->transform_e != 256) {
        box->dl_size += 1;
    }

    eos_touch_evt_set(tag, EOS_TOUCH_ETYPE_TRACK);

    eos_eve_cmd(CMD_MEMSET, "www", mem_addr, 0x0, w * 2 * buf_line_h);
    eos_eve_cmd_exec(1);

    eos_text_update(box, -1);
    *mem_next = box->buf_addr + box->w * 2 * box->buf_line_h + box->dl_size * 4;
}

void eos_text_draw(EOSText *box, uint8_t tag0, int touch_idx) {
    static int line_idx = -1;
    static int line_idx_last;
    uint8_t evt;
    EOSTouch *t = eos_touch_evt(tag0, touch_idx, box->tag, box->tag, &evt);

    if (t && evt) {
        if (evt & EOS_TOUCH_ETYPE_TAG_DOWN) {
            if (line_idx < 0) {
                line_idx = box->line_idx;
                line_idx_last = line_idx;
            }
        }
        if (evt & EOS_TOUCH_ETYPE_TAG_UP) {
            line_idx = line_idx_last;
        }
        if (evt & EOS_TOUCH_ETYPE_TRACK) {
            int _line_idx = LINE_IDX_ADD(line_idx, ((int)t->y0 - t->y) / box->ch_h, box->buf_line_h);
            if (LINE_IDX_LTE(_line_idx, box->line_idx, box->buf_line_h, box->h)) {
                eos_text_update(box, _line_idx);
                line_idx_last = _line_idx;
            }
        }
    } else if (line_idx >= 0) {
        line_idx = -1;
        box->dirty = 1;
    }

    if (box->dirty) {
        eos_text_update(box, -1);
        box->dirty = 0;
    }
    eos_eve_cmd(CMD_APPEND, "ww", box->buf_addr + box->w * 2 * box->buf_line_h, box->dl_size * 4);
}

void eos_text_update(EOSText *box, int line_idx) {
    int text_h1;
    int text_h2;

    if (line_idx < 0) line_idx = box->line_idx;

    if (line_idx + box->h > box->buf_line_h) {
        text_h1 = box->buf_line_h - line_idx;
        text_h2 = box->h - text_h1;
    } else {
        text_h1 = box->h;
        text_h2 = 0;
    }
    eos_eve_dl_start(box->buf_addr + box->w * 2 * box->buf_line_h);

    eos_eve_dl_write(SAVE_CONTEXT());
    eos_eve_dl_write(BEGIN(EVE_BITMAPS));
    eos_eve_dl_write(TAG(box->tag));
    eos_eve_dl_write(VERTEX_FORMAT(0));
    eos_eve_dl_write(BITMAP_HANDLE(15));
    if (box->transform_a != 256) {
        eos_eve_dl_write(BITMAP_TRANSFORM_A(box->transform_a));
    }
    if (box->transform_e != 256) {
        eos_eve_dl_write(BITMAP_TRANSFORM_E(box->transform_e));
    }
    eos_eve_dl_write(BITMAP_SOURCE(box->buf_addr + line_idx * box->w * 2));
    eos_eve_dl_write(BITMAP_LAYOUT(EVE_TEXTVGA, box->w * 2, text_h1));
    eos_eve_dl_write(BITMAP_SIZE(EVE_NEAREST, EVE_BORDER, EVE_BORDER, box->w * box->ch_w, text_h1 * box->ch_h));
    eos_eve_dl_write(BITMAP_SIZE_H(box->w * box->ch_w, text_h1 * box->ch_h));
    eos_eve_dl_write(VERTEX2F(box->x, box->y));

    if (text_h2) {
        eos_eve_dl_write(BITMAP_SOURCE(box->buf_addr));
        eos_eve_dl_write(BITMAP_LAYOUT(EVE_TEXTVGA, box->w * 2, text_h2));
        eos_eve_dl_write(BITMAP_SIZE(EVE_NEAREST, EVE_BORDER, EVE_BORDER, box->w * box->ch_w, text_h2 * box->ch_h));
        eos_eve_dl_write(BITMAP_SIZE_H(box->w * box->ch_w, text_h2 * box->ch_h));
        eos_eve_dl_write(VERTEX2F(box->x, box->y + text_h1 * box->ch_h));
    } else {
        eos_eve_dl_write(NOP());
        eos_eve_dl_write(NOP());
        eos_eve_dl_write(NOP());
        eos_eve_dl_write(NOP());
        eos_eve_dl_write(NOP());
    }

    eos_eve_dl_write(END());
    eos_eve_dl_write(RESTORE_CONTEXT());
}

void eos_text_putc(EOSText *box, int c) {
    int line_c, line_n;

    switch (c) {
        case '\b':
            eos_text_backspace(box);
            break;
        case '\r':
        case '\n':
            eos_text_newline(box);
            break;
        default:
            line_c = box->buf_idx / 2 / box->w;

            eos_eve_write16(box->buf_addr + box->buf_idx, 0x0200 | (c & 0xff));
            box->buf_idx = (box->buf_idx + 2) % (box->buf_line_h * box->w * 2);
            eos_eve_write16(box->buf_addr + box->buf_idx, TEXT_CRSR);

            line_n = box->buf_idx / 2 / box->w;
            if ((line_c != line_n) && (LINE_IDX_DIFF(line_n, box->line_idx, box->buf_line_h) == box->h)) scroll1(box);
            break;
    }
}

void eos_text_newline(EOSText *box) {
    int line = (box->buf_idx / 2 / box->w + 1) % box->buf_line_h;

    eos_eve_write16(box->buf_addr + box->buf_idx, 0);
    box->buf_idx = line * box->w * 2;
    if (LINE_IDX_DIFF(line, box->line_idx, box->buf_line_h) == box->h) scroll1(box);
    eos_eve_write16(box->buf_addr + box->buf_idx, TEXT_CRSR);
}

void eos_text_backspace(EOSText *box) {
    uint16_t c;

    eos_eve_write16(box->buf_addr + box->buf_idx, 0);
    box->buf_idx = (box->buf_idx - 2) % (box->buf_line_h * box->w * 2);
    if (eos_eve_read16(box->buf_addr + box->buf_idx) == 0) {
        box->buf_idx = (box->buf_idx + 2) % (box->buf_line_h * box->w * 2);
    }
    eos_eve_write16(box->buf_addr + box->buf_idx, TEXT_CRSR);
}