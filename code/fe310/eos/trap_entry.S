#include "encoding.h"
#include "sifive/bits.h"

#define MCAUSE_INT        0x80000000
#define MCAUSE_EXT        (MCAUSE_INT | IRQ_M_EXT)
#define MCAUSE_TIMER      (MCAUSE_INT | IRQ_M_TIMER)

#define PLIC_PRIORITY     0x0C000000
#define PLIC_THRESHOLD    0x0C200000
#define PLIC_CLAIM        0x0C200004

#define PWM0_CTRL_ADDR    0x10015000
#define PWM1_CTRL_ADDR    0x10025000
#define PWM2_CTRL_ADDR    0x10035000
#include "sifive/devices/pwm.h"

#define GPIO_CTRL_ADDR    0x10012000
#include "sifive/devices/gpio.h"

#define SPI1_CTRL_ADDR    0x10024000
#include "sifive/devices/spi.h"

#include "evt_def.h"
#include "msgq_def.h"
#include "i2s_def.h"

#define INT_PWM0_BASE     40
#define INT_PWM1_BASE     44
#define INT_PWM2_BASE     48

#define I2S_PWM_CTRL_ADDR_CK  PWM0_CTRL_ADDR
#define I2S_PWM_CTRL_ADDR_WS  PWM2_CTRL_ADDR

#include "net_def.h"
#include "spi_def.h"

#define INT_SPI1_BASE     6
#define INT_GPIO_BASE     8

#define IOF_SPI1_SS2      9

  .section      .data.entry
  .align 2

evtq_push:
  la x9, _eos_event_q
  lbu x18, MSGQ_OFF_IDXR(x9)
  lbu x19, MSGQ_OFF_IDXW(x9)
  lbu x8, MSGQ_OFF_SIZE(x9)

  sub x18, x19, x18
  beq x18, x8, 1f

  addi x8, x8, -1
  and x8, x8, x19
  li x18, MSGQ_ITEM_SIZE
  mul x8, x8, x18
  lw x18, MSGQ_OFF_ARRAY(x9)
  add x8, x8, x18

  addi x19, x19, 1
  sb x19, MSGQ_OFF_IDXW(x9)
  jalr x0, x20

1:  
  mv x8, x0
  jalr x0, x20

.global eos_trap_entry
eos_trap_entry:
  addi sp, sp, -4*REGBYTES
  STORE x8, 0*REGBYTES(sp)
  STORE x9, 1*REGBYTES(sp)
  STORE x18, 2*REGBYTES(sp)
  STORE x19, 3*REGBYTES(sp)

  csrr x8, mcause
  li x18, MCAUSE_EXT
  bne x8, x18, handler
  li x18, PLIC_CLAIM
  lw x9, 0(x18)
  li x18, I2S_IRQ_SD_ID
  beq x9, x18, i2s_handler_sd
  li x18, I2S_IRQ_CK_ID
  beq x9, x18, i2s_handler_ck
  li x18, I2S_IRQ_WS_ID
  beq x9, x18, i2s_handler_ws
  li x18, I2S_IRQ_CI_ID
  beq x9, x18, i2s_handler_ci
  li x18, INT_SPI1_BASE
  beq x9, x18, spi_handler_xchg
  li x18, INT_GPIO_BASE + SPI_PIN_CTS
  beq x9, x18, spi_handler_cts
  li x18, INT_GPIO_BASE + SPI_PIN_RTS
  beq x9, x18, spi_handler_rts
  j handler

