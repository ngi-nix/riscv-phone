#include <stdlib.h>
#include <string.h>

#include "eve.h"
#include "eve_kbd.h"

#include "screen.h"
#include "window.h"
#include "page.h"
#include "font.h"
#include "form.h"

#include "widget/label.h"
#include "widget/widget.h"

#define CH_EOF              0x1a

int eve_form_init(EVEForm *form, EVEWidget *widget, uint16_t widget_size, eve_page_open_t open, eve_page_close_t close, EVEWindow *window) {
    memset(form, 0, sizeof(EVEForm));
    eve_page_init(&form->p, eve_form_touch, eve_form_draw, open, close, eve_form_handle_evt, eve_form_update_g, window);
    form->widget = widget;
    form->widget_size = widget_size;
    eve_form_update_g(&form->p, NULL);
}

int eve_form_touch(EVEView *v, uint8_t tag0, int touch_idx) {
    EVEForm *form = (EVEForm *)v;
    EVEWidget *widget = form->widget;
    EVEWidget *widget_f = eve_page_get_focus(&form->p);
    int i, ret = 0;
    EVERect focus = {0,0,0,0};

    for (i=0; i<form->widget_size; i++) {
        if (eve_page_rect_visible(&form->p, &widget->g)) {
            int a;
            EVERect r = {0,0,0,0};

            a = widget->touch(widget, &form->p, tag0, touch_idx, &r);
            ret = ret || a;
            if (r.w && r.h && (widget_f != widget)) {
                EVEKbd *kbd = eve_screen_get_kbd(form->p.window->screen);

                if (kbd) {
                    if (widget_f && widget_f->putc) {
                        eve_screen_hide_kbd(form->p.window->screen);
                        widget_f->putc(widget_f, CH_EOF);
                    }
                    eve_kbd_set_handler(kbd, widget->putc, &form->p);
                    if (widget->putc) {
                        eve_screen_show_kbd(form->p.window->screen);
                    }
                }
                widget_f = widget;
                focus = r;
            }
        }
        widget = eve_widget_next(widget);
    }

    if (focus.w && focus.h) eve_page_set_focus(&form->p, widget_f, &focus);
    return ret;
}

uint8_t eve_form_draw(EVEView *v, uint8_t tag0) {
    EVEForm *form = (EVEForm *)v;
    EVEWidget *widget = form->widget;
    int i;
    uint8_t tagN = tag0;

    eve_cmd_dl(VERTEX_FORMAT(0));

    for (i=0; i<form->widget_size; i++) {
        if (widget->label && eve_page_rect_visible(&form->p, &widget->label->g)) {
            eve_cmd_dl(TAG_MASK(0));
            eve_label_draw(widget->label);
            eve_cmd_dl(TAG_MASK(1));
        }
        if (eve_page_rect_visible(&form->p, &widget->g)) tagN = widget->draw(widget, &form->p, tagN);
        widget = eve_widget_next(widget);
    }

    for (i=tag0; i<tagN; i++) {
        eve_touch_set_opt(i, eve_touch_get_opt(i) | EVE_TOUCH_OPT_TRACK | EVE_TOUCH_OPT_TRACK_XY | EVE_TOUCH_OPT_TRACK_EXT);
    }

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
            if (widget->label->g.w + widget->g.w > form->p.window->screen->w) h += widget->label->g.h;
        }
        widget->g.y = h;
        h += widget->g.h;

        widget = eve_widget_next(widget);
    }
}
