#include <stdlib.h>

#include "unicode.h"

#include "eve/eve.h"
#include "eve/eve_kbd.h"
#include "eve/eve_font.h"

#include "eve/screen/screen.h"
#include "eve/screen/window.h"
#include "eve/screen/view.h"
#include "eve/screen/page.h"
#include "eve/screen/form.h"

#include "eve/widget/widgets.h"

#include "app_screen.h"
#include "app_form.h"

static EVEFont font;

static void widgets_destroy(EVEWidget *widget, uint16_t widget_size) {
    int i;

    for (i=0; i<widget_size; i++) {
        if (widget->label) eve_free(widget->label);
        eve_widget_destroy(widget);
        widget = eve_widget_next(widget);
    }
}

EVEForm *app_form_create(EVEWindow *window, EVEViewStack *stack, EVEWidgetSpec spec[], uint16_t spec_size, eve_form_action_t action, eve_form_destructor_t destructor) {
    EVEWidget *widgets;
    EVEWidget *widget;
    EVELabel *label;
    EVEForm *form;
    EVEFont *_font;
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
        _font = spec[i].widget.font ? spec[i].widget.font : &font;
        r = eve_widget_create(widget, spec[i].widget.type, &spec[i].widget.g, _font, &spec[i].widget.spec);
        if (r) {
            widgets_destroy(widgets, i);
            eve_free(widgets);
            eve_free(form);
            return NULL;
        }
        if (spec[i].label.title) {
            _font = spec[i].label.font ? spec[i].label.font : &font;
            label = eve_malloc(sizeof(EVELabel));
            if (label == NULL) {
                eve_widget_destroy(widget);
                widgets_destroy(widgets, i);
                eve_free(widgets);
                eve_free(form);
                return NULL;
            }
            eve_label_init(label, &spec[i].label.g, _font, spec[i].label.title);
            eve_widget_set_label(widget, label);
            if (label->g.w == 0) label->g.w = eve_font_str_w(_font, label->title);
        }
        if (widget->label && (widget->label->g.w == 0)) eve_font_str_w(label->font, label->title) + APP_LABEL_MARGIN;
        if (widget->g.w == 0) widget->g.w = APP_SCREEN_W - (widget->label ? widget->label->g.w : 0);
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

void app_form_init(void) {
    eve_spi_start();
    eve_font_init(&font, APP_FONT_HANDLE);
    eve_spi_stop();
}
