#include <stdint.h>

typedef struct EVESelectWidget {
    EVEWidget w;
    EVEFont *font;
    utf8_t *option;
    uint16_t option_size;
    uint16_t option_count;
    uint32_t select;
    uint16_t line0;
    uint8_t multi;
} EVESelectWidget;

typedef struct EVESelectSpec {
    EVEFont *font;
    uint16_t option_size;
    uint8_t multi;
} EVESelectSpec;

int eve_selectw_create(EVESelectWidget *widget, EVERect *g, EVEPage *page, EVESelectSpec *spec);
void eve_selectw_init(EVESelectWidget *widget, EVERect *g, EVEPage *page, EVEFont *font, utf8_t *option, uint16_t option_size, uint8_t multi);
void eve_selectw_destroy(EVESelectWidget *widget);

uint8_t eve_selectw_draw(EVEWidget *_widget, uint8_t tag0);
int eve_selectw_touch(EVEWidget *_widget, EVETouch *touch, uint16_t evt);

utf8_t *eve_selectw_option_get(EVESelectWidget *widget, int idx);
utf8_t *eve_selectw_option_get_select(EVESelectWidget *widget);
int eve_selectw_option_add(EVESelectWidget *widget, utf8_t *option);
int eve_selectw_option_set(EVESelectWidget *widget, utf8_t *option, uint16_t option_size);
