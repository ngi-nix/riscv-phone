#include <stdint.h>

#define APP_SCREEN_W        480
#define APP_SCREEN_H        800
#define APP_STATUS_H        60

#define APP_FONT_HANDLE     31

typedef struct APPLabelSpec {
    EVERect g;
    EVEFont *font;
    char *title;
} APPLabelSpec;

typedef struct APPWidgetSpec {
    APPLabelSpec label;
    struct {
        EVERect g;
        EVEWidgetSpec spec;
        uint8_t type;
    } widget;
} APPWidgetSpec;

EVEScreen *app_screen(void);
void app_screen_init(eve_view_constructor_t home_page);
void app_screen_refresh(void);

EVEForm *app_form_create(EVEWindow *window, EVEViewStack *stack, APPWidgetSpec spec[], uint16_t spec_size, eve_form_action_t action, eve_form_destructor_t destructor);
void app_form_destroy(EVEForm *form);