i2s_handler_sd:
  # exit if too early
  li x18, I2S_PWM_CTRL_ADDR_CK
  lw x8, PWM_COUNT(x18)
  lw x9, PWM_CMP2(x18)
  srli x9, x9, I2S_PWM_SCALE_CK
  bltu x8, x9, i2s_handler_sd_exit_

  # read mic value -> x8
  li x18, GPIO_CTRL_ADDR
  lw x8, GPIO_INPUT_VAL(x18)

  # set bit as ws pwm counter/(ck period + 1) - sd volume -> x9
  li x18, I2S_PWM_CTRL_ADDR_WS
  lw x9, PWM_COUNT(x18)
  la x18, _eos_i2s_ck_period
  lw x19, 0(x18)
  addi x19, x19, 1
  divu x9, x9, x19
  la x18, _eos_i2s_mic_volume
  lw x19, 0(x18)
  sub x9, x9, x19
  
  # disable sd irq
  li x18, PLIC_PRIORITY
  sw x0, 4*I2S_IRQ_SD_ID(x18)
  
  # store x20
  addi sp, sp, -1*REGBYTES
  STORE x20, 0*REGBYTES(sp)

  # bit <= 0 or bit > 32 -> exit
  li x18, 32
  bge x0, x9, i2s_handler_sd_exit
  bge x9, x18, i2s_handler_sd_exit

  # sample |= sd gpio value << (32 - bit) -> x8
  # sample_mask |= 1 << (32 - bit) -> x20
  li x18, (0x1 << I2S_PIN_SD)
  and x8, x8, x18
  srli x8, x8, I2S_PIN_SD
  li x18, 32
  sub x19, x18, x9
  sll x8, x8, x19
  li x20, 1
  sll x20, x20, x19
  la x18, sd_sample_mask
  lw x19, 0(x18)
  or x19, x19, x20
  sw x19, 0(x18)
  mv x20, x19
  la x18, sd_sample
  lw x19, 0(x18)
  or x19, x19, x8
  sw x19, 0(x18)
  mv x8, x19

  # bit < I2S_SMPL_BITS -> exit
  li x18, I2S_SMPL_BITS
  blt x9, x18, i2s_handler_sd_exit

  # disable ck irq
  li x18, PLIC_PRIORITY
  sw x0, 4*I2S_IRQ_CK_ID(x18)

  # reset irq mask
  li x18, PLIC_THRESHOLD
  sw x0, 0(x18)

  # reset sample / sample_mask
  la x18, sd_sample
  sw x0, 0(x18)
  la x18, sd_sample_mask
  sw x0, 0(x18)
  
  # correct for missed bits
  # la x18, sd_sample_save
  # don't correct
  # j 0f
  li x19, 0x80000000
  srai x19, x19, I2S_SMPL_BITS_S-1
  and x20, x20, x19
  beq x19, x20, 0f
  # drop missing
  # bne x19, x20, i2s_handler_sd_exit
  # lw x8, 0(x18)
  # zero missing frame
  mv x8, x0
0:
  # sw x8, 0(x18)
  srai x8, x8, 32-I2S_SMPL_BITS

  # aLaw encode -> x8
  li x18, 0x800
  li x19, 7
  bgez x8, 1f
  neg x8, x8
  lui x9, 0x80000
  or x8, x8, x9
1:
  and x9, x8, x18
  beq x9, x18, 2f
  beqz x19, 2f
  srli x18, x18, 1
  addi x19, x19, -1
  j 1b
2:
  mv x9, x19
  bnez x9, 3f
  addi x9, x9, 1
3:
  sra x8, x8, x9
  li x9, 0x8000000f
  and x8, x8, x9
  slli x19, x19, 4
  or x8, x8, x19
  bgez x8, 4f
  ori x8, x8, 0x80
  
4:
  xori x8, x8, 0x55
  andi x8, x8, 0xff

  # push to mic buf
  la x9, _eos_i2s_mic_buf
  lhu x18, I2S_ABUF_OFF_IDXR(x9)
  lhu x19, I2S_ABUF_OFF_IDXW(x9)
  lhu x20, I2S_ABUF_OFF_SIZE(x9)

  sub x18, x19, x18
  beq x18, x20, i2s_handler_sd_exit

  addi x20, x20, -1
  and x20, x20, x19
  lw x18, I2S_ABUF_OFF_ARRAY(x9)
  add x20, x20, x18
  sb x8, 0(x20)
  
  addi x19, x19, 1
  sh x19, I2S_ABUF_OFF_IDXW(x9)

  # check for push to event queue
  la x9, _eos_i2s_mic_wm
  lw x20, 0(x9)
  beqz x20, i2s_handler_sd_exit
  
  la x9, _eos_i2s_mic_buf
  lhu x18, I2S_ABUF_OFF_IDXR(x9)
  sub x18, x19, x18
  bltu x18, x20, i2s_handler_sd_exit

  la x9, _eos_i2s_mic_rd
  lw x18, 0(x9)
  beqz x18, i2s_handler_sd_exit
  sw x0, 0(x9)

  # push to event queue
  jal x20, evtq_push
  beqz x8, i2s_handler_sd_exit
  li x18, (EOS_EVT_AUDIO | I2S_EVT_MIC)
  sb x18, MSGQ_ITEM_OFF_CMD(x8)
  
