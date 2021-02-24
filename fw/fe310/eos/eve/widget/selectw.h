#include <stdint.h>

typedef struct EVESelectWidget {
    EVEWidget w;
    EVEFont *font;
    utf8_t *option;
    uint16_t option_size;
    uint32_t select;
} EVESelectWidget;

typedef struct EVESelectSpec {
    EVEFont *font;
    uint16_t option_size;
} EVESelectSpec;

int eve_selectw_create(EVESelectWidget *widget, EVERect *g, EVESelectSpec *spec);
void eve_selectw_destroy(EVESelectWidget *widget);
void eve_selectw_init(EVESelectWidget *widget, EVERect *g, EVEFont *font, utf8_t *option, uint16_t option_size);
void eve_selectw_update(EVESelectWidget *widget, EVEFont *font, utf8_t *option, uint16_t option_size);

int eve_selectw_touch(EVEWidget *_widget, EVEPage *page, EVETouch *t, uint16_t evt);
uint8_t eve_selectw_draw(EVEWidget *_widget, EVEPage *page, uint8_t tag0);

utf8_t *eve_selectw_option_get(EVESelectWidget *widget, int idx);
utf8_t *eve_selectw_option_get_select(EVESelectWidget *widget);
int eve_selectw_option_add(EVESelectWidget *widget, utf8_t *opt);
int eve_selectw_option_set(EVESelectWidget *widget, utf8_t *opt, uint16_t size);
