#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "eve.h"
#include "eve_kbd.h"

#include "screen/screen.h"
#include "screen/window.h"
#include "screen/page.h"
#include "screen/font.h"

#include "label.h"
#include "widget.h"
#include "text.h"

#define CH_BS                   0x08
#define CH_DEL                  0x7f
#define CH_EOF                  0x1a

#define CH_CTRLX                0x18
#define CH_CTRLC                0x03
#define CH_CTRLV                0x16

#define TEXTW_TMODE_CURSOR      1
#define TEXTW_TMODE_SCROLL      2

#define LINE_LEN(w,l)           ((l) ? (w->line[l] ? w->line[l] - w->line[(l) - 1] - 1 : 0) : w->line[l])
#define LINE_START(w,l)         ((l) ? w->line[(l) - 1] + 1 : 0)
#define LINE_END(w,l)           (w->line[l])
#define LINE_EMPTY              0xffff

#define CHAR_VALID_INPUT(c)     (((c >= 0x20) && (c < 0x7f)) || (c == '\n'))

void eve_textw_init(EVETextWidget *widget, EVERect *g, EVEFont *font, char *text, uint16_t text_size, uint16_t *line, uint16_t line_size) {
    EVEWidget *_widget = &widget->w;

    memset(widget, 0, sizeof(EVETextWidget));
    eve_widget_init(_widget, EVE_WIDGET_TYPE_TEXT, g, eve_textw_touch, eve_textw_draw, eve_textw_putc);
    widget->font = font;
    widget->text = text;
    widget->text_size = text_size;
    widget->line = line;
    widget->line_size = line_size;
    if (text_size && line_size) eve_textw_update(widget, NULL, 0);
}

static EVETextCursor *cursor_prox(EVETextWidget *widget, EVETextCursor *cursor, EVEPage *page, EVETouch *t, short *dx, short *dl) {
    EVEWidget *_widget = &widget->w;
    int x = eve_page_x(page, t->x0) - _widget->g.x;
    int l = (int)t->tag0 - widget->tag0 + widget->line0;
    int _dx, _dl;

    *dx = cursor->x - x;
    *dl = cursor->line - l;

    _dx = *dx < 0 ? -(*dx) : *dx;
    _dl = *dl < 0 ? -(*dl) : *dl;

    if ((_dx <= widget->font->h) && (_dl <= 1)) return cursor;
    return NULL;
}

int eve_textw_touch(EVEWidget *_widget, EVEPage *page, uint8_t tag0, int touch_idx, EVERect *focus) {
    EVETextWidget *widget = (EVETextWidget *)_widget;
    EVETouch *t;
    uint16_t evt;
    int ret = 0;

    if (touch_idx > 0) return 0;

    t = eve_touch_evt(tag0, touch_idx, widget->tag0, widget->tagN - widget->tag0, &evt);
    if (t && evt) {
        EVETextCursor *t_cursor = NULL;
        short dx, dl;

        if (evt & (EVE_TOUCH_ETYPE_LPRESS | EVE_TOUCH_ETYPE_TRACK_START)) {
            if (widget->cursor2.on) {
                t_cursor = cursor_prox(widget, &widget->cursor2, page, t, &dx, &dl);
            }
            if ((t_cursor == NULL) && widget->cursor1.on) {
                t_cursor = cursor_prox(widget, &widget->cursor1, page, t, &dx, &dl);
            }
            if (evt & EVE_TOUCH_ETYPE_TRACK_START) {
                widget->track.cursor = t_cursor;
                if (t_cursor) {
                    widget->track.mode = TEXTW_TMODE_CURSOR;
                    widget->track.dx = dx;
                    widget->track.dl = dl;
                } else {
                    widget->track.mode = TEXTW_TMODE_SCROLL;
                }
            }
        }

        switch (widget->track.mode) {
            case TEXTW_TMODE_SCROLL:
                if (page && page->handle_evt) page->handle_evt(page, _widget, t, evt, tag0, touch_idx);
                break;

            case TEXTW_TMODE_CURSOR:
                if (evt & EVE_TOUCH_ETYPE_TRACK) eve_textw_cursor_set(widget, widget->track.cursor, t->tag + widget->track.dl, eve_page_x(page, t->x) + widget->track.dx);
                break;

            default:
                if (evt & EVE_TOUCH_ETYPE_LPRESS) {
                    if (widget->cursor2.on) {
                        // copy
                    } else if (widget->cursor1.on) {
                        if (t_cursor && (dl == 0)) {
                            // paste
                        } else {
                            eve_textw_cursor_set(widget, &widget->cursor2, t->tag, eve_page_x(page, t->x));
                        }
                    } else {
                        // select
                    }
                }
                if ((evt & EVE_TOUCH_ETYPE_POINT_UP) && !(t->eevt & EVE_TOUCH_EETYPE_LPRESS)) {
                    eve_textw_cursor_set(widget, &widget->cursor1, t->tag_up, eve_page_x(page, t->x));
                    if (widget->cursor2.on) eve_textw_cursor_clear(widget, &widget->cursor2);
                    widget->cursor_f = &widget->cursor1;
                }
                break;
        }

        if (evt & EVE_TOUCH_ETYPE_TRACK_STOP) {
            widget->track.mode = 0;
            widget->track.cursor = NULL;
            widget->track.dx = 0;
            widget->track.dl = 0;
        }

        ret = 1;
    }

    if (widget->cursor_f && focus) {
        focus->x = _widget->g.x;
        focus->y = _widget->g.y + widget->cursor_f->line * widget->font->h;
        focus->w = _widget->g.w;
        focus->h = 2 * widget->font->h;
        widget->cursor_f = NULL;
    }

    return ret;
}