i2s_handler_sd_exit:
  LOAD x20, 0*REGBYTES(sp)
  addi sp, sp, 1*REGBYTES

i2s_handler_sd_exit_:
  # complete
  li x18, I2S_IRQ_SD_ID
  li x19, PLIC_CLAIM
  sw x18, 0(x19)

  # exit
  j trap_exit_data
  
i2s_handler_ck:
  # enable sd irq
  li x18, PLIC_PRIORITY
  li x19, I2S_IRQ_SD_PRIORITY
  sw x19, 4*I2S_IRQ_SD_ID(x18)

  # complete
  li x18, I2S_IRQ_CK_ID
  li x19, PLIC_CLAIM
  sw x18, 0(x19)

  # exit
  j trap_exit_data
  
i2s_handler_ws:
  # enable ci irq
  li x18, PLIC_PRIORITY
  li x19, I2S_IRQ_CI_PRIORITY
  sw x19, 4*I2S_IRQ_CI_ID(x18)

  # complete
  li x18, I2S_IRQ_WS_ID
  li x19, PLIC_CLAIM
  sw x18, 0(x19)

  # exit
  j trap_exit_data

i2s_handler_ci:
  # exit if too early
  li x18, I2S_PWM_CTRL_ADDR_WS
  lw x8, PWM_COUNT(x18)
  lw x9, PWM_CMP2(x18)
  bltu x8, x9, i2s_handler_ci_exit

  # disable ci/enable ck irq
  li x18, PLIC_PRIORITY
  sw x0, 4*I2S_IRQ_CI_ID(x18)
  li x19, I2S_IRQ_CK_PRIORITY
  sw x19, 4*I2S_IRQ_CK_ID(x18)

  # set irq mask
  li x18, PLIC_THRESHOLD
  li x19, I2S_IRQ_MASK
  sw x19, 0(x18)

  # pop from spk buf -> x8
  la x9, _eos_i2s_spk_buf
  lhu x18, I2S_ABUF_OFF_IDXR(x9)
  lhu x19, I2S_ABUF_OFF_IDXW(x9)
  beq x18, x19, i2s_handler_ci_exit
  lhu x19, I2S_ABUF_OFF_SIZE(x9)

  addi x19, x19, -1
  and x19, x19, x18
  lw x8, I2S_ABUF_OFF_ARRAY(x9)
  add x19, x19, x8
  lbu x8, 0(x19)
  
  addi x18, x18, 1
  sh x18, I2S_ABUF_OFF_IDXR(x9)

#define UART0_CTRL_ADDR 0x10013000
#define UART_REG_TXFIFO 0x00
  # li x18, UART0_CTRL_ADDR
  # sw x8, UART_REG_TXFIFO(x18)
  
  # aLaw decode -> x8
  xori x8, x8, 0x55
  andi x9, x8, 0x80
  beqz x9, 0f
  li x9, 1
  slli x9, x9, 7
  not x9, x9
  and x9, x8, x9
  li x9, -1
0:
  andi x18, x8, 0xf0
  srli x18, x18, 4
  addi x18, x18, 4
  
  li x19, 4
  beq x18, x19, 1f
  
  andi x8, x8, 0x0f
  addi x19, x18, -4
  sll x8, x8, x19
  
  li x19, 1
  sll x19, x19, x18
  or x8, x8, x19
  
  li x19, 1
  addi x18, x18, -5
  sll x19, x19, x18
  or x8, x8, x19  
  j 2f
1:
  slli x8, x8, 1
  ori x8, x8, 1
2:
  beqz x9, 3f
  mul x8, x8, x9
3:
  # END
  # li x18, UART0_CTRL_ADDR
  # mv x9, x8
  # srai x9, x9, 8
  # andi x9, x9, 0xff
  # sw x9, UART_REG_TXFIFO(x18)
  # mv x9, x8
  # andi x9, x9, 0xff
  # sw x9, UART_REG_TXFIFO(x18)
  
i2s_handler_ci_exit:
  # complete
  li x18, I2S_IRQ_CI_ID
  li x19, PLIC_CLAIM
  sw x18, 0(x19)

  # exit
  j trap_exit_data

# x9 - cmd, x18 - buffer, x19 - len, x20 - &flags, x21 - flags
spi_xchg_start:
  ori x21, x21, SPI_FLAG_INIT
  andi x21, x21, ~SPI_FLAG_CTS
  la x22, _eos_spi_state_next_cnt
  lbu x23, 0(x22)
  beqz x23, 1f
  la x22, _eos_spi_state_next_buf
  lw x23, 0(x22)
  bnez x23, 1f
  ori x9, x9, EOS_NET_CMD_FLAG_ONEW
  
