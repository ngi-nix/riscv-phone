#include <stdlib.h>
#include <string.h>

#include "eve.h"
#include "eve_kbd.h"
#include "unicode.h"

#include "screen.h"
#include "window.h"
#include "view.h"
#include "page.h"
#include "form.h"

#include "widget/font.h"
#include "widget/label.h"
#include "widget/widget.h"

#define MIN(X, Y)               (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y)               (((X) > (Y)) ? (X) : (Y))

int eve_form_init(EVEForm *form, EVEWindow *window, EVEViewStack *stack, EVEWidget *widget, uint16_t widget_size, eve_form_action_t action, eve_form_destructor_t destructor) {
    memset(form, 0, sizeof(EVEForm));
    eve_page_init(&form->p, window, stack, eve_form_touch, eve_form_draw, (eve_page_destructor_t)destructor);

    form->widget = widget;
    form->widget_size = widget_size;
    form->action = action;
    eve_form_update_g(form, NULL);
}

int eve_form_touch(EVEView *v, uint8_t tag0, int touch_idx) {
    EVEForm *form = (EVEForm *)v;
    EVEWidget *widget = form->widget;
    EVETouch *t;
    uint16_t evt;
    int i, ret = 0;

    if (touch_idx > 0) return 0;

    t = eve_touch_evt(tag0, touch_idx, form->p.v.window->tag, 1, &evt);
    if (t && evt) {
        ret = eve_form_handle_evt(form, NULL, t, evt);
        if ((evt & EVE_TOUCH_ETYPE_POINT_UP) && (t->eevt == 0)) eve_page_set_focus(&form->p, NULL, NULL);
        if (ret) return 1;
    }
    for (i=0; i<form->widget_size; i++) {
        if (eve_page_rect_visible(&form->p, &widget->g)) {
            t = eve_touch_evt(tag0, touch_idx, widget->tag0, widget->tagN - widget->tag0, &evt);
            if (t && evt) {
                ret = widget->touch(widget, &form->p, t, evt);
                if (ret) return 1;
                ret = eve_form_handle_evt(form, widget, t, evt);
                if (ret) return 1;
            }
        }
        widget = eve_widget_next(widget);
    }

    return 0;
}

uint8_t eve_form_draw(EVEView *v, uint8_t tag0) {
    EVEForm *form = (EVEForm *)v;
    EVEWidget *widget = form->widget;
    int i;
    uint8_t tagN = tag0;
    uint8_t tag_opt = EVE_TOUCH_OPT_TRACK | EVE_TOUCH_OPT_TRACK_XY | EVE_TOUCH_OPT_TRACK_EXT_Y;

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
        if (eve_page_rect_visible(&form->p, &widget->g)) {
            uint16_t h = widget->g.h;
            tagN = widget->draw(widget, &form->p, tagN);
            if (h != widget->g.h) eve_form_update_g(form, widget);
        }
        widget = eve_widget_next(widget);
    }

    eve_cmd_dl(RESTORE_CONTEXT());

    for (i=tag0; i<tagN; i++) {
        eve_touch_set_opt(i, eve_touch_get_opt(i) | tag_opt);
    }
    if (v->window->tag != EVE_TAG_NOTAG) eve_touch_set_opt(v->window->tag, eve_touch_get_opt(v->window->tag) | tag_opt);

    return tagN;
}

void eve_form_update_g(EVEForm *form, EVEWidget *_widget) {
    EVEWidget *widget = form->widget;
    int i;
    uint16_t w = 0;
    uint16_t h = 0;
    uint16_t _h = 0;

    for (i=0; i<form->widget_size; i++) {
        if (widget->label) {
            h += _h;
            w = widget->label->g.w;
            _h = widget->label->g.h;
            widget->label->g.x = 0;
            widget->label->g.y = h;
        }
        if (w + widget->g.w > form->p.v.window->g.w) {
            h += _h;
            w = 0;
            _h = 0;
        }
        widget->g.x = w;
        widget->g.y = h;

        w += widget->g.w;
        _h = MAX(_h, widget->g.h);

        widget = eve_widget_next(widget);
    }
}

int eve_form_handle_evt(EVEForm *form, EVEWidget *widget, EVETouch *touch, uint16_t evt) {
    static int16_t start;

    if ((evt & EVE_TOUCH_ETYPE_TRACK) && (touch->eevt & EVE_TOUCH_EETYPE_TRACK_Y)) {
        if (evt & EVE_TOUCH_ETYPE_TRACK_START) {
            start = form->p.win_y;
        }
        form->p.win_y = start + touch->y0 - touch->y;
        return 1;
    }
    if ((evt & EVE_TOUCH_ETYPE_POINT_UP) && (touch->eevt & EVE_TOUCH_EETYPE_TRACK_RIGHT)) {
        eve_page_close((EVEPage *)form);
        return 1;
    }
    if ((evt & EVE_TOUCH_ETYPE_POINT_UP) && (touch->eevt & EVE_TOUCH_EETYPE_TRACK_LEFT) && form->action) {
        form->action(form);
        return 1;
    }

    return 0;
}

EVEWidget *eve_form_widget(EVEForm *form, uint16_t idx) {
    EVEWidget *w = form->widget;
    int i;

    if (idx >= form->widget_size) return NULL;

    for (i=0; i<idx; i++) {
        w = eve_widget_next(w);
    }
    return w;
}
