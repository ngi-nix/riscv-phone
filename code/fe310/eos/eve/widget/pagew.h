#include <stdint.h>

typedef struct EVEPageWidget {
    EVEWidget w;
    char *title;
    EVEFont *font;
    EVEPage *page;
    uint8_t tag;
} EVEPageWidget;

void eve_pagew_init(EVEPageWidget *widget, EVERect *g, char *title, EVEFont *font, EVEPage *page);
int eve_pagew_touch(EVEWidget *_widget, EVEPage *page, uint8_t tag0, int touch_idx, EVERect *focus);
uint8_t eve_pagew_draw(EVEWidget *_widget, EVEPage *page, uint8_t tag0);
