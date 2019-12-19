#include "eve.h"
#include "eve_text.h"

static int _line_diff(EVETextBox *box, int line) {
    if (line > box->line_idx) {
        return line - box->line_idx;
    } else {
        return box->buf_line_h - box->line_idx + line;
    }
}

static void _line_scroll(EVETextBox *box) {
    box->line_idx = (box->line_idx + 1) % box->buf_line_h;
    eos_eve_cmd(CMD_MEMSET, "www", EVE_RAM_G + box->buf_addr + box->buf_idx, 0x0, box->w * 2);
    eos_eve_cmd_exec(1);
    eos_eve_dl_start(box->dl_offset);
    eos_eve_text_update(box);
    eos_eve_dl_swap();
}

void eos_eve_text_init(EVETextBox *box, uint16_t buf_addr, int buf_line_h, int w, int h, int x, int y, uint8_t bitmap_handle) {
    box->buf_addr = buf_addr;
    box->buf_line_h = buf_line_h;
    box->w = w;
    box->h = h;
    box->x = x;
    box->y = y;
    box->buf_idx = 0;
    box->line_idx = 0;
    box->dl_offset = 0;
    box->bitmap_handle = bitmap_handle;

    eos_eve_cmd(CMD_MEMSET, "www", EVE_RAM_G + buf_addr, 0x0, w * 2 * buf_line_h);
    eos_eve_cmd_exec(1);
}

void eos_eve_text_update(EVETextBox *box) {
    int text_h1;
    int text_h2;

    eos_eve_dl_write(BITMAP_HANDLE(box->bitmap_handle));
    eos_eve_dl_write(BITMAP_SOURCE(box->buf_addr + box->line_idx * box->w * 2));
    if (box->line_idx + box->h > box->buf_line_h) {
        text_h1 = box->buf_line_h - box->line_idx;
        text_h2 = box->h - text_h1;
    } else {
        text_h1 = box->h;
        text_h2 = 0;
    }
    eos_eve_dl_write(BITMAP_LAYOUT(EVE_TEXTVGA, box->w * 2, text_h1));
    eos_eve_dl_write(BITMAP_SIZE(EVE_NEAREST, EVE_BORDER, EVE_BORDER, box->w * 8, text_h1 * 16));

    if (text_h2) {
        eos_eve_dl_write(BITMAP_HANDLE(box->bitmap_handle+1));
        eos_eve_dl_write(BITMAP_SOURCE(box->buf_addr));
        eos_eve_dl_write(BITMAP_LAYOUT(EVE_TEXTVGA, box->w * 2, text_h2));
        eos_eve_dl_write(BITMAP_SIZE(EVE_NEAREST, EVE_BORDER, EVE_BORDER, box->w * 8, text_h2 * 16));
    } else {
        eos_eve_dl_write(NOP());
        eos_eve_dl_write(NOP());
        eos_eve_dl_write(NOP());
        eos_eve_dl_write(NOP());
    }

    eos_eve_dl_write(BEGIN(EVE_BITMAPS));
    eos_eve_dl_write(VERTEX2II(box->x, box->y, 0, 0));
    if (text_h2) {
        eos_eve_dl_write(VERTEX2II(box->x, box->y + text_h1 * 16, 1, 0));
    } else {
        eos_eve_dl_write(NOP());
    }
    eos_eve_dl_write(END());
}

void eos_eve_text_draw(EVETextBox *box) {
    box->dl_offset = eos_eve_dl_offset();
    eos_eve_text_update(box);
}

void eos_eve_text_putc(EVETextBox *box, int c) {
    int line_c, line_n;

    line_c = box->buf_idx / 2 / box->w;

    eos_eve_write16(EVE_RAM_G + box->buf_addr + box->buf_idx, 0x0A00 | (c & 0xff));
    box->buf_idx = (box->buf_idx + 2) % (box->buf_line_h * box->w * 2);

    line_n = box->buf_idx / 2 / box->w;
    if ((line_c != line_n) && (_line_diff(box, line_n) == box->h)) {
        _line_scroll(box);
    }
}

void eos_eve_text_newline(EVETextBox *box) {
    int line = (box->buf_idx / 2 / box->w + 1) % box->buf_line_h;
    box->buf_idx = line * box->w * 2;
    if (_line_diff(box, line) == box->h) {
        _line_scroll(box);
    }
}