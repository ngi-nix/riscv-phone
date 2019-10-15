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

#define SPI0_CTRL_ADDR    0x10014000
#define SPI1_CTRL_ADDR    0x10024000
#include "sifive/devices/spi.h"

#include "evt_def.h"
#include "msgq_def.h"
#include "i2s_def.h"

#define INT_PWM0_BASE     40
#define INT_PWM1_BASE     44
#define INT_PWM2_BASE     48

#define I2S_PWM_CTRL_ADDR_CK      PWM0_CTRL_ADDR
#define I2S_PWM_CTRL_ADDR_WS_MIC  PWM1_CTRL_ADDR
#define I2S_PWM_CTRL_ADDR_WS_SPK  PWM2_CTRL_ADDR

#define IOF_SPI1_SS0      2
#define IOF_SPI1_SS1      8
#define IOF_SPI1_SS2      9
#define IOF_SPI1_SS3      10

#define INT_SPI1_BASE     6
#define INT_GPIO_BASE     8

#include "net_def.h"
#include "spi_def.h"


  .section      .data.entry
  .align 2

.global eos_trap_entry
eos_trap_entry:
  addi sp, sp, -8*REGBYTES
  STORE x8, 0*REGBYTES(sp)
  STORE x9, 1*REGBYTES(sp)
  STORE x18, 2*REGBYTES(sp)
  STORE x19, 3*REGBYTES(sp)
  STORE x20, 4*REGBYTES(sp)
  STORE x21, 5*REGBYTES(sp)
  STORE x22, 6*REGBYTES(sp)
  STORE x23, 7*REGBYTES(sp)

  csrr x8, mcause
  li x18, MCAUSE_EXT
  bne x8, x18, handler
  li x18, PLIC_CLAIM
  lw x9, 0(x18)
  li x18, I2S_IRQ_WS_ID
  beq x9, x18, i2s_handler_ws
  li x18, I2S_IRQ_SD_ID
  beq x9, x18, i2s_handler_sd
  j handler

evtq_push:
  la x9, _eos_event_q
  lbu x18, MSGQ_OFF_IDXR(x9)
  lbu x19, MSGQ_OFF_IDXW(x9)
  lbu x20, MSGQ_OFF_SIZE(x9)
  
  sub x18, x19, x18
  andi x18, x18, 0xff
  beq x18, x20, 0f

  addi x20, x20, -1
  and x20, x20, x19
  li x18, MSGQ_ITEM_SIZE
  mul x20, x20, x18
  lw x21, MSGQ_OFF_ARRAY(x9)
  add x21, x21, x20

  addi x19, x19, 1
  sb x19, MSGQ_OFF_IDXW(x9)
  jalr x0, x22

0:
  mv x20, x0
  jalr x0, x21

i2s_handler_sd:
  # exit if too early
  li x18, I2S_PWM_CTRL_ADDR_WS_SPK
  lw x8, PWM_COUNT(x18)
  lw x9, PWM_CMP2(x18)
  bltu x8, x9, i2s_handler_sd_exit

  # disable sd irq
  li x18, PLIC_PRIORITY
  sw x0, 4*I2S_IRQ_SD_ID(x18)

  # pop from spk buf -> x8
  mv x8, x0
  la x9, _eos_i2s_spk_buf
  lhu x18, I2S_ABUF_OFF_IDXR(x9)
  lhu x19, I2S_ABUF_OFF_IDXW(x9)
  lhu x20, I2S_ABUF_OFF_SIZE(x9)

  beq x18, x19, i2s_handler_sd_xchg

  addi x20, x20, -1
  and x20, x20, x18
  lw x21, I2S_ABUF_OFF_ARRAY(x9)
  add x21, x21, x20
  lbu x8, 0(x21)

  addi x18, x18, 1
  sh x18, I2S_ABUF_OFF_IDXR(x9)
  
  li x21, 0xffff
  sub x18, x19, x18
  and x18, x18, x21
  
  # check for push to event queue
  la x9, _eos_i2s_spk_wm
  lw x20, 0(x9)
  beqz x20, i2s_decode
  bgtu x18, x20, i2s_decode

  la x9, _eos_i2s_spk_evt_enable
  lw x18, 0(x9)
  beqz x18, i2s_decode
  sw x0, 0(x9)

  # push to event queue
  jal x22, evtq_push
  beqz x21, i2s_decode
  li x18, (EOS_EVT_AUDIO | I2S_ETYPE_SPK)
  sb x18, MSGQ_ITEM_OFF_TYPE(x21)
 
i2s_decode:
  # aLaw decode -> x8
  xori x8, x8, 0x55
  andi x9, x8, 0x80
  beqz x9, 0f
  li x9, 1
  slli x9, x9, 7
  not x9, x9
  and x8, x8, x9
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
  beqz x9, i2s_handler_sd_xchg
  mul x8, x8, x9

