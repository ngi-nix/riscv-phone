#include <stdint.h>

#define EVE_FORM_LABEL_MARGIN   10

struct EVEWidget;
struct EVEWidgetSpec;
struct EVEForm;

typedef int (*eve_form_uievt_t) (struct EVEForm *, uint16_t, void *);
typedef void (*eve_form_action_t) (struct EVEForm *);
typedef void (*eve_form_destructor_t) (struct EVEForm *);

typedef struct EVEForm {
    EVEPage p;
    eve_form_action_t action;
} EVEForm;

EVEForm *eve_form_create(EVEWindow *window, EVEViewStack *stack, struct EVEWidgetSpec *spec, uint16_t spec_size, eve_form_uievt_t uievt, eve_form_action_t action, eve_form_destructor_t destructor);
void eve_form_init(EVEForm *form, EVEWindow *window, EVEViewStack *stack, struct EVEWidget *widget, uint16_t widget_size, eve_form_uievt_t uievt, eve_form_action_t action, eve_form_destructor_t destructor);
void eve_form_update(EVEForm *form, struct EVEWidget *widget, uint16_t widget_size);
void eve_form_destroy(EVEForm *form);

int eve_form_uievt(EVEForm *form, uint16_t evt, void *param);