1:
  andi x22, x9, EOS_NET_CMD_FLAG_ONEW
  beqz x22, 2f
  ori x21, x21, SPI_FLAG_ONEW
2:
  sw x21, 0(x20)

  la x20, _eos_spi_state_cmd
  sb x9, 0(x20)
  la x20, _eos_spi_state_buf
  sw x18, 0(x20)
  la x20, _eos_spi_state_len_tx
  sw x19, 0(x20)
  la x20, _eos_spi_state_len_rx
  sw x0, 0(x20)
  la x20, _eos_spi_state_idx_tx
  sw x0, 0(x20)
  la x20, _eos_spi_state_idx_rx
  sw x0, 0(x20)

  li x20, SPI1_CTRL_ADDR
  li x21, SPI_CSMODE_HOLD
  sw x21, SPI_REG_CSMODE(x20)

  slli x21, x9, 3
  srli x22, x19, 8
  or x21, x21, x22
  andi x21, x21, 0xff
  sw x21, SPI_REG_TXFIFO(x20)

  andi x21, x19, 0xff
  sw x21, SPI_REG_TXFIFO(x20)

  li x21, 1
  sw x21, SPI_REG_RXCTRL(x20)
  li x21, SPI_IP_RXWM
  sw x21, SPI_REG_IE(x20)
  jalr x0, x8  
  
spi_handler_xchg:
  addi sp, sp, -4*REGBYTES
  STORE x20, 0*REGBYTES(sp)
  STORE x21, 1*REGBYTES(sp)
  STORE x22, 2*REGBYTES(sp)
  STORE x23, 3*REGBYTES(sp)

  li x19, SPI1_CTRL_ADDR
  la x18, _eos_spi_state_flags
  lbu x20, 0(x18)
  andi x8, x20, SPI_FLAG_RST
  beqz x8, 1f

  andi x20, x20, ~SPI_FLAG_RST
  sb x20, 0(x18)
  lw x0, SPI_REG_RXFIFO(x19)
  li x8, SPI_CSMODE_AUTO
  sw x8, SPI_REG_CSMODE(x19)
  sw x0, SPI_REG_IE(x19)
  j spi_handler_xchg_exit

1:
  andi x8, x20, SPI_FLAG_INIT
  beqz x8, 6f

  andi x20, x20, ~SPI_FLAG_INIT
  sb x20, 0(x18)
  li x8, SPI_SIZE_WM
  sw x8, SPI_REG_TXCTRL(x19)
  li x8, SPI_IP_TXWM
  sw x8, SPI_REG_IE(x19)
  lw x8, SPI_REG_RXFIFO(x19)
  lw x9, SPI_REG_RXFIFO(x19)
  andi x8, x8, 0xff
  andi x9, x9, 0xff
  la x18, _eos_spi_state_cmd
  lbu x23, 0(x18)
  andi x23, x23, EOS_NET_CMD_FLAG_ONEW
  beqz x23, 2f
  mv x8, x0
  mv x9, x0
2:
  srli x23, x8, 3
  sb x23, 0(x18)
  andi x22, x8, 0x07
  slli x22, x22, 8
  or x22, x22, x9
  la x18, _eos_spi_state_len_rx
  sw x22, 0(x18)
  la x18, _eos_spi_state_len_tx
  lw x21, 0(x18)
  bgeu x21, x22, 3f
  mv x21, x22

3:
  li x8, 6
  bgeu x21, x8, 4f
  mv x21, x8
  j 5f
4:
  addi x8, x21, 2
  li x9, 4
  remu x18, x8, x9
  beqz x18, 5f
  divu x21, x8, x9
  addi x21, x21, 1
  mul x21, x21, x9
  addi x21, x21, -2

5:
  la x18, _eos_spi_state_len
  sw x21, 0(x18)
  j spi_handler_xchg_exit

6:
  la x18, _eos_spi_state_len
  lw x20, 0(x18)
  la x18, _eos_spi_state_idx_tx
  lw x21, 0(x18)
  la x18, _eos_spi_state_idx_rx
  lw x22, 0(x18)
  la x18, _eos_spi_state_buf
  lw x23, 0(x18)
  
  sub x9, x20, x21
  li x8, SPI_SIZE_CHUNK
  bltu x8, x9, 7f
  mv x8, x9