i2s_handler_sd_xchg:
  # slli x8, x8, 4
  # li x8, 0xAf0A
  # li x8, 0x50f5
  # read/write shift reg: x8 -> sr -> x8
  li x18, GPIO_CTRL_ADDR
  li x19, (0x1 << I2S_PIN_SD_IN)
  li x20, (0x1 << I2S_PIN_SD_OUT)
  li x21, (0x1 << I2S_PIN_CK_SR)
  lw x22, GPIO_OUTPUT_VAL(x18)

  lw x9, GPIO_OUTPUT_EN(x18)
  or x9, x9, x20
  sw x9, GPIO_OUTPUT_EN(x18)

  not x20, x20
  or x22, x22, x21

  li x23, 17
0:
  addi x23, x23, -1
  beqz x23, 1f
  lw x9, GPIO_INPUT_VAL(x18)
  and x9, x9, x19
  srli x9, x9, I2S_PIN_SD_IN
  slli x8, x8, 1
  or x8, x8, x9

1:
  # idle
  li x9, I2S_IDLE_CYCLES
2:
  addi x9, x9, -1
  bnez x9, 2b

  # ck high
  xor x22, x22, x21
  sw x22, GPIO_OUTPUT_VAL(x18)

  li x9, 0x1
  slli x9, x9, 16
  and x9, x9, x8
#if I2S_PIN_SD_OUT > 16
  slli x9, x9, (I2S_PIN_SD_OUT - 16)
#else
  srli x9, x9, (16 - I2S_PIN_SD_OUT)
#endif
  and x22, x22, x20
  or x22, x22, x9

  # idle
  li x9, I2S_IDLE_CYCLES
2:
  addi x9, x9, -1
  bnez x9, 2b

  # ck low
  xor x22, x22, x21
  sw x22, GPIO_OUTPUT_VAL(x18)

  beqz x23, 3f
  j 0b

3:
  slli x8, x8, 16
  srai x8, x8, 16

  lw x9, GPIO_OUTPUT_EN(x18)
  and x9, x9, x20
  sw x9, GPIO_OUTPUT_EN(x18)

i2s_encode:
  # aLaw encode -> x8
  li x18, 0x800
  li x19, 7
  bgez x8, 0f
  neg x8, x8
  lui x9, 0x80000
  or x8, x8, x9
0:
  and x9, x8, x18
  beq x9, x18, 1f
  beqz x19, 1f
  srli x18, x18, 1
  addi x19, x19, -1
  j 0b
1:
  mv x9, x19
  bnez x9, 2f
  addi x9, x9, 1
2:
  sra x8, x8, x9
  li x9, 0x8000000f
  and x8, x8, x9
  slli x19, x19, 4
  or x8, x8, x19
  bgez x8, 3f
  ori x8, x8, 0x80
3:
  xori x8, x8, 0x55
  andi x8, x8, 0xff

  # push to mic buf
  la x9, _eos_i2s_mic_buf
  lhu x18, I2S_ABUF_OFF_IDXR(x9)
  lhu x19, I2S_ABUF_OFF_IDXW(x9)
  lhu x20, I2S_ABUF_OFF_SIZE(x9)
  li x21, 0xffff

  sub x18, x19, x18
  and x18, x18, x21
  beq x18, x20, i2s_handler_sd_exit

  addi x20, x20, -1
  and x20, x20, x19
  lw x21, I2S_ABUF_OFF_ARRAY(x9)
  add x20, x21, x20

  addi x19, x19, 1
  sh x19, I2S_ABUF_OFF_IDXW(x9)

  sb x8, 0(x20)
  addi x18, x18, 1

  # check for push to event queue
  la x9, _eos_i2s_mic_wm
  lw x20, 0(x9)
  beqz x20, i2s_handler_sd_exit
  bltu x18, x20, i2s_handler_sd_exit

  la x9, _eos_i2s_mic_evt_enable
  lw x18, 0(x9)
  beqz x18, i2s_handler_sd_exit
  sw x0, 0(x9)

  # push to event queue
  jal x22, evtq_push
  beqz x21, i2s_handler_sd_exit
  li x18, (EOS_EVT_AUDIO | I2S_ETYPE_MIC)
  sb x18, MSGQ_ITEM_OFF_TYPE(x21)

i2s_handler_sd_exit:
  # complete
  li x18, I2S_IRQ_SD_ID
  li x19, PLIC_CLAIM
  sw x18, 0(x19)

  # exit
  j trap_exit_data

