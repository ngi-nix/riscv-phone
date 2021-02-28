#include <stdint.h>

#define APP_SCREEN_W        480
#define APP_SCREEN_H        800
#define APP_STATUS_H        60

EVEScreen *app_screen(void);
void app_screen_init(eve_view_constructor_t home_page);
void app_screen_refresh(void);
