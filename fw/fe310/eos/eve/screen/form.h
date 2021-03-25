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
    int win_x0;
    int win_y0;
    uint16_t w;
    uint16_t h;
    uint8_t evt_lock;
    EVEPhyLHO lho;
    uint64_t lho_t0;
} EVEForm;

void eve_form_init(EVEForm *form, EVEWindow *window, EVEViewStack *stack, struct EVEWidget *widget, uint16_t widget_size, eve_form_action_t action, eve_form_destructor_t destructor);
void eve_form_update(EVEForm *form, struct EVEWidget *widget, uint16_t widget_size, eve_form_action_t action);

uint8_t eve_form_draw(EVEView *view, uint8_t tag0);
int eve_form_touch(EVEView *view, EVETouch *touch, uint16_t evt, uint8_t tag0);

void eve_form_update_g(EVEForm *form, struct EVEWidget *widget);
int eve_form_handle_evt(EVEForm *form, struct EVEWidget *widget, EVETouch *touch, uint16_t evt);
struct EVEWidget *eve_form_widget(EVEForm *form, uint16_t idx);
