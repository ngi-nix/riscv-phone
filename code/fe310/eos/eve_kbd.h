#include <stdint.h>

typedef void (*eos_kbd_fptr_t) (int);

typedef struct EOSKbd {
    uint32_t mem_addr;
    uint16_t mem_size;
    uint8_t key_count;
    uint8_t key_down;
    uint8_t key_modifier;
    uint8_t key_modifier_sticky;
    uint8_t key_modifier_lock;
    eos_kbd_fptr_t key_down_f;
} EOSKbd;

void eos_kbd_init(EOSKbd *kbd, eos_kbd_fptr_t key_down_f, uint32_t mem_addr, uint32_t *mem_next);
void eos_kbd_draw(EOSKbd *kbd, uint8_t tag0, int touch_idx);
void eos_kbd_update(EOSKbd *kbd);