7:
  addi x8, x8, -1
  add x9, x23, x21

8:  
  bltz x8, 9f
  lw x18, SPI_REG_TXFIFO(x19)
  bltz x18, 9f
  lbu x18, 0(x9)
  sw x18, SPI_REG_TXFIFO(x19)
  addi x8, x8, -1
  addi x9, x9, 1
  addi x21, x21, 1
  j 8b
  
9:
  sub x8, x21, x22
  addi x8, x8, -1
  add x9, x23, x22
  
10:  
  bltz x8, 11f
  lw x18, SPI_REG_RXFIFO(x19)
  bltz x18, 11f
  sb x18, 0(x9)
  addi x8, x8, -1
  addi x9, x9, 1
  addi x22, x22, 1
  j 10b

11:
  la x18, _eos_spi_state_idx_tx
  sw x21, 0(x18)
  la x18, _eos_spi_state_idx_rx
  sw x22, 0(x18)
  
  bne x22, x20, 15f
  li x8, SPI_CSMODE_AUTO
  sw x8, SPI_REG_CSMODE(x19)
  sw x0, SPI_REG_IE(x19)
  la x18, _eos_spi_state_cmd
  lbu x21, 0(x18)
  beqz x21, 12f

  # push to event queue
  jal x20, evtq_push
  beqz x8, 14f
  ori x21, x21, EOS_EVT_NET
  sb x21, MSGQ_ITEM_OFF_CMD(x8)
  sw x23, MSGQ_ITEM_OFF_BUF(x8)
  la x18, _eos_spi_state_len_rx
  lw x22, 0(x18)
  sh x22, MSGQ_ITEM_OFF_SIZE(x8)
  j spi_handler_xchg_exit

12:
  la x18, _eos_spi_state_flags
  lbu x8, 0(x18)
  andi x19, x8, SPI_FLAG_ONEW
  bnez x19, 13f
  la x19, _eos_spi_state_next_cnt
  lbu x9, 0(x19)
  beqz x9, 14f
13:
  la x19, _eos_spi_state_next_buf
  lw x9, 0(x19)
  bnez x9, 14f
  andi x8, x8, ~SPI_FLAG_ONEW
  sb x8, 0(x18)
  sw x23, 0(x19)
  
  j spi_handler_xchg_exit
  
14:
  # push to spi buf queue
  la x19, _eos_spi_buf_q
  lbu x8, SPI_BUFQ_OFF_IDXW(x19)

  andi x9, x8, SPI_SIZE_BUFQ - 1
  slli x9, x9, 2
  add x9, x19, x9
  sw x23, SPI_BUFQ_OFF_ARRAY(x9)

  addi x8, x8, 1
  sb x8, SPI_BUFQ_OFF_IDXW(x19)

  j spi_handler_xchg_exit

15:  
  bne x21, x20, spi_handler_xchg_exit
  sub x8, x20, x22
  addi x8, x8, -1
  li x9, SPI_SIZE_CHUNK - 1
  bltu x8, x9, 16f
  mv x8, x9
16:
  sw x8, SPI_REG_RXCTRL(x19)
  li x9, SPI_IP_RXWM
  sw x9, SPI_REG_IE(x19)
  
spi_handler_xchg_exit:
  LOAD x20, 0*REGBYTES(sp)
  LOAD x21, 1*REGBYTES(sp)
  LOAD x22, 2*REGBYTES(sp)
  LOAD x23, 3*REGBYTES(sp)
  addi sp, sp, 4*REGBYTES

  # complete
  li x18, INT_SPI1_BASE
  li x19, PLIC_CLAIM
  sw x18, 0(x19)

  # exit
  j trap_exit_data