static void _draw_line(EVETextWidget *widget, uint16_t l, uint16_t ch, uint16_t len, uint16_t x1, uint16_t x2, char s) {
    EVEWidget *_widget = &widget->w;

    if (x1 != x2) {
        eve_cmd_dl(BEGIN(EVE_RECTS));
        if (!s) eve_cmd_dl(COLOR_MASK(0 ,0 ,0 ,0));
        eve_cmd_dl(VERTEX2F(_widget->g.x + x1, _widget->g.y + l * widget->font->h));
        eve_cmd_dl(VERTEX2F(_widget->g.x + x2, _widget->g.y + (l + 1) * widget->font->h));
        if (!s) eve_cmd_dl(COLOR_MASK(1 ,1 ,1 ,1));
        eve_cmd_dl(END());
        if (len) {
            if (s) eve_cmd_dl(COLOR_RGB(0, 0, 0));
            eve_cmd(CMD_TEXT, "hhhhpb", _widget->g.x, _widget->g.y + l * widget->font->h, widget->font->id, 0, widget->text + ch, len, 0);
            if (s) eve_cmd_dl(COLOR_RGB(255, 255, 255));
        }
    }
}

static void _draw_cursor(EVETextWidget *widget, EVETextCursor *cursor) {
    uint16_t x, y;
    EVEWidget *_widget = &widget->w;

    x = _widget->g.x + cursor->x;
    y = _widget->g.y + cursor->line * widget->font->h;
    eve_cmd_dl(BEGIN(EVE_LINES));
    eve_cmd_dl(VERTEX2F(x, y));
    eve_cmd_dl(VERTEX2F(x, y + widget->font->h));
    eve_cmd_dl(END());
}

uint8_t eve_textw_draw(EVEWidget *_widget, EVEPage *page, uint8_t tag0) {
    int line0, lineN;
    char lineNvisible;
    EVETextWidget *widget = (EVETextWidget *)_widget;

    if (page) {
        int _line0, _lineN;

        _line0 = line0 = ((int)page->win_y - _widget->g.y) / widget->font->h;
        _lineN = lineN = ceil((double)((int)page->win_y - _widget->g.y + page->window->g.h) / widget->font->h);
        if (line0 < 0) line0 = 0;
        if (lineN < 0) lineN = 0;
        if (line0 > widget->line_len) line0 = widget->line_len;
        if (lineN > widget->line_len) lineN = widget->line_len;
        lineNvisible = (lineN >= _line0) && (lineN < _lineN);
    } else {
        line0 = 0;
        lineN = widget->line_len;
        lineNvisible = 1;
    }

    if (lineNvisible || (line0 < lineN)) {
        int i;
        char s = 0;
        EVETextCursor *c1, *c2;

        widget->line0 = line0;
        widget->tag0 = tag0;
        widget->tagN = tag0;

        if (widget->cursor2.on) {
            if (widget->cursor1.ch <= widget->cursor2.ch) {
                c1 = &widget->cursor1;
                c2 = &widget->cursor2;
            } else {
                c1 = &widget->cursor2;
                c2 = &widget->cursor1;
            }
        } else {
            c1 = NULL;
            c2 = NULL;
        }

        for (i=line0; i<lineN; i++) {
            if (widget->tagN != EVE_TAG_NOTAG) {
                eve_cmd_dl(TAG(widget->tagN));
                eve_touch_set_opt(widget->tagN, EVE_TOUCH_OPT_LPRESS);
                widget->tagN++;
            }
            if (!s && c1 && (c1->line == i)) {
                int l1, l2, l3;

                l1 = c1->ch - LINE_START(widget, i);
                if (c2->line == i) {
                    l2 = c2->ch - c1->ch;
                    l3 = LINE_START(widget, i) + LINE_LEN(widget, i) - c2->ch;
                } else {
                    l2 = LINE_START(widget, i) + LINE_LEN(widget, i) - c1->ch;
                    l3 = 0;
                    s = 1;
                }
                _draw_line(widget, i, LINE_START(widget, i), l1, 0, c1->x, 0);
                _draw_line(widget, i, c1->ch, l2, c1->x, s ? _widget->g.w : c2->x, 1);
                if (!s) {
                    _draw_line(widget, i, c2->ch, l3, c2->x, _widget->g.w, 0);
                    c1 = NULL;
                    c2 = NULL;
                }
            } else if (s && (c2->line == i)) {
                int l1 = c2->ch - LINE_START(widget, i);
                int l2 = LINE_START(widget, i) + LINE_LEN(widget, i) - c2->ch;

                _draw_line(widget, i, LINE_START(widget, i), l1, 0, c2->x, 1);
                _draw_line(widget, i, c2->ch, l2, c2->x, _widget->g.w, 0);
                c1 = NULL;
                c2 = NULL;
                s = 0;
            } else {
                if (widget->cursor1.on && (widget->cursor1.line == i)) _draw_cursor(widget, &widget->cursor1);
                _draw_line(widget, i, LINE_START(widget, i), LINE_LEN(widget, i), 0, _widget->g.w, s);
            }
        }
        if (lineNvisible) {
            if (widget->tagN != EVE_TAG_NOTAG) {
                eve_cmd_dl(TAG(widget->tagN));
                eve_touch_set_opt(widget->tagN, EVE_TOUCH_OPT_LPRESS);
                widget->tagN++;
            }
            _draw_line(widget, lineN, 0, 0, 0, _widget->g.w, 0);
        }

        return widget->tagN;
    } else {
        widget->line0 = 0;
        widget->tag0 = EVE_TAG_NOTAG;
        widget->tagN = EVE_TAG_NOTAG;

        return tag0;
    }
}

