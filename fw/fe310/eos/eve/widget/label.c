#include <stdlib.h>
#include <string.h>

#include "unicode.h"

#include "eve.h"
#include "eve_kbd.h"
#include "eve_font.h"

#include "screen/screen.h"
#include "screen/window.h"
#include "screen/view.h"
#include "screen/page.h"

#include "label.h"

void eve_label_init(EVELabel *label, EVERect *g, EVEFont *font, char *title) {
    memset(label, 0, sizeof(EVELabel));
    if (g) label->g = *g;
    label->font = font;
    label->title = title;
    if (label->g.h == 0) label->g.h = eve_font_h(font);
}

void eve_label_draw(EVELabel *label) {
    eve_cmd(CMD_TEXT, "hhhhs", label->g.x, label->g.y, label->font->id, 0, label->title);
}