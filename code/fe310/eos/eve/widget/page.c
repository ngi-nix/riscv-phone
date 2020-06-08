#include <stdlib.h>
#include <string.h>

#include "eve.h"
#include "eve_kbd.h"

#include "screen/screen.h"
#include "screen/window.h"
#include "screen/page.h"

#include "widget.h"
#include "page.h"

void eve_pagew_init(EVEPageWidget *widget, EVERect *g, uint8_t font_id, char *title, EVEPage *page) {
    memset(widget, 0, sizeof(EVEPageWidget));
    eve_widget_init(&widget->w, EVE_WIDGET_TYPE_PAGE, g, eve_pagew_touch, eve_pagew_draw, NULL);
    widget->font_id = font_id;
    widget->title = title;
    widget->page = page;
}

int eve_pagew_touch(EVEWidget *_widget, EVEPage *page, uint8_t tag0, int touch_idx, EVERect *focus) {
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
            if (focus) *focus = _widget->g;
        }
        ret = 1;
    }
    return ret;
}

uint8_t eve_pagew_draw(EVEWidget *_widget, EVEPage *page, uint8_t tag0) {
    EVEPageWidget *widget = (EVEPageWidget *)_widget;
    char draw = page ? eve_page_widget_visible(page, _widget) : 1;

    widget->tag = tag0;
    if (draw) {
        if (tag0 != EVE_TAG_NOTAG) {
            eve_cmd_dl(TAG(tag0));
            tag0++;
        }
        eve_cmd(CMD_TEXT, "hhhhs", _widget->g.x, _widget->g.y, widget->font_id, 0, widget->title);
    }

    return tag0;
}
