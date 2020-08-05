#include <stdint.h>

typedef struct EVELabel {
    EVERect g;
    char *title;
    EVEFont *font;
} EVELabel;

void eve_label_init(EVELabel *label, EVERect *g, EVEFont *font, char *title);
void eve_label_draw(EVELabel *label);