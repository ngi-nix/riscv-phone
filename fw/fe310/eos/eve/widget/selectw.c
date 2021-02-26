#include <stdlib.h>
#include <string.h>

#include "eve.h"
#include "eve_kbd.h"
#include "unicode.h"

#include "screen/screen.h"
#include "screen/window.h"
#include "screen/view.h"
#include "screen/page.h"

#include "font.h"
#include "label.h"
#include "widget.h"
#include "selectw.h"

#define SELECTW_NOSELECT    0xffffffff

static int selectw_option_verify(utf8_t *opt, uint16_t size) {
    int o_len;
    uint16_t o_curr;
    int rv;

    o_curr = 0;
    while (o_curr < size) {
        rv = utf8_verify(opt + o_curr, size - o_curr, &o_len);
        if (rv) return EVE_ERR;
        if (o_len == 0) return EVE_OK;
        o_curr += o_len + 1;
    }

    return EVE_OK;
}

int eve_selectw_create(EVESelectWidget *widget, EVERect *g, EVESelectSpec *spec) {
    utf8_t *option;

    option = eve_malloc(spec->option_size);
    if (option == NULL) {
        return EVE_ERR_NOMEM;
    }
    memset(option, 0, spec->option_size);

    eve_selectw_init(widget, g, spec->font, option, spec->option_size, spec->multi);

    return EVE_OK;
}

void eve_selectw_destroy(EVESelectWidget *widget) {
    eve_free(widget->option);
}

void eve_selectw_init(EVESelectWidget *widget, EVERect *g, EVEFont *font, utf8_t *option, uint16_t option_size, uint8_t multi) {
    EVEWidget *_widget = &widget->w;

    memset(widget, 0, sizeof(EVESelectWidget));
    eve_widget_init(_widget, EVE_WIDGET_TYPE_SELECT, g, eve_selectw_touch, eve_selectw_draw, NULL);
    eve_selectw_update(widget, font, option, option_size);
    widget->multi = multi;
}

void eve_selectw_update(EVESelectWidget *widget, EVEFont *font, utf8_t *option, uint16_t option_size) {
    int rv, text_len;

    if (font) widget->font = font;
    if (option) {
        int rv = selectw_option_verify(option, option_size);
        if (rv == EVE_OK) {
            widget->option = option;
            widget->option_size = option_size;
            widget->select = widget->multi ? 0 : SELECTW_NOSELECT;
        }
    }
}

int eve_selectw_touch(EVEWidget *_widget, EVEPage *page, EVETouch *t, uint16_t evt) {
    EVESelectWidget *widget = (EVESelectWidget *)_widget;

    if (evt & EVE_TOUCH_ETYPE_TAG_UP) {
        int i = t->tag0 - _widget->tag0;
        if (widget->multi) {
            uint32_t f = (0x1 << i);

            if (widget->select & f) {
                widget->select &= ~f;
            } else {
                widget->select |= f;
            }
        } else {
            if (widget->select == i) {
                widget->select = SELECTW_NOSELECT;
            } else {
                widget->select = i;
            }
        }
        return 1;
    }
    return 0;
}

uint8_t eve_selectw_draw(EVEWidget *_widget, EVEPage *page, uint8_t tag0) {
    EVESelectWidget *widget = (EVESelectWidget *)_widget;
    int o_len;
    int o_curr;
    int i = 0, s;
    int16_t x1, x2, y1, y2;
    uint16_t new_h;

    _widget->tag0 = tag0;
    _widget->tagN = tag0;
    o_curr = 0;
    do {
        o_len = strnlen(widget->option + o_curr, widget->option_size - o_curr);
        if (!o_len || (o_len == widget->option_size - o_curr)) break;

        if (_widget->tagN != EVE_TAG_NOTAG) {
            eve_cmd_dl(TAG(_widget->tagN));
            _widget->tagN++;
        }
        s = widget->multi ? widget->select & (0x1 << i) : widget->select == i;
        x1 = _widget->g.x;
        x2 = x1 + _widget->g.w;
        y1 = _widget->g.y + i * widget->font->h;
        y2 = y1 + widget->font->h;
        eve_cmd_dl(BEGIN(EVE_RECTS));
        if (!s) eve_cmd_dl(COLOR_MASK(0 ,0 ,0 ,0));
        eve_cmd_dl(VERTEX2F(x1, y1));
        eve_cmd_dl(VERTEX2F(x2, y2));
        if (!s) {
            eve_cmd_dl(COLOR_MASK(1 ,1 ,1 ,1));
            eve_cmd_dl(BEGIN(EVE_LINES));
            eve_cmd_dl(VERTEX2F(x1, y2));
            eve_cmd_dl(VERTEX2F(x2, y2));
        }
        eve_cmd_dl(END());
        if (s) eve_cmd_dl(COLOR_RGBC(page->v.window->color_bg));
        eve_cmd(CMD_TEXT, "hhhhs", x1, y1, widget->font->id, 0, widget->option + o_curr);
        if (s) eve_cmd_dl(COLOR_RGBC(page->v.window->color_fg));

        o_curr += o_len + 1;
        i++;
    } while (o_len);

    _widget->g.h = i * widget->font->h;

    return _widget->tagN;
}

utf8_t *eve_selectw_option_get(EVESelectWidget *widget, int idx) {
    int o_len;
    int o_curr;
    int i = 0;

    o_curr = 0;
    do {
        o_len = strnlen(widget->option + o_curr, widget->option_size - o_curr);
        if (o_len == widget->option_size - o_curr) return NULL;
        if (o_len && (i == idx)) return widget->option + o_curr;

        o_curr += o_len + 1;
        i++;
    } while (o_len);

    return NULL;
}

utf8_t *eve_selectw_option_get_select(EVESelectWidget *widget) {
    return eve_selectw_option_get(widget, widget->select);
}

int eve_selectw_option_add(EVESelectWidget *widget, utf8_t *opt) {
    int o_len;
    int o_curr;
    int rv;

    rv = utf8_verify(opt, strlen(opt) + 1, NULL);
    if (rv) return EVE_ERR;

    o_curr = 0;
    do {
        o_len = strnlen(widget->option + o_curr, widget->option_size - o_curr);
        if (o_len == widget->option_size - o_curr) return EVE_ERR_FULL;
        if (o_len) o_curr += o_len + 1;
    } while (o_len);

    if (o_curr + strlen(opt) + 1 > widget->option_size) return EVE_ERR_FULL;
    strcpy(widget->option + o_curr, opt);

    return EVE_OK;
}

int eve_selectw_option_set(EVESelectWidget *widget, utf8_t *opt, uint16_t size) {
    int rv;

    rv = selectw_option_verify(opt, size);
    if (rv) return rv;
    if (size > widget->option_size) return EVE_ERR_FULL;

    memcpy(widget->option, opt, size);
    memset(widget->option + size, 0, widget->option_size - size);

    return EVE_OK;
}
