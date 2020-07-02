#include <stdlib.h>
#include <string.h>

#include "eve.h"
#include "eve_kbd.h"

#include "screen/screen.h"
#include "screen/window.h"
#include "screen/page.h"
#include "screen/font.h"

#include "label.h"

void eve_label_init(EVELabel *label, EVERect *g, char *title, EVEFont *font) {
    memset(label, 0, sizeof(EVELabel));
    if (g) label->g = *g;
    label->title = title;
    label->font = font;
    if (label->g.w == 0) label->g.w = eve_font_strw(font, label->title);
    if (label->g.h == 0) label->g.h = eve_font_h(font);
}

void eve_label_draw(EVELabel *label) {
    eve_cmd(CMD_TEXT, "hhhhs", label->g.x, label->g.y, label->font->id, 0, label->title);
}