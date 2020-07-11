#include <stdlib.h>
#include <string.h>

#include "eve.h"
#include "eve_kbd.h"

#include "screen/screen.h"
#include "screen/window.h"
#include "screen/page.h"
#include "screen/font.h"

#include "label.h"
#include "widget.h"
#include "pagew.h"

void eve_pagew_init(EVEPageWidget *widget, EVERect *g, char *title, EVEFont *font, EVEPage *page) {
    EVEWidget *_widget = &widget->w;

    memset(widget, 0, sizeof(EVEPageWidget));
    eve_widget_init(_widget, EVE_WIDGET_TYPE_PAGE, g, eve_pagew_touch, eve_pagew_draw, NULL);
    widget->title = title;
    widget->font = font;
    widget->page = page;
    if (_widget->g.w == 0) _widget->g.w = eve_font_str_w(font, widget->title);
    if (_widget->g.h == 0) _widget->g.h = eve_font_h(font);
}

int eve_pagew_touch(EVEWidget *_widget, EVEPage *page, uint8_t tag0, int touch_idx) {
    EVEPageWidget *widget = (EVEPageWidget *)_widget;
    EVETouch *t;
    uint16_t evt;
    int ret = 0;

    if (touch_idx > 0) return 0;

    t = eve_touch_evt(tag0, touch_idx, widget->tag, 1, &evt);
    if (t && evt) {
        if (evt & EVE_TOUCH_ETYPE_TRACK_MASK) {
            if (page && page->handle_evt) page->handle_evt(page, _widget, t, evt, tag0, touch_idx);
        } else if (evt & EVE_TOUCH_ETYPE_TAG_UP) {
            widget->page->open(widget->page, page);
        }
        ret = 1;
    }
    return ret;
}

uint8_t eve_pagew_draw(EVEWidget *_widget, EVEPage *page, uint8_t tag0) {
    EVEPageWidget *widget = (EVEPageWidget *)_widget;

    widget->tag = tag0;
    if (tag0 != EVE_TAG_NOTAG) {
        eve_cmd_dl(TAG(tag0));
        tag0++;
    }
    eve_cmd(CMD_TEXT, "hhhhs", _widget->g.x, _widget->g.y, widget->font->id, 0, widget->title);

    return tag0;
}
