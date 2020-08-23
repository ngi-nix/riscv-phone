#include <stdint.h>

typedef struct EVEPageWidget {
    EVEWidget w;
    char *title;
    EVEFont *font;
    eve_page_constructor_t constructor;
    uint8_t tag;
} EVEPageWidget;

typedef struct EVEPageSpec {
    EVEFont *font;
    char *title;
    eve_page_constructor_t constructor;
} EVEPageSpec;

int eve_pagew_create(EVEPageWidget *widget, EVERect *g, EVEPageSpec *spec);
void eve_pagew_init(EVEPageWidget *widget, EVERect *g, EVEFont *font, char *title, eve_page_constructor_t constructor);
void eve_pagew_update(EVEPageWidget *widget, EVEFont *font, char *title, eve_page_constructor_t constructor);

int eve_pagew_touch(EVEWidget *_widget, EVEPage *page, uint8_t tag0, int touch_idx);
uint8_t eve_pagew_draw(EVEWidget *_widget, EVEPage *page, uint8_t tag0);
