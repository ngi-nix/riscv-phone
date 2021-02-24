#include <stdlib.h>

#include "net.h"
#include "unicode.h"

#include "eve/eve.h"
#include "eve/eve_kbd.h"

#include "eve/screen/screen.h"
#include "eve/screen/window.h"
#include "eve/screen/view.h"
#include "eve/screen/page.h"
#include "eve/screen/form.h"

#include "eve/widget/widgets.h"

#include "status.h"
#include "root.h"

EVEFont *_app_font_default;

static EVEKbd kbd;
static EVEFont font;
static EVEScreen screen;
static EVEWindow win_status;
static EVEWindow win_main;
static EVEWindow win_kbd;
static EVEView view_kbd;
static EVEView view_status;
static EVEViewStack view_stack;

EVEScreen *app_screen(void) {
    return &screen;
}

static int kbd_touch(EVEView *v, uint8_t tag0, int touch_idx) {
    EVEKbd *kbd = v->param;

    return eve_kbd_touch(kbd, tag0, touch_idx);
}

static uint8_t kbd_draw(EVEView *v, uint8_t tag0) {
    EVEKbd *kbd = v->param;

    eve_kbd_draw(kbd);
    return tag0;
}

void app_root_init(eve_view_constructor_t home_page) {
    EVERect g;

    _app_font_default = &font;
    eos_spi_dev_start(EOS_DEV_DISP);

    eve_brightness(0x40);

    eve_font_init(&font, APP_FONT_HANDLE);
    eve_screen_init(&screen, APP_SCREEN_W, APP_SCREEN_H);

    eve_kbd_init(&kbd, NULL, screen.mem_next, &screen.mem_next);
    eve_screen_set_kbd(&screen, &kbd);

    g.x = 0;
    g.y = 0;
    g.w = APP_SCREEN_W;
    g.h = APP_STATUS_H;
    eve_window_init(&win_status, &g, &screen, "status");
    eve_view_init(&view_status, &win_status, app_status_touch, app_status_draw, NULL);

    g.x = 0;
    g.y = APP_STATUS_H;
    g.w = APP_SCREEN_W;
    g.h = APP_SCREEN_H - APP_STATUS_H;
    eve_window_init(&win_main, &g, &screen, "main");

    eve_window_init(&win_kbd, &kbd.g, &screen, "kbd");
    eve_view_init(&view_kbd, &win_kbd, kbd_touch, kbd_draw, &kbd);

    eve_view_stack_init(&view_stack);
    eve_view_create(&win_main, &view_stack, home_page);

    eve_window_append(&win_status);
    eve_window_append(&win_main);
    eve_window_append(&win_kbd);

    eve_screen_hide_kbd(&screen);
    eve_screen_draw(&screen);

    eos_spi_dev_stop();

    eos_net_acquire_for_evt(EOS_EVT_UI | EVE_ETYPE_INTR, 1);
}

static void widgets_destroy(EVEWidget *widget, uint16_t widget_size) {
    int i;

    for (i=0; i<widget_size; i++) {
        if (widget->label) eve_free(widget->label);
        eve_widget_destroy(widget);
        widget = eve_widget_next(widget);
    }
}

EVEForm *app_form_create(EVEWindow *window, EVEViewStack *stack, APPWidgetSpec spec[], uint16_t spec_size, eve_form_action_t action, eve_form_destructor_t destructor) {
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

    widgets = eve_malloc(w_size);
    if (widgets == NULL) {
        eve_free(form);
        return NULL;
    }

    widget = widgets;
    for (i=0; i<spec_size; i++) {
        r = eve_widget_create(widget, spec[i].widget.type, &spec[i].widget.g, &spec[i].widget.spec);
        if (r) {
            widgets_destroy(widgets, i);
            eve_free(widgets);
            eve_free(form);
            return NULL;
        }
        if (spec[i].label.title) {
            label = eve_malloc(sizeof(EVELabel));
            if (label == NULL) {
                eve_widget_destroy(widget);
                widgets_destroy(widgets, i);
                eve_free(widgets);
                eve_free(form);
                return NULL;
            }
            eve_label_init(label, &spec[i].label.g, spec[i].label.font, spec[i].label.title);
            eve_widget_set_label(widget, label);
        }
        widget = eve_widget_next(widget);
    }

    if (destructor == NULL) destructor = app_form_destroy;
    eve_form_init(form, window, stack, widgets, spec_size, action, destructor);

    return form;
}

void app_form_destroy(EVEForm *form) {
    widgets_destroy(form->widget, form->widget_size);
    eve_free(form->widget);
    eve_free(form);
}
