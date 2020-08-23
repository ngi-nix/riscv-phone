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


void app_root_init(void);
EVEForm *app_form_create(EVEWindow *window, EVEPageStack *stack, APPWidgetSpec spec[], uint16_t spec_size, eve_page_destructor_t destructor);
void app_form_destroy(EVEForm *form);