#include <stdint.h>

struct EVETouch;
struct EVEWidget;

typedef struct EVERect {
    uint16_t x1;
    uint16_t y1;
    uint16_t x2;
    uint16_t y2;
} EVERect;

typedef struct EVEScreen {
    char ro;
    EVERect *visible;
    void (*handle_evt) (struct EVEScreen *, struct EVEWidget *, uint8_t, int, struct EVETouch *, uint16_t);
    void (*move) (struct EVEScreen *, struct EVEWidget *, EVERect *);
} EVEScreen;
