#include <stdlib.h>
#include <string.h>

#include "eve.h"
#include "eve_kbd.h"

#include "screen.h"
#include "tile.h"
#include "page.h"
#include "form.h"

#include "widget/widget.h"

#define CH_EOF              0x1a

int eve_form_init(EVEForm *form, EVETile *tile, eve_page_open_t open, eve_page_close_t close, EVEWidget *widget, uint16_t widget_size) {
    memset(form, 0, sizeof(EVEForm));
    eve_page_init(&form->p, tile, eve_form_touch, eve_form_draw, open, close);
    form->widget = widget;
    form->widget_size = widget_size;

}
int eve_form_touch(EVECanvas *c, uint8_t tag0, int touch_idx) {
    EVEForm *form = (EVEForm *)c;
    EVEWidget *widget = form->widget;
    int a, i, ret = 0;
    EVEPageFocus focus = {NULL, {0,0,0,0}};

    for (i=0; i<form->widget_size; i++) {
        a = widget->touch(widget, &form->p, tag0, touch_idx, &focus);
        ret = ret || a;
        if (focus.w && (form->widget_f != focus.w)) {
            EVEKbd *kbd = eve_screen_get_kbd(form->p.tile->screen);

            if (kbd) {
                if (form->widget_f && form->widget_f->putc) {
                    eve_screen_hide_kbd(form->p.tile->screen);
                    form->widget_f->putc(form->widget_f, CH_EOF);
                }
                eve_kbd_set_handler(kbd, widget->putc);
                if (widget && widget->putc) {
                    eve_screen_show_kbd(form->p.tile->screen);
                }
            }
            form->widget_f = widget;
        }
        widget = eve_widget_next(widget);
    }

    eve_page_focus(&form->p, &focus);
    return ret;
}

uint8_t eve_form_draw(EVECanvas *c, uint8_t tag0) {
    EVEForm *form = (EVEForm *)c;
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

