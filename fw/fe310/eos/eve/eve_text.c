#include <stdlib.h>
#include <string.h>

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
    eve_text_update(box);
    box->dirty = 1;
}

void eve_text_init(EVEText *box, EVERect *g, uint16_t w, uint16_t h, uint16_t line_size, uint32_t mem_addr, uint32_t *mem_next) {
    double scale_x, scale_y;

    if (g->w == 0) {
        g->w = w * 8;
    }
    if (g->h == 0) {
        g->h = h * 16;
    }

    scale_x = (double)g->w / (w * 8);
    scale_y = (double)g->h / (h * 16);
    box->g = *g;
    box->w = w;
    box->h = h;
    box->tag = EVE_NOTAG;
    box->transform_a = 256 / scale_x;
    box->transform_e = 256 / scale_y;
    box->ch_w = scale_x * 8;
    box->ch_h = scale_y * 16;
    box->dl_size = 14;
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

    eve_cmd(CMD_MEMSET, "www", mem_addr, 0x0, box->w * 2 * box->line_size);
    eve_cmd_exec(1);

    eve_text_update(box);
    *mem_next = box->mem_addr + box->w * 2 * box->line_size + box->dl_size * 4;
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

    eve_dl_start(box->mem_addr + box->w * 2 * box->line_size, 1);
    eve_dl_write(BEGIN(EVE_BITMAPS));
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
    eve_dl_write(VERTEX2F(box->g.x, box->g.y));

    if (text_h2) {
        eve_dl_write(BITMAP_SOURCE(box->mem_addr));
        eve_dl_write(BITMAP_LAYOUT(EVE_TEXTVGA, box->w * 2, text_h2));
        eve_dl_write(BITMAP_SIZE(EVE_NEAREST, EVE_BORDER, EVE_BORDER, box->w * box->ch_w, text_h2 * box->ch_h));
        eve_dl_write(BITMAP_SIZE_H(box->w * box->ch_w, text_h2 * box->ch_h));
        eve_dl_write(VERTEX2F(box->g.x, box->g.y + text_h1 * box->ch_h));
    } else {
        eve_dl_write(NOP());
        eve_dl_write(NOP());
        eve_dl_write(NOP());
        eve_dl_write(NOP());
        eve_dl_write(NOP());
    }

    eve_dl_write(END());
    eve_dl_end();
}

void eve_text_scroll0(EVEText *box) {
    if (box->line_top >= 0) {
        box->line_top = -1;
        box->line_top0 = -1;
        eve_text_update(box);
    }
}

int eve_text_touch(EVEText *box, EVETouch *touch, uint16_t evt, uint8_t tag0) {
    evt = eve_touch_evt(touch, evt, tag0, box->tag, 1);
    if (touch && evt) {
        if ((evt & EVE_TOUCH_ETYPE_TRACK_START) && (box->line_top < 0)) {
            box->line_top = box->line0;
            box->line_top0 = box->line0;
        }
        if ((evt & EVE_TOUCH_ETYPE_TRACK) && (box->line_top0 >=0)) {
            int line = LINE_IDX_ADD(box->line_top0, (touch->y0 - touch->y) / box->ch_h, box->line_size);
            if (LINE_IDX_LTE(line, box->line0, box->line_size, box->h)) {
                box->line_top = line;
                eve_text_update(box);
            }
        }
        if (evt & EVE_TOUCH_ETYPE_TRACK_STOP) {
            box->line_top0 = box->line_top;
        }

        return 1;
    }

    return 0;
}

uint8_t eve_text_draw(EVEText *box, uint8_t tag) {
    eve_cmd_dl(SAVE_CONTEXT());
    box->tag = tag;
    if (tag != EVE_NOTAG) {
        eve_cmd_dl(TAG(tag));
        eve_touch_set_opt(tag, EVE_TOUCH_OPT_TRACK | EVE_TOUCH_OPT_TRACK_EXT_Y);
        tag++;
    }
    eve_cmd(CMD_APPEND, "ww", box->mem_addr + box->w * 2 * box->line_size, box->dl_size * 4);
    eve_cmd_dl(RESTORE_CONTEXT());

    return tag;
}

void eve_text_putc(EVEText *box, int c) {
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

void eve_text_newline(EVEText *box) {
    int line = (box->ch_idx / 2 / box->w + 1) % box->line_size;

    eve_write16(box->mem_addr + box->ch_idx, 0);
    box->ch_idx = line * box->w * 2;
    if (LINE_IDX_DIFF(line, box->line0, box->line_size) == box->h) scroll1(box);
    eve_write16(box->mem_addr + box->ch_idx, TEXT_CRSR);
}

void eve_text_puts(EVEText *box, char *s) {
    while (*s) {
        eve_text_putc(box, *s);
        s++;
    }
}
