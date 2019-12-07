#include <stdint.h>

#define EOS_UART_ETYPE_TX       1
#define EOS_UART_ETYPE_RX       2

#define EOS_UART_MAX_ETYPE      2

typedef void (*eos_uart_fptr_t) (unsigned char);

void eos_uart_init(void);
void eos_uart_set_handler(unsigned char type, eos_uart_fptr_t handler, uint8_t flags);

void eos_uart_txwm_set(uint8_t wm);
void eos_uart_txwm_clear(void);
void eos_uart_rxwm_set(uint8_t wm);
void eos_uart_rxwm_clear(void);
int eos_uart_putc(int c, char b);
int eos_uart_getc(char b);