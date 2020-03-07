#include "eve.h"
#include "eve_text.h"

#define TEXT_ATTR   0x0A
#define TEXT_CRSR   0x02DB

#define LINE_IDX_ADD(l1,l2,s)       (((((l1) + (l2)) % (s)) + (s)) % (s))
#define LINE_IDX_SUB(l1,l2,s)       (((((l1) - (l2)) % (s)) + (s)) % (s))

#define LINE_IDX_LTE(l1,l2,s,h)     (LINE_IDX_SUB(l2,l1,s) <= (s) - (h))
#define LINE_IDX_DIFF(l1,l2,s)      ((l1) > (l2) ? (l1) - (l2) : (s) + (l1) - (l2))

static void scroll1(EVEText *box) {
    box->line0 = (box->line0 + 1) % box->line_size;
    eve_cmd(CMD_MEMSET, "www", box->mem_addr + box->ch_idx, 0x0, box->w * 2);
    eve_cmd_exec(1);
    box->dirty = 1;
}

void eve_text_init(EVEText *box, int16_t x, int16_t y, uint16_t w, uint16_t h, double scale_x, double scale_y, uint8_t tag, uint16_t line_size, uint32_t mem_addr, uint32_t *mem_next) {
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
    box->mem_addr = mem_addr;
    box->line_size = line_size;
    box->line0 = 0;
    box->line_top = -1;
    box->line_top0 = -1;
    box->ch_idx = 0;
    if (box->transform_a != 256) {
        box->dl_size += 1;
    }
    if (box->transform_e != 256) {
        box->dl_size += 1;
    }

    eve_touch_set_opt(tag, EVE_TOUCH_OPT_TRACK);

    eve_cmd(CMD_MEMSET, "www", mem_addr, 0x0, box->w * 2 * box->line_size);
    eve_cmd_exec(1);

    eve_text_update(box);
    *mem_next = box->mem_addr + box->w * 2 * box->line_size + box->dl_size * 4;
}

int eve_text_touch(EVEText *box, uint8_t tag0, int touch_idx) {
    EVETouch *t;
    uint16_t evt;
    int ret = 0;

    t = eve_touch_evt(tag0, touch_idx, box->tag, box->tag, &evt);
    if (t && evt) {
        if ((evt & EVE_TOUCH_ETYPE_TRACK_START) && (box->line_top < 0)) {
            box->line_top = box->line0;
            box->line_top0 = box->line0;
        }
        if ((evt & EVE_TOUCH_ETYPE_TRACK) && (box->line_top0 >=0)) {
            int line = LINE_IDX_ADD(box->line_top0, (t->y0 - t->y) / box->ch_h, box->line_size);
            if (LINE_IDX_LTE(line, box->line0, box->line_size, box->h)) {
                box->line_top = line;
                box->dirty = 1;
            }
        }
        if (evt & EVE_TOUCH_ETYPE_TRACK_STOP) {
            box->line_top0 = box->line_top;
        }
        ret = 1;
    } else if (box->line_top >= 0) {
        box->line_top = -1;
        box->line_top0 = -1;
        box->dirty = 1;
    }
    return ret;
}

uint8_t eve_text_draw(EVEText *box) {
    if (box->dirty) {
        eve_text_update(box);
        box->dirty = 0;
    }
    eve_cmd(CMD_APPEND, "ww", box->mem_addr + box->w * 2 * box->line_size, box->dl_size * 4);
    return box->tag;
}

int eve_text_putc(EVEText *box, int c) {
    int line_c, line_n;

    switch (c) {
        case '\b':
            eve_text_backspace(box);
            break;
        case '\r':
        case '\n':
            eve_text_newline(box);
            break;
        default:
            line_c = box->ch_idx / 2 / box->w;

            eve_write16(box->mem_addr + box->ch_idx, 0x0200 | (c & 0xff));
            box->ch_idx = (box->ch_idx + 2) % (box->line_size * box->w * 2);
            eve_write16(box->mem_addr + box->ch_idx, TEXT_CRSR);

            line_n = box->ch_idx / 2 / box->w;
            if ((line_c != line_n) && (LINE_IDX_DIFF(line_n, box->line0, box->line_size) == box->h)) scroll1(box);
            break;
    }
    return EVE_OK;
}

void eve_text_update(EVEText *box) {
    int text_h1;
    int text_h2;
    int line_top;

    line_top = box->line_top >= 0 ? box->line_top : box->line0;

    if (line_top + box->h > box->line_size) {
        text_h1 = box->line_size - line_top;
        text_h2 = box->h - text_h1;
    } else {
        text_h1 = box->h;
        text_h2 = 0;
    }
    eve_dl_start(box->mem_addr + box->w * 2 * box->line_size);

    eve_dl_write(SAVE_CONTEXT());
    eve_dl_write(BEGIN(EVE_BITMAPS));
    eve_dl_write(TAG(box->tag));
    eve_dl_write(VERTEX_FORMAT(0));
    eve_dl_write(BITMAP_HANDLE(15));
    if (box->transform_a != 256) {
        eve_dl_write(BITMAP_TRANSFORM_A(box->transform_a));
    }
    if (box->transform_e != 256) {
        eve_dl_write(BITMAP_TRANSFORM_E(box->transform_e));
    }
    eve_dl_write(BITMAP_SOURCE(box->mem_addr + line_top * box->w * 2));
    eve_dl_write(BITMAP_LAYOUT(EVE_TEXTVGA, box->w * 2, text_h1));
    eve_dl_write(BITMAP_SIZE(EVE_NEAREST, EVE_BORDER, EVE_BORDER, box->w * box->ch_w, text_h1 * box->ch_h));
    eve_dl_write(BITMAP_SIZE_H(box->w * box->ch_w, text_h1 * box->ch_h));
    eve_dl_write(VERTEX2F(box->x, box->y));

    if (text_h2) {
        eve_dl_write(BITMAP_SOURCE(box->mem_addr));
        eve_dl_write(BITMAP_LAYOUT(EVE_TEXTVGA, box->w * 2, text_h2));
        eve_dl_write(BITMAP_SIZE(EVE_NEAREST, EVE_BORDER, EVE_BORDER, box->w * box->ch_w, text_h2 * box->ch_h));
        eve_dl_write(BITMAP_SIZE_H(box->w * box->ch_w, text_h2 * box->ch_h));
        eve_dl_write(VERTEX2F(box->x, box->y + text_h1 * box->ch_h));
    } else {
        eve_dl_write(NOP());
        eve_dl_write(NOP());
        eve_dl_write(NOP());
        eve_dl_write(NOP());
        eve_dl_write(NOP());
    }

    eve_dl_write(END());
    eve_dl_write(RESTORE_CONTEXT());
}

void eve_text_newline(EVEText *box) {
    int line = (box->ch_idx / 2 / box->w + 1) % box->line_size;

    eve_write16(box->mem_addr + box->ch_idx, 0);
    box->ch_idx = line * box->w * 2;
    if (LINE_IDX_DIFF(line, box->line0, box->line_size) == box->h) scroll1(box);
    eve_write16(box->mem_addr + box->ch_idx, TEXT_CRSR);
}

void eve_text_backspace(EVEText *box) {
    uint16_t c;

    eve_write16(box->mem_addr + box->ch_idx, 0);
    box->ch_idx = (box->ch_idx - 2) % (box->line_size * box->w * 2);
    if (eve_read16(box->mem_addr + box->ch_idx) == 0) {
        box->ch_idx = (box->ch_idx + 2) % (box->line_size * box->w * 2);
    }
    eve_write16(box->mem_addr + box->ch_idx, TEXT_CRSR);
}