#include <stdint.h>

struct EVEWidget;

typedef struct EVEForm {
    EVEPage p;
    struct EVEWidget *widget_f;
    struct EVEWidget *widget;
    uint16_t widget_size;
} EVEForm;

int eve_form_init(EVEForm *form, struct EVEWidget *widget, uint16_t widget_size, eve_page_open_t open, eve_page_close_t close, EVEWindow *window);
int eve_form_touch(EVEView *v, uint8_t tag0, int touch_idx);
uint8_t eve_form_draw(EVEView *v, uint8_t tag0);
