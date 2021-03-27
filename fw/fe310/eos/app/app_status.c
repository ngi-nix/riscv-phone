#include <stdlib.h>

#include "eve/eve.h"
#include "eve/eve_kbd.h"
#include "eve/eve_font.h"

#include "eve/screen/window.h"

#include "app_status.h"

uint8_t app_status_draw(EVEView *view, uint8_t tag0) {
    return eve_view_clear(view, tag0, 0);
}

int app_status_touch(EVEView *view, EVETouch *touch, uint16_t evt, uint8_t tag0) {
    return 0;
}
