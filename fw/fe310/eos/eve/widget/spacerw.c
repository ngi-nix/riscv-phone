#include <stdlib.h>
#include <string.h>

#include "eve.h"
#include "eve_kbd.h"
#include "eve_font.h"

#include "screen/window.h"
#include "screen/page.h"

#include "label.h"
#include "widget.h"
#include "spacerw.h"

int eve_spacerw_create(EVESpacerWidget *widget, EVERect *g, EVEPage *page, EVESpacerSpec *spec) {
    eve_spacerw_init(widget, g, page);

    return EVE_OK;
}

void eve_spacerw_init(EVESpacerWidget *widget, EVERect *g, EVEPage *page) {
    EVEWidget *_widget = &widget->w;

    memset(widget, 0, sizeof(EVESpacerWidget));
    eve_widget_init(_widget, EVE_WIDGET_TYPE_SPACER, g, page, eve_spacerw_draw, eve_spacerw_touch, NULL);
}

uint8_t eve_spacerw_draw(EVEWidget *_widget, uint8_t tag0) {
    _widget->tag0 = tag0;
    _widget->tagN = tag0;
    return _widget->tagN;
}

int eve_spacerw_touch(EVEWidget *_widget, EVETouch *touch, uint16_t evt) {
    return 0;
}
