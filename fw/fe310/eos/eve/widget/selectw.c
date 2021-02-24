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

int eve_selectw_create(EVESelectWidget *widget, EVERect *g, EVESelectSpec *spec) {
    utf8_t *option;

    option = eve_malloc(spec->option_size);
    if (option == NULL) {
        return EVE_ERR_NOMEM;
    }
    memset(option, 0, spec->option_size);

    eve_selectw_init(widget, g, spec->font, option, spec->option_size);

    return EVE_OK;
}

void eve_selectw_destroy(EVESelectWidget *widget) {
    eve_free(widget->option);
}

void eve_selectw_init(EVESelectWidget *widget, EVERect *g, EVEFont *font, utf8_t *option, uint16_t option_size) {
    EVEWidget *_widget = &widget->w;

    memset(widget, 0, sizeof(EVESelectWidget));
    eve_widget_init(_widget, EVE_WIDGET_TYPE_SELECT, g, eve_selectw_touch, eve_selectw_draw, NULL);
    eve_selectw_update(widget, font, option, option_size);
}

void eve_selectw_update(EVESelectWidget *widget, EVEFont *font, utf8_t *option, uint16_t option_size) {
    int rv, text_len;

    if (font) widget->font = font;
    if (option) {
        widget->option = option;
        widget->option_size = option_size;
        widget->select = SELECTW_NOSELECT;
    }
}

int eve_selectw_touch(EVEWidget *_widget, EVEPage *page, EVETouch *t, uint16_t evt) {
    EVESelectWidget *widget = (EVESelectWidget *)_widget;

    if (evt & EVE_TOUCH_ETYPE_TAG_UP) {
        if (widget->select == t->tag0 - _widget->tag0) {
            widget->select = SELECTW_NOSELECT;
        } else {
            widget->select = t->tag0 - _widget->tag0;
        }
    }
    return 0;
}

uint8_t eve_selectw_draw(EVEWidget *_widget, EVEPage *page, uint8_t tag0) {
    EVESelectWidget *widget = (EVESelectWidget *)_widget;
    uint16_t o_len;
    uint16_t o_curr;
    int i = 0, s;
    int16_t x1, x2, y1, y2;
    uint16_t new_h;

    _widget->tag0 = tag0;
    _widget->tagN = tag0;
    o_curr = 0;
    do {
        o_len = strnlen(widget->option + o_curr, widget->option_size - o_curr);
        if (o_curr + o_len + 1 >= widget->option_size) break;
        if (o_len) {
            if (_widget->tagN != EVE_TAG_NOTAG) {
                eve_cmd_dl(TAG(_widget->tagN));
                _widget->tagN++;
            }
            s = (widget->select != SELECTW_NOSELECT) && (widget->select == i) ? 1 : 0;
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
        }
    } while (o_len);

    _widget->g.h = i * widget->font->h;

    return _widget->tagN;
}

utf8_t *eve_selectw_option_get(EVESelectWidget *widget, int idx) {
    uint16_t o_len;
    uint16_t o_curr;
    int i = 0;

    o_curr = 0;
    do {
        if (i == idx) return widget->option + o_curr;
        o_len = strnlen(widget->option + o_curr, widget->option_size - o_curr);
        if (o_curr + o_len + 1 >= widget->option_size) return NULL;
        if (o_len) {
            o_curr += o_len + 1;
            i++;
        }
    } while (o_len);
    return NULL;
}

utf8_t *eve_selectw_option_get_select(EVESelectWidget *widget) {
    return eve_selectw_option_get(widget, widget->select);
}

int eve_selectw_option_add(EVESelectWidget *widget, utf8_t *opt) {
    uint16_t o_len;
    uint16_t o_curr;

    o_curr = 0;
    do {
        o_len = strnlen(widget->option + o_curr, widget->option_size - o_curr);
        if (o_curr + o_len + 1 >= widget->option_size) return EVE_ERR_FULL;
        if (o_len) {
            o_curr += o_len + 1;
        }
    } while (o_len);
    if (o_curr + strlen(opt) + 1 > widget->option_size) return EVE_ERR_FULL;
    strcpy(widget->option + o_curr, opt);

    return EVE_OK;
}

int eve_selectw_option_set(EVESelectWidget *widget, utf8_t *opt, uint16_t size) {
    if (size > widget->option_size) return EVE_ERR_FULL;
    memcpy(widget->option, opt, size);
    memset(widget->option + size, 0, widget->option_size - size);

    return EVE_OK;
}
