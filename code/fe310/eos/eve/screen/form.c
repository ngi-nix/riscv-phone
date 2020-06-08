#include <stdlib.h>
#include <string.h>

#include "eve.h"
#include "eve_kbd.h"

#include "screen.h"
#include "window.h"
#include "page.h"
#include "form.h"

#include "widget/widget.h"

#define CH_EOF              0x1a

int eve_form_init(EVEForm *form, EVEWidget *widget, uint16_t widget_size, eve_page_open_t open, eve_page_close_t close, EVEWindow *window) {
    memset(form, 0, sizeof(EVEForm));
    eve_page_init(&form->p, eve_form_touch, eve_form_draw, open, close, window);
    form->widget = widget;
    form->widget_size = widget_size;
}

int eve_form_touch(EVEView *v, uint8_t tag0, int touch_idx) {
    EVEForm *form = (EVEForm *)v;
    EVEWidget *widget = form->widget;
    int a, i, ret = 0;
    EVERect focus = {0,0,0,0};

    for (i=0; i<form->widget_size; i++) {
        a = widget->touch(widget, &form->p, tag0, touch_idx, &focus);
        ret = ret || a;
        if (focus.w && focus.h && (form->widget_f != widget)) {
            EVEKbd *kbd = eve_screen_get_kbd(form->p.window->screen);

            if (kbd) {
                if (form->widget_f && form->widget_f->putc) {
                    eve_screen_hide_kbd(form->p.window->screen);
                    form->widget_f->putc(form->widget_f, CH_EOF);
                }
                eve_kbd_set_handler(kbd, widget->putc, widget);
                if (widget && widget->putc) {
                    eve_screen_show_kbd(form->p.window->screen);
                }
            }
            form->widget_f = widget;
        }
        widget = eve_widget_next(widget);
    }

    eve_page_focus(&form->p, &focus);
    return ret;
}

uint8_t eve_form_draw(EVEView *v, uint8_t tag0) {
    EVEForm *form = (EVEForm *)v;
    EVEWidget *widget = form->widget;
    int i, j;
    uint8_t tagN, _tagN = 0;

    for (i=0; i<form->widget_size; i++) {
        tagN = widget->draw(widget, &form->p, tag0);
        if (tagN) {
            for (j=tag0; j<=tagN; j++) {
                eve_touch_set_opt(j, eve_touch_get_opt(j) | EVE_TOUCH_OPT_TRACK | EVE_TOUCH_OPT_TRACK_XY | EVE_TOUCH_OPT_TRACK_EXT);
            }
            if (tagN < 0xfe) {
                tag0 = tagN + 1;
            } else {
                tag0 = 0;
            }
            _tagN = tagN;
        }
        widget = eve_widget_next(widget);
    }

    return _tagN;
}
