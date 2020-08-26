#include <stdint.h>

struct EVEWidget;

typedef struct EVEForm {
    EVEPage p;
    struct EVEWidget *widget;
    uint16_t widget_size;
} EVEForm;

int eve_form_init(EVEForm *form, EVEWindow *window, EVEViewStack *stack, struct EVEWidget *widget, uint16_t widget_size, eve_page_destructor_t destructor);
int eve_form_touch(EVEView *v, uint8_t tag0, int touch_idx);
uint8_t eve_form_draw(EVEView *v, uint8_t tag0);
int eve_form_handle_evt(EVEPage *page, struct EVEWidget *widget, EVETouch *touch, uint16_t evt, uint8_t tag0, int touch_idx);
void eve_form_update_g(EVEPage *page, struct EVEWidget *widget);
struct EVEWidget *eve_form_widget(EVEForm *form, uint16_t idx);
