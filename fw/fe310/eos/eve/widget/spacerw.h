#include <stdint.h>

typedef struct EVESpacerWidget {
    EVEWidget w;
} EVESpacerWidget;

typedef struct EVESpacerSpec {
} EVESpacerSpec;

int eve_spacerw_create(EVESpacerWidget *widget, EVERect *g, EVEPage *page, EVESpacerSpec *spec);
void eve_spacerw_init(EVESpacerWidget *widget, EVERect *g, EVEPage *page);

uint8_t eve_spacerw_draw(EVEWidget *_widget, uint8_t tag0);
int eve_spacerw_touch(EVEWidget *_widget, EVETouch *touch, uint16_t evt);
