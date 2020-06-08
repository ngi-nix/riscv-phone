#include <stdint.h>

typedef struct EVEPageWidget {
    EVEWidget w;
    char *title;
    EVEPage *page;
    uint8_t tag;
    uint8_t font_id;
} EVEPageWidget;

void eve_pagew_init(EVEPageWidget *widget, EVERect *g, uint8_t font_id, char *title, EVEPage *page);
int eve_pagew_touch(EVEWidget *_widget, EVEPage *page, uint8_t tag0, int touch_idx, EVERect *focus);
uint8_t eve_pagew_draw(EVEWidget *_widget, EVEPage *page, uint8_t tag0);