void eve_textw_putc(void *_page, int c) {
    EVEPage *page = _page;
    EVETextWidget *widget = (EVETextWidget *)eve_page_get_focus(page);
    EVETextCursor *cursor1 = &widget->cursor1;
    EVETextCursor *cursor2 = &widget->cursor2;
    char *text;
    int i, r, ins_c = 0, del_c = 0;
    int ch_w = 0;

    if (c == CH_EOF) {
        if (cursor1->on) eve_textw_cursor_clear(widget, cursor1);
        if (cursor2->on) eve_textw_cursor_clear(widget, cursor2);
        return;
    }

    if (cursor2->on) {
        EVETextCursor *c1;
        EVETextCursor *c2;
        char *clipb = NULL;

        if (cursor1->ch <= cursor2->ch) {
            c1 = cursor1;
            c2 = cursor2;
        } else {
            c1 = cursor2;
            c2 = cursor1;
        }

        text = widget->text + c1->ch;
        del_c = c2->ch - c1->ch;
        if ((c == CH_CTRLX) || (c == CH_CTRLC)) {
            // eve_clipb_push(text, del_c);
            if (c == CH_CTRLC) return;
        }
        if (CHAR_VALID_INPUT(c) && (widget->text_len < widget->text_size + del_c - 1)) {
            ins_c = 1;
            ch_w = widget->font->w_ch[c];
        } else if (c == CH_CTRLV) {
            // clipb = eve_clipb_pop();
            ins_c = clipb ? strlen(clipb) : 0;
            if (ins_c) {
                if (widget->text_len >= widget->text_size - (ins_c - del_c)) ins_c = widget->text_size - widget->text_len + del_c - 1;
                ch_w = eve_font_bufw(widget->font, clipb, ins_c);
            }
        }
        if (ins_c != del_c) memmove(text + ins_c, text + del_c, widget->text_len - c2->ch + 1);
        if (ins_c) {
            if (c == CH_CTRLV) {
                memcpy(text, clipb, ins_c);
            } else {
                *text = c;
            }
            c1->ch += ins_c;
        }
        if (c1 == cursor2) widget->cursor1 = widget->cursor2;
        eve_textw_cursor_clear(widget, cursor2);
    } else if (cursor1->on) {
        text = widget->text + cursor1->ch;

        switch (c) {
            case CH_BS:
                if (cursor1->ch > 0) {
                    ch_w = -widget->font->w_ch[*(text - 1)];
                    memmove(text - 1, text, widget->text_len - cursor1->ch + 1);
                    cursor1->ch--;
                    del_c = 1;
                }
                break;

            case CH_DEL:
                if (cursor1->ch < widget->text_len) {
                    memmove(text, text + 1, widget->text_len - cursor1->ch);
                    del_c = 1;
                }
                break;

            default:
                if (CHAR_VALID_INPUT(c) && (widget->text_len < widget->text_size - 1)) {
                    ch_w = widget->font->w_ch[c];
                    memmove(text + 1, text, widget->text_len - cursor1->ch + 1);
                    *text = c;
                    cursor1->ch++;
                    ins_c = 1;
                }
                break;
        }
    }

    if ((ins_c == 0) && (del_c == 0)) return;

    widget->text_len += ins_c - del_c;
    for (i=cursor1->line; i<widget->line_len; i++) {
        widget->line[i] += ins_c - del_c;
    }

    r = cursor1->line;
    if (cursor1->line) r = eve_textw_update(widget, page, cursor1->line - 1);
    if ((cursor1->line == 0) || (r == cursor1->line - 1)) r = eve_textw_update(widget, page, cursor1->line);
    if (r < 0) return;

    if (cursor1->line && (cursor1->ch < LINE_START(widget, cursor1->line))) {
        cursor1->line--;
        widget->cursor_f = cursor1;
        eve_textw_cursor_update(widget, cursor1);
    } else if (cursor1->ch > LINE_END(widget, cursor1->line)) {
        while (cursor1->ch > LINE_END(widget, cursor1->line)) cursor1->line++;
        widget->cursor_f = cursor1;
        eve_textw_cursor_update(widget, cursor1);
    } else {
        cursor1->x += ch_w;
    }
}

