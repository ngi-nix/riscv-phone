#include <stdint.h>

typedef struct EVELabel {
    EVERect g;
    char *title;
    EVEFont *font;
} EVELabel;

void eve_label_init(EVELabel *label, EVERect *g, char *title, EVEFont *font);
void eve_label_draw(EVELabel *label);