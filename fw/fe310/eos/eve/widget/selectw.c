#include <stdlib.h>
#include <string.h>

#include "eve.h"
#include "eve_kbd.h"
#include "eve_font.h"

#include "screen/window.h"
#include "screen/page.h"

#include "label.h"
#include "widget.h"
#include "selectw.h"

#define SELECTW_NOSELECT        0xffffffff

#define DIVC(x,y)               ((x) / (y) + ((x) % (y) != 0))

static int selectw_verify(utf8_t *option, uint16_t option_size) {
    int o_len;
    uint16_t o_curr;
    int rv;

    o_curr = 0;
    while (o_curr < option_size) {
        rv = utf8_verify(option + o_curr, option_size - o_curr, &o_len);
        if (rv) return EVE_ERR;
        if (o_len == 0) return EVE_OK;
        o_curr += o_len + 1;
    }

    return EVE_OK;
}

static int selectw_count(EVESelectWidget *widget) {
    int o_len;
    int o_curr;
    int i;

    o_curr = 0;
    i = 0;
    do {
        o_len = strnlen(widget->option + o_curr, widget->option_size - o_curr);
        if (o_len == widget->option_size - o_curr) return i;
        if (o_len) {
            o_curr += o_len + 1;
            i++;
        }
    } while (o_len);

    return i;
}

static void selectw_update_sz(EVESelectWidget *widget, int uievt) {
    EVEWidget *_widget = &widget->w;
    EVEPage *page = _widget->page;

    _widget->g.h = widget->option_count * widget->font->h;
    if (uievt) eve_view_uievt_push(&page->v, EVE_UIEVT_WIDGET_UPDATE_G, _widget);
}

int eve_selectw_create(EVESelectWidget *widget, EVERect *g, EVEPage *page, EVESelectSpec *spec) {
    EVEFont *font = spec->font ? spec->font : eve_window_font(page->v.window);
    utf8_t *option;

    option = eve_malloc(spec->option_size);
    if (option == NULL) {
        return EVE_ERR_NOMEM;
    }
    memset(option, 0, spec->option_size);

    eve_selectw_init(widget, g, page, font, option, spec->option_size, spec->multi);

    return EVE_OK;
}

void eve_selectw_init(EVESelectWidget *widget, EVERect *g, EVEPage *page, EVEFont *font, utf8_t *option, uint16_t option_size, uint8_t multi) {
    EVEWidget *_widget = &widget->w;
    int rv;

    memset(widget, 0, sizeof(EVESelectWidget));
    eve_widget_init(_widget, EVE_WIDGET_TYPE_SELECT, g, page, eve_selectw_draw, eve_selectw_touch, NULL);
    widget->font = font;
    rv = selectw_verify(option, option_size);
    if (rv == EVE_OK) {
        widget->option = option;
        widget->option_size = option_size;
        widget->option_count = selectw_count(widget);
        selectw_update_sz(widget, 0);
    }
    widget->multi = multi;
    widget->select = widget->multi ? 0 : SELECTW_NOSELECT;
}

void eve_selectw_destroy(EVESelectWidget *widget) {
    eve_free(widget->option);
}

uint8_t eve_selectw_draw(EVEWidget *_widget, uint8_t tag0) {
    EVEPage *page = _widget->page;
    EVESelectWidget *widget = (EVESelectWidget *)_widget;
    int o_len;
    int o_curr;
    int i, s;
    int16_t x1, x2, y1, y2;
    uint16_t new_h;

    int line0, lineN;

    line0 = -((int)eve_page_win_y(page, _widget->g.y)) / widget->font->h;
    lineN = DIVC((-((int)eve_page_win_y(page, _widget->g.y)) + page->v.window->g.h), widget->font->h);
    if (line0 < 0) line0 = 0;
    if (lineN < 0) lineN = 0;
    if (line0 > widget->option_count) line0 = widget->option_count;
    if (lineN > widget->option_count) lineN = widget->option_count;

    _widget->tag0 = tag0;
    _widget->tagN = tag0;
    widget->line0 = line0;

    o_curr = 0;
    i = 0;
    do {
        o_len = strnlen(widget->option + o_curr, widget->option_size - o_curr);
        if (!o_len || (o_len == widget->option_size - o_curr)) break;
        if ((i >= line0) && (i < lineN)) {
            if (_widget->tagN != EVE_NOTAG) {
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
            if (s) eve_cmd_dl(COLOR_RGBC(page->v.color_bg));
            eve_cmd(CMD_TEXT, "hhhhs", x1, y1, widget->font->id, 0, widget->option + o_curr);
            if (s) eve_cmd_dl(COLOR_RGBC(page->v.color_fg));
        }

        o_curr += o_len + 1;
        i++;
    } while (o_len);

    return _widget->tagN;
}

int eve_selectw_touch(EVEWidget *_widget, EVETouch *touch, uint16_t evt) {
    EVESelectWidget *widget = (EVESelectWidget *)_widget;

    if (evt & EVE_TOUCH_ETYPE_TAG_UP) {
        int i = touch->tag0 - _widget->tag0 + widget->line0;
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

utf8_t *eve_selectw_option_get(EVESelectWidget *widget, int idx) {
    int o_len;
    int o_curr;
    int i;

    o_curr = 0;
    i = 0;
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

int eve_selectw_option_add(EVESelectWidget *widget, utf8_t *option) {
    int o_len;
    int o_curr;
    int rv, i;

    rv = utf8_verify(option, strlen(option) + 1, NULL);
    if (rv) return EVE_ERR;

    o_curr = 0;
    i = 0;
    do {
        o_len = strnlen(widget->option + o_curr, widget->option_size - o_curr);
        if (o_len == widget->option_size - o_curr) return EVE_ERR_FULL;
        if (o_len) {
            o_curr += o_len + 1;
            i++;
        }
    } while (o_len);

    if (o_curr + strlen(option) + 1 > widget->option_size) return EVE_ERR_FULL;
    strcpy(widget->option + o_curr, option);

    widget->option_count = i + 1;
    selectw_update_sz(widget, 1);

    return EVE_OK;
}

int eve_selectw_option_set(EVESelectWidget *widget, utf8_t *option, uint16_t option_size) {
    int rv, i;

    rv = selectw_verify(option, option_size);
    if (rv) return rv;
    if (option_size > widget->option_size) return EVE_ERR_FULL;

    memcpy(widget->option, option, option_size);
    memset(widget->option + option_size, 0, widget->option_size - option_size);

    widget->option_count = selectw_count(widget);
    selectw_update_sz(widget, 1);

    return EVE_OK;
}
