#include <stdint.h>

typedef struct EVEText {
    int16_t x;
    int16_t y;
    uint16_t w;
    uint16_t h;
    uint32_t mem_addr;
    uint16_t line_size;
    uint16_t line0;
    int line_top;
    int line_top0;
    uint16_t ch_idx;
    uint16_t transform_a;
    uint16_t transform_e;
    uint8_t tag;
    uint8_t ch_h;
    uint8_t ch_w;
    uint8_t dl_size;
    char dirty;
} EVEText;

void eve_text_init(EVEText *box, int16_t x, int16_t y, uint16_t w, uint16_t h, double scale_x, double scale_y, uint8_t tag, uint16_t line_size, uint32_t mem_addr, uint32_t *mem_next);
int eve_text_touch(EVEText *box, uint8_t tag0, int touch_idx);
uint8_t eve_text_draw(EVEText *box);
int eve_text_putc(EVEText *box, int c);
void eve_text_update(EVEText *box);
void eve_text_newline(EVEText *box);
void eve_text_backspace(EVEText *box);
