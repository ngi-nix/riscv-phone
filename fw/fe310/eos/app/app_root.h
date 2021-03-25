#include <stdint.h>

#define APP_SCREEN_W        480
#define APP_SCREEN_H        800
#define APP_STATUS_H        60

#define APP_FONT_HANDLE     31

EVEWindow *app_root(void);
void app_root_refresh(void);

void app_root_init(eve_view_constructor_t home_page);