spi_handler_cts:
  addi sp, sp, -4*REGBYTES
  STORE x20, 0*REGBYTES(sp)
  STORE x21, 1*REGBYTES(sp)
  STORE x22, 2*REGBYTES(sp)
  STORE x23, 3*REGBYTES(sp)

  li x8, 1
  slli x8, x8, SPI_PIN_CTS
  li x19, GPIO_CTRL_ADDR
  sw x8, GPIO_RISE_IP(x19)

  la x20, _eos_spi_state_flags
  lw x21, 0(x20)
  ori x21, x21, SPI_FLAG_CTS
  sw x21, 0(x20)
  andi x9, x21, SPI_FLAG_RDY
  beqz x9, 2f
  
  # pop from send q
  la x9, _eos_spi_send_q
  lbu x18, MSGQ_OFF_IDXR(x9)
  lbu x19, MSGQ_OFF_IDXW(x9)
  beq x18, x19, 1f
  lbu x8, MSGQ_OFF_SIZE(x9)

  addi x8, x8, -1
  and x8, x8, x18
  li x19, MSGQ_ITEM_SIZE
  mul x8, x8, x19
  lw x19, MSGQ_OFF_ARRAY(x9)
  add x8, x19, x8
  addi x18, x18, 1
  sb x18, MSGQ_OFF_IDXR(x9)

  lbu x9, MSGQ_ITEM_OFF_CMD(x8)
  lw x18, MSGQ_ITEM_OFF_BUF(x8)
  lhu x19, MSGQ_ITEM_OFF_SIZE(x8)
  beqz x18, 1f
  jal x8, spi_xchg_start
  
  j spi_handler_cts_exit
  
1:
  la x9, _eos_spi_buf_q
  lbu x18, SPI_BUFQ_OFF_IDXR(x9)
  lbu x19, SPI_BUFQ_OFF_IDXW(x9)
  beq x18, x19, spi_handler_cts_exit

  andi x8, x18, SPI_SIZE_BUFQ - 1
  slli x8, x8, 2
  add x8, x9, x8
  addi x18, x18, 1
  sb x18, SPI_BUFQ_OFF_IDXW(x9)
  mv x9, x0
  lw x18, SPI_BUFQ_OFF_ARRAY(x8)
  mv x19, x0
  beqz x18, spi_handler_cts_exit
  jal x8, spi_xchg_start

  j spi_handler_cts_exit

2:
  lw x8, GPIO_IOF_EN(x19)
  li x9, 1
  slli x9, x9, IOF_SPI1_SS2
  not x9, x9
  and x8, x8, x9
  sw x8, GPIO_IOF_EN(x19)

spi_handler_cts_exit:
  LOAD x20, 0*REGBYTES(sp)
  LOAD x21, 1*REGBYTES(sp)
  LOAD x22, 2*REGBYTES(sp)
  LOAD x23, 3*REGBYTES(sp)
  addi sp, sp, 4*REGBYTES

  # complete
  li x18, INT_GPIO_BASE + SPI_PIN_CTS
  li x19, PLIC_CLAIM
  sw x18, 0(x19)

  # exit
  j trap_exit_data

spi_handler_rts:
  li x8, 1
  slli x8, x8, SPI_PIN_RTS
  li x18, GPIO_CTRL_ADDR
  la x19, _eos_spi_state_flags

  lw x9, GPIO_RISE_IP(x18)
  and x9, x9, x8
  beqz x9, 1f

  sw x8, GPIO_RISE_IP(x18)
  lbu x8, 0(x19)
  ori x8, x8, SPI_FLAG_RTS
  sb x8, 0(x19)

  andi x9, x8, SPI_FLAG_RDY
  beqz x9, spi_handler_rts_exit
  andi x9, x8, SPI_FLAG_CTS
  beqz x9, spi_handler_rts_exit

  andi x8, x8, ~SPI_FLAG_CTS
  ori x8, x8, SPI_FLAG_RST
  sb x8, 0(x19)

  li x19, SPI1_CTRL_ADDR
  li x8, SPI_CSMODE_HOLD
  sw x8, SPI_REG_CSMODE(x19)
  sw x0, SPI_REG_TXFIFO(x19)
  sw x0, SPI_REG_RXCTRL(x19)
  li x8, SPI_IP_RXWM
  sw x8, SPI_REG_IE(x19)
  
  j spi_handler_rts_exit

1:
  lw x9, GPIO_FALL_IP(x18)
  and x9, x9, x8
  beqz x9, spi_handler_rts_exit

  sw x8, GPIO_FALL_IP(x18)
  lbu x8, 0(x19)
  andi x8, x8, ~SPI_FLAG_RTS
  sb x8, 0(x19)

spi_handler_rts_exit:
  # complete
  li x18, INT_GPIO_BASE + SPI_PIN_RTS
  li x19, PLIC_CLAIM
  sw x18, 0(x19)

  # exit
  j trap_exit_data

