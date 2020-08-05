#include <stdint.h>

typedef struct EVEKbdView {
    EVEView v;
    EVEKbd kbd;
} EVEKbdView;

typedef struct EVEKbdWin {
    EVEWindow win;
    EVEKbdView view;
} EVEKbdWin;

void eve_kbdwin_init(EVEKbdWin *kbd_win, EVEScreen *screen);
void eve_kbdwin_append(EVEKbdWin *kbd_win);