int eve_textw_update(EVETextWidget *widget, EVEPage *page, uint16_t line) {
    int i;
    char ch;
    uint8_t ch_w;
    uint16_t word_w, line_w, line_b;
    uint16_t new_h;
    EVEWidget *_widget = &widget->w;

    word_w = 0;
    line_w = 0;
    line_b = LINE_EMPTY;
    for (i=LINE_START(widget, line); i<widget->text_size; i++) {
        ch = widget->text[i];
        if (ch < 128) ch_w = widget->font->w_ch[ch];
        if (ch <= 0x20) {
            if ((ch == '\n') || (ch == '\0')) {
                if (widget->line[line] == i) return line;
                widget->line[line] = i;
                line++;
                if ((ch == '\0') || (line == widget->line_size)) break;
                word_w = 0;
                line_w = 0;
                line_b = LINE_EMPTY;
            } else if (ch == ' ') {
                word_w = 0;
                line_w += ch_w;
                line_b = i;
            } else {
                return EVE_ERR_TEXT;
            }
        } else if (ch < 0x7f) {
            word_w += ch_w;
            line_w += ch_w;
        } else {
            return EVE_ERR_TEXT;
        }
        if ((line_w > _widget->g.w) && (line_b != LINE_EMPTY)) {
            if (widget->line[line] == line_b) return line;
            widget->line[line] = line_b;
            line++;
            if (line == widget->line_size) {
                i = line_b;
                break;
            }
            line_w = word_w;
            line_b = LINE_EMPTY;
        }
    }

    if (i == widget->text_size) i--;
    widget->text[i] = '\0';
    widget->text_len = i;

    widget->line_len = line;
    new_h = (line + 1) * widget->font->h;
    for (i=line; i<widget->line_size; i++) {
        widget->line[i] = LINE_EMPTY;
    }

    if (_widget->g.h != new_h) {
        _widget->g.h = new_h;
        if (page && page->update_g) page->update_g(page, _widget);
    }

    return line;
}

void eve_textw_cursor_update(EVETextWidget *widget, EVETextCursor *cursor) {
    int i;
    uint16_t x;

    x = 0;
    for (i=LINE_START(widget, cursor->line); i<LINE_END(widget, cursor->line); i++) {
        if (cursor->ch == i) break;
        x += widget->font->w_ch[widget->text[i]];
    }
    cursor->x = x;
}

void eve_textw_cursor_set(EVETextWidget *widget, EVETextCursor *cursor, uint8_t tag, int16_t x) {
    int i;
    uint16_t _x, _d;
    uint16_t c_line = LINE_EMPTY;
    EVEWidget *_widget = &widget->w;

    if ((tag >= widget->tag0) && ((widget->tagN == EVE_TAG_NOTAG) || (tag < widget->tagN))) c_line = tag - widget->tag0 + widget->line0;
    if (c_line < widget->line_len) {
        cursor->line = c_line;
    } else if (c_line == widget->line_len) {
        cursor->line = c_line - 1;
    } else if (!cursor->on) {
        return;
    }

    x -= _widget->g.x;
    _x = 0;
    _d = x;
    for (i=LINE_START(widget, cursor->line); i<LINE_END(widget, cursor->line); i++) {
        _x += widget->font->w_ch[widget->text[i]];
        if (_x >= x) {
            if (_x - x < _d) {
                i++;
            } else {
                _x -= widget->font->w_ch[widget->text[i]];
            }
            break;
        } else {
            _d = x - _x;
        }
    }
    cursor->x = _x;
    cursor->ch = i;
    cursor->on = 1;
}

void eve_textw_cursor_clear(EVETextWidget *widget, EVETextCursor *cursor) {
    cursor->on = 0;
}