trap_exit_data:
  # Remain in M-mode after mret
  li x18, MSTATUS_MPP
  csrs mstatus, x18

  LOAD x8, 0*REGBYTES(sp)
  LOAD x9, 1*REGBYTES(sp)
  LOAD x18, 2*REGBYTES(sp)
  LOAD x19, 3*REGBYTES(sp)
  addi sp, sp, 4*REGBYTES

  mret

handler:
  lui x18,       %hi(trap_entry_text)
  addi x18, x18, %lo(trap_entry_text)
  jalr x0, x18

  .align 4

sd_sample:
  .word 0
sd_sample_save:
  .word 0
sd_sample_mask:
  .word 0

  .section      .text.entry
  .align 2
  
trap_entry_text:
  addi sp, sp, -28*REGBYTES

  STORE x1, 0*REGBYTES(sp)
  STORE x2, 1*REGBYTES(sp)
  STORE x3, 2*REGBYTES(sp)
  STORE x4, 3*REGBYTES(sp)
  STORE x5, 4*REGBYTES(sp)
  STORE x6, 5*REGBYTES(sp)
  STORE x7, 6*REGBYTES(sp)
  STORE x10, 7*REGBYTES(sp)
  STORE x11, 8*REGBYTES(sp)
  STORE x12, 9*REGBYTES(sp)
  STORE x13, 10*REGBYTES(sp)
  STORE x14, 11*REGBYTES(sp)
  STORE x15, 12*REGBYTES(sp)
  STORE x16, 13*REGBYTES(sp)
  STORE x17, 14*REGBYTES(sp)
  STORE x20, 15*REGBYTES(sp)
  STORE x21, 16*REGBYTES(sp)
  STORE x22, 17*REGBYTES(sp)
  STORE x23, 18*REGBYTES(sp)
  STORE x24, 19*REGBYTES(sp)
  STORE x25, 20*REGBYTES(sp)
  STORE x26, 21*REGBYTES(sp)
  STORE x27, 22*REGBYTES(sp)
  STORE x28, 23*REGBYTES(sp)
  STORE x29, 24*REGBYTES(sp)
  STORE x30, 25*REGBYTES(sp)
  STORE x31, 26*REGBYTES(sp)

  li x18, MCAUSE_TIMER
  beq x8, x18, handle_timer
  li x18, MCAUSE_EXT
  beq x8, x18, handle_ext
  mv a0, x8
  call _exit

handle_timer:
  call eos_timer_handle
  j trap_exit_text

handle_ext:
  mv a0, x9
  call eos_intr_handle
  li x18, PLIC_CLAIM
  sw a0, 0(x18)
  j trap_exit_text
  
trap_exit_text:
  # Remain in M-mode after mret
  li t0, MSTATUS_MPP
  csrs mstatus, t0

  LOAD x1, 0*REGBYTES(sp)
  LOAD x2, 1*REGBYTES(sp)
  LOAD x3, 2*REGBYTES(sp)
  LOAD x4, 3*REGBYTES(sp)
  LOAD x5, 4*REGBYTES(sp)
  LOAD x6, 5*REGBYTES(sp)
  LOAD x7, 6*REGBYTES(sp)
  LOAD x10, 7*REGBYTES(sp)
  LOAD x11, 8*REGBYTES(sp)
  LOAD x12, 9*REGBYTES(sp)
  LOAD x13, 10*REGBYTES(sp)
  LOAD x14, 11*REGBYTES(sp)
  LOAD x15, 12*REGBYTES(sp)
  LOAD x16, 13*REGBYTES(sp)
  LOAD x17, 14*REGBYTES(sp)
  LOAD x20, 15*REGBYTES(sp)
  LOAD x21, 16*REGBYTES(sp)
  LOAD x22, 17*REGBYTES(sp)
  LOAD x23, 18*REGBYTES(sp)
  LOAD x24, 19*REGBYTES(sp)
  LOAD x25, 20*REGBYTES(sp)
  LOAD x26, 21*REGBYTES(sp)
  LOAD x27, 22*REGBYTES(sp)
  LOAD x28, 23*REGBYTES(sp)
  LOAD x29, 24*REGBYTES(sp)
  LOAD x30, 25*REGBYTES(sp)
  LOAD x31, 26*REGBYTES(sp)

  LOAD x8, 28*REGBYTES(sp)
  LOAD x9, 29*REGBYTES(sp)
  LOAD x18, 30*REGBYTES(sp)
  LOAD x19, 31*REGBYTES(sp)

  addi sp, sp, 32*REGBYTES
  mret
