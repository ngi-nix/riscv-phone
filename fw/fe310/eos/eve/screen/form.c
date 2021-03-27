#include <stdlib.h>
#include <string.h>

#include "eve.h"
#include "eve_kbd.h"
#include "eve_font.h"

#include "window.h"
#include "page.h"
#include "form.h"

#include "widget/widgets.h"

#define MIN(X, Y)               (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y)               (((X) > (Y)) ? (X) : (Y))

static void form_update_g(EVEForm *form, EVEWidget *_widget) {
    EVEPage *page = &form->p;
    EVEWidget *widget = page->widget;
    int i;
    uint16_t w = 0;
    uint16_t h = 0;
    uint16_t l_h = 0;

    for (i=0; i<page->widget_size; i++) {
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
    page->g.w = page->v.window->g.w;
    page->g.h = h + l_h;
}

static void widgets_destroy(EVEWidget *widget, uint16_t widget_size) {
    int i;

    for (i=0; i<widget_size; i++) {
        if (widget->label) eve_free(widget->label);
        eve_widget_destroy(widget);
        widget = eve_widget_next(widget);
    }
}

EVEForm *eve_form_create(EVEWindow *window, EVEViewStack *stack, EVEWidgetSpec spec[], uint16_t spec_size, eve_form_action_t action, eve_form_destructor_t destructor) {
    EVEWidget *widgets;
    EVEWidget *widget;
    EVELabel *label;
    EVEForm *form;
    int w_size = 0;
    int i, r;

    for (i=0; i<spec_size; i++) {
        w_size += eve_widget_size(spec[i].widget.type);
    }
    form = eve_malloc(sizeof(EVEForm));
    if (form == NULL) {
        return NULL;
    }
    if (destructor == NULL) destructor = eve_form_destroy;
    eve_form_init(form, window, stack, NULL, 0, action, destructor);

    widgets = eve_malloc(w_size);
    if (widgets == NULL) {
        eve_free(form);
        return NULL;
    }

    widget = widgets;
    for (i=0; i<spec_size; i++) {
        r = eve_widget_create(widget, spec[i].widget.type, &spec[i].widget.g, (EVEPage *)form, &spec[i].widget.spec);
        if (r) {
            widgets_destroy(widgets, i);
            eve_free(widgets);
            eve_free(form);
            return NULL;
        }
        if (spec[i].label.title) {
            EVEFont *font = spec[i].label.font ? spec[i].label.font : eve_window_font(window);
            label = eve_malloc(sizeof(EVELabel));
            if (label == NULL) {
                eve_widget_destroy(widget);
                widgets_destroy(widgets, i);
                eve_free(widgets);
                eve_free(form);
                return NULL;
            }
            eve_label_init(label, &spec[i].label.g, font, spec[i].label.title);
            eve_widget_set_label(widget, label);
            if (label->g.w == 0) label->g.w = eve_font_str_w(font, label->title);
        }
        if (widget->label && (widget->label->g.w == 0)) eve_font_str_w(label->font, label->title) + EVE_FORM_LABEL_MARGIN;
        if (widget->g.w == 0) widget->g.w = window->g.w - (widget->label ? widget->label->g.w : 0);
        widget = eve_widget_next(widget);
    }
    eve_form_update(form, widgets, spec_size);

    return form;
}

void eve_form_init(EVEForm *form, EVEWindow *window, EVEViewStack *stack, EVEWidget *widget, uint16_t widget_size, eve_form_action_t action, eve_form_destructor_t destructor) {
    memset(form, 0, sizeof(EVEForm));
    eve_page_init(&form->p, window, stack, NULL, 0, EVE_PAGE_OPT_SCROLL_Y | EVE_PAGE_OPT_SCROLL_BACK | EVE_PAGE_OPT_TRACK_EXT_Y, eve_page_draw, eve_page_touch, eve_form_uievt, (eve_page_destructor_t)destructor);
    form->action = action;
    eve_form_update(form, widget, widget_size);
}

void eve_form_update(EVEForm *form, EVEWidget *widget, uint16_t widget_size) {
    eve_page_update((EVEPage *)form, widget, widget_size);
    form_update_g(form, NULL);
}

void eve_form_destroy(EVEForm *form) {
    widgets_destroy(form->p.widget, form->p.widget_size);
    eve_free(form->p.widget);
    eve_free(form);
}

int eve_form_uievt(EVEView *view, uint16_t evt, void *param) {
    EVEForm *form = (EVEForm *)view;

    switch (evt) {
        case EVE_UIEVT_WIDGET_UPDATE_G:
            form_update_g(form, (EVEWidget *)param);
            break;

        case EVE_UIEVT_PAGE_SCROLL_START:
            break;

        case EVE_UIEVT_PAGE_SCROLL_STOP:
            break;

        case EVE_UIEVT_PAGE_TRACK_START:
            break;

        case EVE_UIEVT_PAGE_TRACK_STOP: {
            EVEUIEvtTouch *touch_p = (EVEUIEvtTouch *)param;
            if (touch_p->evt & EVE_TOUCH_ETYPE_TRACK_STOP) {
                if (touch_p->touch->eevt & EVE_TOUCH_EETYPE_TRACK_RIGHT) {
                    eve_page_close((EVEPage *)form);
                    return 1;
                }
                if (touch_p->touch->eevt & EVE_TOUCH_EETYPE_TRACK_LEFT) {
                    if (form->action) form->action(form);
                }
            }
            break;
        }
    }
    return 0;
}
