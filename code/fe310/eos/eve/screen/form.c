#include <stdlib.h>
#include <string.h>

#include "eve.h"
#include "eve_kbd.h"
#include "unicode.h"

#include "screen.h"
#include "window.h"
#include "page.h"
#include "font.h"
#include "form.h"

#include "widget/label.h"
#include "widget/widget.h"

int eve_form_init(EVEForm *form, EVEWindow *window, EVEWidget *widget, uint16_t widget_size, eve_page_open_t open, eve_page_close_t close) {
    memset(form, 0, sizeof(EVEForm));
    eve_page_init(&form->p, window, eve_form_touch, eve_form_draw, open, close, eve_form_handle_evt, eve_form_update_g);
    form->widget = widget;
    form->widget_size = widget_size;
    eve_form_update_g(&form->p, NULL);
}

int eve_form_touch(EVEView *v, uint8_t tag0, int touch_idx) {
    EVEForm *form = (EVEForm *)v;
    EVEWidget *widget = form->widget;
    int i, ret = 0;

    if (touch_idx == 0) {
        EVETouch *t;
        uint16_t evt;

        t = eve_touch_evt(tag0, touch_idx, form->p.v.window->tag, 1, &evt);
        if (t && evt) {
            eve_form_handle_evt(&form->p, NULL, t, evt, tag0, touch_idx);
            if ((evt & EVE_TOUCH_ETYPE_POINT_UP) && (t->eevt == 0)) eve_page_set_focus(&form->p, NULL, NULL);
            ret = 1;
        }
    }
    for (i=0; i<form->widget_size; i++) {
        if (eve_page_rect_visible(&form->p, &widget->g)) {
            int r = widget->touch(widget, &form->p, tag0, touch_idx);
            ret = ret || r;
        }
        widget = eve_widget_next(widget);
    }

    return ret;
}

uint8_t eve_form_draw(EVEView *v, uint8_t tag0) {
    EVEForm *form = (EVEForm *)v;
    EVEWidget *widget = form->widget;
    int i;
    uint8_t tagN = tag0;

    eve_cmd_dl(SAVE_CONTEXT());
    eve_cmd_dl(VERTEX_FORMAT(0));
    eve_cmd_dl(VERTEX_TRANSLATE_X(eve_page_scr_x(&form->p, 0) * 16));
    eve_cmd_dl(VERTEX_TRANSLATE_Y(eve_page_scr_y(&form->p, 0) * 16));

    for (i=0; i<form->widget_size; i++) {
        if (widget->label && eve_page_rect_visible(&form->p, &widget->label->g)) {
            eve_cmd_dl(TAG_MASK(0));
            eve_label_draw(widget->label);
            eve_cmd_dl(TAG_MASK(1));
        }
        if (eve_page_rect_visible(&form->p, &widget->g)) tagN = widget->draw(widget, &form->p, tagN);
        widget = eve_widget_next(widget);
    }

    eve_cmd_dl(RESTORE_CONTEXT());

    for (i=tag0; i<tagN; i++) {
        eve_touch_set_opt(i, eve_touch_get_opt(i) | EVE_TOUCH_OPT_TRACK | EVE_TOUCH_OPT_TRACK_XY | EVE_TOUCH_OPT_TRACK_EXT);
    }
    if (v->window->tag != EVE_TAG_NOTAG) eve_touch_set_opt(v->window->tag, eve_touch_get_opt(v->window->tag) | EVE_TOUCH_OPT_TRACK | EVE_TOUCH_OPT_TRACK_XY | EVE_TOUCH_OPT_TRACK_EXT);

    return tagN;
}

void eve_form_handle_evt(EVEPage *page, EVEWidget *widget, EVETouch *touch, uint16_t evt, uint8_t tag0, int touch_idx) {
    /*
    if (evt & EVE_TOUCH_ETYPE_TRACK_Y) {
        // do scroll
    } else {
        // go back / forward
    }
    */
}

void eve_form_update_g(EVEPage *page, EVEWidget *_widget) {
    EVEForm *form = (EVEForm *)page;
    EVEWidget *widget = form->widget;
    int i;
    uint16_t h = 0;

    for (i=0; i<form->widget_size; i++) {
        if (widget->label) {
            widget->label->g.y = h;
            if (widget->label->g.w + widget->g.w > form->p.v.window->g.w) h += widget->label->g.h;
        }
        widget->g.y = h;
        h += widget->g.h;

        widget = eve_widget_next(widget);
    }
}
