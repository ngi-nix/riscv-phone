#include <stdlib.h>

#include "eve/eve.h"
#include "eve/eve_kbd.h"
#include "eve/eve_font.h"

#include "eve/screen/window.h"
#include "eve/screen/page.h"
#include "eve/screen/form.h"

#include "eve/widget/widgets.h"

#include "app_form.h"

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
    int w_size = 0;
    int i, r;

    for (i=0; i<spec_size; i++) {
        w_size += eve_widget_size(spec[i].widget.type);
    }
    form = eve_malloc(sizeof(EVEForm));
    if (form == NULL) {
        return NULL;
    }
    if (destructor == NULL) destructor = app_form_destroy;
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
        if (widget->label && (widget->label->g.w == 0)) eve_font_str_w(label->font, label->title) + APP_LABEL_MARGIN;
        if (widget->g.w == 0) widget->g.w = window->g.w - (widget->label ? widget->label->g.w : 0);
        widget = eve_widget_next(widget);
    }
    eve_form_update(form, widgets, spec_size, NULL);

    return form;
}

void app_form_destroy(EVEForm *form) {
    widgets_destroy(form->widget, form->widget_size);
    eve_free(form->widget);
    eve_free(form);
}
