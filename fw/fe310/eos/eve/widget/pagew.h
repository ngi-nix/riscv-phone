#include <stdint.h>

typedef struct EVEPageWidget {
    EVEWidget w;
    EVEFont *font;
    char *title;
    eve_view_constructor_t constructor;
} EVEPageWidget;

typedef struct EVEPageSpec {
    EVEFont *font;
    char *title;
    eve_view_constructor_t constructor;
} EVEPageSpec;

int eve_pagew_create(EVEPageWidget *widget, EVERect *g, EVEPage *page, EVEPageSpec *spec);
void eve_pagew_init(EVEPageWidget *widget, EVERect *g, EVEPage *page, EVEFont *font, char *title, eve_view_constructor_t constructor);

uint8_t eve_pagew_draw(EVEWidget *_widget, uint8_t tag0);
int eve_pagew_touch(EVEWidget *_widget, EVETouch *touch, uint16_t evt);