i2s_handler_ws:
  # enable sd irq
  li x18, PLIC_PRIORITY
  li x19, I2S_IRQ_SD_PRIORITY
  sw x19, 4*I2S_IRQ_SD_ID(x18)

  # complete
  li x18, I2S_IRQ_WS_ID
  li x19, PLIC_CLAIM
  sw x18, 0(x19)

  # exit
  j trap_exit_data

.global _eos_set_pwm
_eos_set_pwm:
  addi sp, sp, -8*REGBYTES
  STORE x8, 0*REGBYTES(sp)
  STORE x9, 1*REGBYTES(sp)
  STORE x18, 2*REGBYTES(sp)
  STORE x19, 3*REGBYTES(sp)
  STORE x20, 4*REGBYTES(sp)
  STORE x21, 5*REGBYTES(sp)
  STORE x22, 6*REGBYTES(sp)
  STORE x23, 7*REGBYTES(sp)

  li x18, I2S_PWM_CTRL_ADDR_CK
  li x19, I2S_PWM_CTRL_ADDR_WS_MIC
  li x20, I2S_PWM_CTRL_ADDR_WS_SPK
  li x21, PWM_CFG_ENALWAYS | PWM_CFG_ZEROCMP | I2S_PWM_SCALE_CK
  li x22, PWM_CFG_ENALWAYS | PWM_CFG_ZEROCMP | PWM_CFG_CMP2GANG
  li x23, PWM_CFG_ENALWAYS | PWM_CFG_ZEROCMP
  sw x21, PWM_CFG(x18)
  sw x22, PWM_CFG(x19)
  sw x23, PWM_CFG(x20)

  LOAD x8, 0*REGBYTES(sp)
  LOAD x9, 1*REGBYTES(sp)
  LOAD x18, 2*REGBYTES(sp)
  LOAD x19, 3*REGBYTES(sp)
  LOAD x20, 4*REGBYTES(sp)
  LOAD x21, 5*REGBYTES(sp)
  LOAD x22, 6*REGBYTES(sp)
  LOAD x23, 7*REGBYTES(sp)
  addi sp, sp, 8*REGBYTES

  ret

.global eos_flash_set
eos_flash_set:
  li a3, SPI0_CTRL_ADDR
  sw x0, SPI_REG_FCTRL(a3)
  sw a0, SPI_REG_SCKDIV(a3)
  sw a1, SPI_REG_FFMT(a3)
  li a0, 1
  sw a0, SPI_REG_FCTRL(a3)
  ret

trap_exit_data:
  # Remain in M-mode after mret
  li x18, MSTATUS_MPP
  csrs mstatus, x18

  LOAD x8, 0*REGBYTES(sp)
  LOAD x9, 1*REGBYTES(sp)
  LOAD x18, 2*REGBYTES(sp)
  LOAD x19, 3*REGBYTES(sp)
  LOAD x20, 4*REGBYTES(sp)
  LOAD x21, 5*REGBYTES(sp)
  LOAD x22, 6*REGBYTES(sp)
  LOAD x23, 7*REGBYTES(sp)
  addi sp, sp, 8*REGBYTES

  mret

handler:
  lui x18,       %hi(trap_entry_text)
  addi x18, x18, %lo(trap_entry_text)
  jalr x0, x18

  .section      .text.entry
  .align 2

trap_entry_text:
  addi sp, sp, -24*REGBYTES

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
  STORE x24, 15*REGBYTES(sp)
  STORE x25, 16*REGBYTES(sp)
  STORE x26, 17*REGBYTES(sp)
  STORE x27, 18*REGBYTES(sp)
  STORE x28, 19*REGBYTES(sp)
  STORE x29, 20*REGBYTES(sp)
  STORE x30, 21*REGBYTES(sp)
  STORE x31, 22*REGBYTES(sp)

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
  LOAD x24, 15*REGBYTES(sp)
  LOAD x25, 16*REGBYTES(sp)
  LOAD x26, 17*REGBYTES(sp)
  LOAD x27, 18*REGBYTES(sp)
  LOAD x28, 19*REGBYTES(sp)
  LOAD x29, 20*REGBYTES(sp)
  LOAD x30, 21*REGBYTES(sp)
  LOAD x31, 22*REGBYTES(sp)

  LOAD x8, 24*REGBYTES(sp)
  LOAD x9, 25*REGBYTES(sp)
  LOAD x18, 26*REGBYTES(sp)
  LOAD x19, 27*REGBYTES(sp)
  LOAD x20, 28*REGBYTES(sp)
  LOAD x21, 29*REGBYTES(sp)
  LOAD x22, 30*REGBYTES(sp)
  LOAD x23, 31*REGBYTES(sp)

  addi sp, sp, 32*REGBYTES
  mret
