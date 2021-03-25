#include <stdlib.h>
#include <string.h>

#include "eve.h"
#include "eve_kbd.h"
#include "eve_font.h"

#include "screen/window.h"
#include "screen/page.h"

#include "widgets.h"

static const size_t _widget_size[] = {
    0,
    sizeof(EVEFreeWidget),
    sizeof(EVESpacerWidget),
    sizeof(EVEPageWidget),
    sizeof(EVEStrWidget),
    sizeof(EVETextWidget),
    sizeof(EVESelectWidget),
};

static const eve_widget_create_t _widget_create[] = {
    NULL,
    (eve_widget_create_t)eve_freew_create,
    (eve_widget_create_t)eve_spacerw_create,
    (eve_widget_create_t)eve_pagew_create,
    (eve_widget_create_t)eve_strw_create,
    (eve_widget_create_t)eve_textw_create,
    (eve_widget_create_t)eve_selectw_create,
};

static const eve_widget_destroy_t _widget_destroy[] = {
    NULL,
    NULL,
    NULL,
    NULL,
    (eve_widget_destroy_t)eve_strw_destroy,
    (eve_widget_destroy_t)eve_textw_destroy,
    (eve_widget_destroy_t)eve_selectw_destroy,
};

void eve_widget_init(EVEWidget *widget, uint8_t type, EVERect *g, EVEPage *page, eve_widget_draw_t draw, eve_widget_touch_t touch, eve_kbd_input_handler_t putc) {
    if (g) widget->g = *g;
    widget->page = page;
    widget->draw = draw;
    widget->touch = touch;
    widget->putc = putc;
    widget->type = type;
}

size_t eve_widget_size(uint8_t type) {
    return _widget_size[type];
}

void eve_widget_set_label(EVEWidget *widget, EVELabel *label) {
    widget->label = label;
}

EVEWidget *eve_widget_next(EVEWidget *widget) {
    char *_w = (char *)widget;
    return (EVEWidget *)(_w + _widget_size[widget->type]);
}

int eve_widget_create(EVEWidget *widget, uint8_t type, EVERect *g, EVEPage *page, EVEWidgetSpecT *spec) {
    return _widget_create[type](widget, g, page, spec);
}

void eve_widget_destroy(EVEWidget *widget) {
    if (_widget_destroy[widget->type]) _widget_destroy[widget->type](widget);
}

void eve_widget_uievt_push(EVEWidget *widget, uint16_t evt, void *param) {
    EVEView *view = &widget->page->v;
    eve_view_uievt_push(view, evt, param ? param : widget);
}