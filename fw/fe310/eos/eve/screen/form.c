#include <stdlib.h>
#include <string.h>

#include "eve.h"
#include "eve_kbd.h"
#include "eve_font.h"

#include "window.h"
#include "page.h"
#include "form.h"

#include "widget/label.h"
#include "widget/widget.h"

#define MIN(X, Y)               (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y)               (((X) > (Y)) ? (X) : (Y))

static void form_update_g(EVEForm *form, EVEWidget *_widget) {
    EVEWidget *widget = form->widget;
    int i;
    uint16_t w = 0;
    uint16_t h = 0;
    uint16_t l_h = 0;

    for (i=0; i<form->widget_size; i++) {
        if (widget->label) {
            h += l_h;
            w = widget->label->g.w;
            l_h = widget->label->g.h;
            widget->label->g.x = 0;
            widget->label->g.y = h;
        }
        if (w + widget->g.w > form->p.v.window->g.w) {
            h += l_h;
            w = 0;
            l_h = 0;
        }
        widget->g.x = w;
        widget->g.y = h;

        w += widget->g.w;
        l_h = MAX(l_h, widget->g.h);

        widget = eve_widget_next(widget);
    }
    form->w = form->p.v.window->g.w;
    form->h = h + l_h;
}

static int form_handle_evt(EVEForm *form, EVEWidget *widget, EVETouch *touch, uint16_t evt, uint8_t tag0) {
    int ret = 0;
    EVEPage *page = &form->p;

    if ((evt & EVE_TOUCH_ETYPE_POINT_UP) && !(touch->eevt & (EVE_TOUCH_EETYPE_TRACK_XY | EVE_TOUCH_EETYPE_ABORT))) {
        if (page->widget_f) eve_page_set_focus(page, NULL, NULL);
        ret = 1;
    }

    /* Scroll start */
    if ((evt & EVE_TOUCH_ETYPE_TRACK_START) && !(touch->eevt & EVE_TOUCH_EETYPE_ABORT)) {
        form->evt_lock = 1;
    }

    /* Scroll stop */
    if (((evt & EVE_TOUCH_ETYPE_TRACK_STOP) && !(evt & EVE_TOUCH_ETYPE_TRACK_ABORT)) ||
        ((evt & EVE_TOUCH_ETYPE_POINT_UP) && (touch->eevt & EVE_TOUCH_EETYPE_ABORT) && !(touch->eevt & EVE_TOUCH_EETYPE_TRACK_XY))) {
        int wmax_x, wmax_y;
        int lho_x, lho_y;
        EVERect vg;

        eve_window_visible_g(page->v.window, &vg);
        wmax_x = form->w > vg.w ? form->w - vg.w : 0;
        wmax_y = form->h > vg.h ? form->h - vg.h : 0;
        lho_x = page->win_x < 0 ? 0 : wmax_x;
        lho_y = page->win_y < 0 ? 0 : wmax_y;
        if ((page->win_x < 0) || (page->win_x > wmax_x) ||
            (page->win_y < 0) || (page->win_y > wmax_y)) {
            EVEPhyLHO *lho = &form->lho;
            eve_phy_lho_init(lho, lho_x, lho_y, 1000, 0.5, 0);
            eve_phy_lho_start(lho, page->win_x, page->win_y);
            form->lho_t0 = eve_time_get_tick();
            eve_touch_timer_start(tag0, 20);
        } else {
            form->evt_lock = 0;
            if (evt & EVE_TOUCH_ETYPE_TRACK_STOP) {
                if (touch->eevt & EVE_TOUCH_EETYPE_TRACK_RIGHT) {
                    eve_page_close((EVEPage *)form);
                    return 1;
                }
                if (touch->eevt & EVE_TOUCH_EETYPE_TRACK_LEFT) {
                    if (form->action) form->action(form);
                    return 1;
                }
            }
        }
    }

    if (evt & EVE_TOUCH_ETYPE_TRACK_START) {
        if (touch->eevt & EVE_TOUCH_EETYPE_TRACK_Y) {
            form->win_y0 = page->win_y;
        }
    }
    if (evt & EVE_TOUCH_ETYPE_TRACK) {
        if (touch->eevt & EVE_TOUCH_EETYPE_TRACK_Y) {
            page->win_y = form->win_y0 + touch->y0 - touch->y;
        }
        ret = 1;
    }
    if (evt & EVE_TOUCH_ETYPE_TIMER) {
        EVEPhyLHO *lho = &form->lho;
        int more = eve_phy_lho_tick(lho, eve_time_get_tick() - form->lho_t0, NULL, &page->win_y);

        if (!more) {
            form->evt_lock = 0;
            eve_touch_timer_stop();
        }
        ret = 1;
    }

    return ret;
}

void eve_form_init(EVEForm *form, EVEWindow *window, EVEViewStack *stack, EVEWidget *widget, uint16_t widget_size, eve_form_action_t action, eve_form_destructor_t destructor) {
    memset(form, 0, sizeof(EVEForm));
    eve_page_init(&form->p, window, stack, eve_form_draw, eve_form_touch, NULL, (eve_page_destructor_t)destructor);
    eve_form_update(form, widget, widget_size, action);
}

void eve_form_update(EVEForm *form, EVEWidget *widget, uint16_t widget_size, eve_form_action_t action) {
    if (widget) {
        form->widget = widget;
        form->widget_size = widget_size;
    }
    if (action) form->action = action;
    form_update_g(form, NULL);
}

uint8_t eve_form_draw(EVEView *view, uint8_t tag0) {
    EVEForm *form = (EVEForm *)view;
    EVEWidget *widget = form->widget;
    int i;
    uint8_t tagN = tag0;
    uint8_t tag_opt = EVE_TOUCH_OPT_TRACK | EVE_TOUCH_OPT_TRACK_XY | EVE_TOUCH_OPT_TRACK_EXT_Y;

    tagN = eve_view_clear(view, tagN);

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
            tagN = widget->draw(widget, tagN);
            if (h != widget->g.h) form_update_g(form, widget);
        }
        widget = eve_widget_next(widget);
    }

    eve_cmd_dl(RESTORE_CONTEXT());

    for (i=tag0; i<tagN; i++) {
        eve_touch_set_opt(i, eve_touch_get_opt(i) | tag_opt);
    }
    if (view->tag != EVE_TAG_NOTAG) eve_touch_set_opt(view->tag, eve_touch_get_opt(view->tag) | tag_opt);

    return tagN;
}

int eve_form_touch(EVEView *view, EVETouch *touch, uint16_t evt, uint8_t tag0) {
    EVEForm *form = (EVEForm *)view;
    EVEWidget *widget = form->widget;
    int8_t touch_idx = eve_touch_get_idx(touch);
    uint16_t _evt;
    int i, ret;

    if (touch_idx > 0) return 0;

    _evt = eve_touch_evt(touch, evt, tag0, form->p.v.tag, 1);
    if (_evt) {
        ret = form_handle_evt(form, NULL, touch, _evt, tag0);
        if (ret) return 1;
    }
    for (i=0; i<form->widget_size; i++) {
        _evt = eve_touch_evt(touch, evt, tag0, widget->tag0, widget->tagN - widget->tag0);
        if (_evt) {
            if (!form->evt_lock) {
                ret = widget->touch(widget, touch, _evt);
                if (ret) return 1;
            }
            ret = form_handle_evt(form, widget, touch, _evt, tag0);
            if (ret) return 1;
        }
        widget = eve_widget_next(widget);
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
