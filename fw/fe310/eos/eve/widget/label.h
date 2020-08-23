#include <stdint.h>

typedef struct EVELabel {
    EVERect g;
    EVEFont *font;
    char *title;
} EVELabel;

void eve_label_init(EVELabel *label, EVERect *g, EVEFont *font, char *title);
void eve_label_draw(EVELabel *label);