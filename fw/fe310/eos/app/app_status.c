#include <stdlib.h>

#include "unicode.h"

#include "eve/eve.h"
#include "eve/eve_kbd.h"

#include "eve/screen/screen.h"
#include "eve/screen/window.h"
#include "eve/screen/view.h"

#include "app_status.h"

int app_status_touch(EVEView *view, EVETouch *touch, uint16_t evt, uint8_t tag0) {
    return 0;
}

uint8_t app_status_draw(EVEView *view, uint8_t tag0) {
    return tag0;
}
