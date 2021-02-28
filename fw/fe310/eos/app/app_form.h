#include <stdint.h>

#define APP_FONT_HANDLE     31
#define APP_LABEL_MARGIN    10

EVEForm *app_form_create(EVEWindow *window, EVEViewStack *stack, EVEWidgetSpec spec[], uint16_t spec_size, eve_form_action_t action, eve_form_destructor_t destructor);
void app_form_destroy(EVEForm *form);
