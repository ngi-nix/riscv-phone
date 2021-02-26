#include <stdint.h>

struct EVEWidget;
struct EVEForm;

typedef void (*eve_form_action_t) (struct EVEForm *);
typedef void (*eve_form_destructor_t) (struct EVEForm *);

typedef struct EVEForm {
    EVEPage p;
    struct EVEWidget *widget;
    uint16_t widget_size;
    eve_form_action_t action;
    int16_t win_x0;
    int16_t win_y0;
    uint8_t evt_lock;
} EVEForm;

int eve_form_init(EVEForm *form, EVEWindow *window, EVEViewStack *stack, struct EVEWidget *widget, uint16_t widget_size, eve_form_action_t action, eve_form_destructor_t destructor);

int eve_form_touch(EVEView *v, uint8_t tag0, int touch_idx);
uint8_t eve_form_draw(EVEView *v, uint8_t tag0);

void eve_form_update_g(EVEForm *form, struct EVEWidget *widget);
int eve_form_handle_evt(EVEForm *form, struct EVEWidget *widget, EVETouch *touch, uint16_t evt);
struct EVEWidget *eve_form_widget(EVEForm *form, uint16_t idx);
