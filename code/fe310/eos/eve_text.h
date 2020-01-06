#include <stdint.h>

typedef struct EOSText {
    int x;
    int y;
    int w;
    int h;
    int buf_line_h;
    int buf_idx;
    int line_idx;
    uint32_t buf_addr;
    uint16_t transform_a;
    uint16_t transform_e;
    uint8_t tag;
    uint8_t ch_h;
    uint8_t ch_w;
    uint8_t dl_size;
    char dirty;
} EOSText;

void eos_text_init(EOSText *box, int x, int y, int w, int h, double scale_x, double scale_y, uint8_t tag, int buf_line_h, uint32_t mem_addr, uint32_t *mem_next);
void eos_text_draw(EOSText *box, uint8_t tag0, int touch_idx);
void eos_text_update(EOSText *box, int line_idx);
void eos_text_putc(EOSText *box, int c);
void eos_text_newline(EOSText *box);
void eos_text_backspace(EOSText *box);
