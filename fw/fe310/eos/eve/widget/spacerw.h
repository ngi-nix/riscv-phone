#include <stdint.h>

typedef struct EVESpacerWidget {
    EVEWidget w;
} EVESpacerWidget;

typedef struct EVESpacerSpec {
} EVESpacerSpec;

int eve_spacerw_create(EVESpacerWidget *widget, EVERect *g, EVESpacerSpec *spec);
void eve_spacerw_init(EVESpacerWidget *widget, EVERect *g);

int eve_spacerw_touch(EVEWidget *_widget, EVEPage *page, uint8_t tag0, int touch_idx);
uint8_t eve_spacerw_draw(EVEWidget *_widget, EVEPage *page, uint8_t tag0);
