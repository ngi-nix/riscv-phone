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

#define PWM_CFG           0x00
#define PWM_COUNT         0x08

#define PWM_CFG_CMP0IP    0x10000000
#define PWM_CFG_CMP1IP    0x20000000
#define PWM_CFG_CMP2IP    0x40000000
#define PWM_CFG_CMP3IP    0x80000000

#define GPIO_CTRL_ADDR    0x10012000
#define GPIO_INPUT_VAL    0x00

#define INT_PWM0_BASE     40
#define INT_PWM1_BASE     44
#define INT_PWM2_BASE     48

#define I2S_PWM_CTRL_ADDR_CK  PWM1_CTRL_ADDR
#define I2S_PWM_CTRL_ADDR_WS  PWM2_CTRL_ADDR

#include "i2s_def.h"
#include "evt_def.h"
#include "msgq_def.h"

  .section      .data.entry
  .align 2

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
  beq x9, x18, handler_sd
  li x18, I2S_IRQ_CK_ID
  beq x9, x18, handler_ck
  li x18, I2S_IRQ_WS_ID
  beq x9, x18, handler_ws
  li x18, I2S_IRQ_CI_ID
  beq x9, x18, handler_ci
  j handler

handler_sd:
  # read mic value -> x8
  li x18, GPIO_CTRL_ADDR
  lw x8, GPIO_INPUT_VAL(x18)

  # set bit as ws pwm counter/ck period - sd volume -> x9
  li x18, I2S_PWM_CTRL_ADDR_WS
  lw x9, PWM_COUNT(x18)
  la x18, _eos_i2s_ck_period
  lw x19, 0(x18)
  divu x9, x9, x19
  la x18, _eos_i2s_mic_volume
  lw x19, 0(x18)
  sub x9, x9, x19
  
  # disable sd irq
  li x18, PLIC_PRIORITY
  sw x0, 4*I2S_IRQ_SD_ID(x18)
  
  addi sp, sp, -1*REGBYTES
  STORE x20, 0*REGBYTES(sp)

  # bit <= 0 -> exit
  bge x0, x9, handler_sd_exit

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

  # bit < I2S_SMPL_WIDTH -> exit
  li x18, I2S_SMPL_WIDTH
  blt x9, x18, handler_sd_exit

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
  la x18, sd_sample_save
  li x19, 0x80000000
  srai x19, x19, I2S_SMPL_WIDTH-1
  and x20, x20, x19
  beq x19, x20, 0f
  lw x8, 0(x18)
0:
  sw x8, 0(x18)
  srai x8, x8, 32-I2S_SMPL_WIDTH

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
  beq x18, x20, handler_sd_exit

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
  beqz x20, handler_sd_exit
  
  la x9, _eos_i2s_mic_buf
  lhu x18, I2S_ABUF_OFF_IDXR(x9)
  sub x18, x19, x18
  blt x18, x20, handler_sd_exit

  la x9, _eos_i2s_mic_rd
  lw x18, 0(x9)
  beqz x18, handler_sd_exit
  sw x0, 0(x9)

  # push to event queue
  la x9, _eos_event_q
  lbu x18, MSGQ_OFF_IDXR(x9)
  lbu x19, MSGQ_OFF_IDXW(x9)
  lbu x20, MSGQ_OFF_SIZE(x9)

  sub x18, x19, x18
  beq x18, x20, handler_sd_exit

  addi x20, x20, -1
  and x20, x20, x19
  li x18, MSGQ_ITEM_SIZE
  mul x20, x20, x18
  lw x18, MSGQ_OFF_ARRAY(x9)
  add x20, x20, x18
  li x18, (EOS_EVT_AUDIO | I2S_EVT_MIC)
  sb x18, 0(x20)
  
  addi x19, x19, 1
  sb x19, MSGQ_OFF_IDXW(x9)
  j handler_sd_exit
  
handler_sd_exit:
  LOAD x20, 0*REGBYTES(sp)
  addi sp, sp, 1*REGBYTES

  # complete
  li x18, I2S_IRQ_SD_ID
  li x19, PLIC_CLAIM
  sw x18, 0(x19)

  # exit
  j trap_exit_data
  
handler_ck:
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
  
handler_ws:
  # enable wsc irq
  li x18, PLIC_PRIORITY
  li x19, I2S_IRQ_CI_PRIORITY
  sw x19, 4*I2S_IRQ_CI_ID(x18)

  # complete
  li x18, I2S_IRQ_WS_ID
  li x19, PLIC_CLAIM
  sw x18, 0(x19)

  # exit
  j trap_exit_data

handler_ci:
  # disable wsc/enable ck irq
  li x18, PLIC_PRIORITY
  sw x0, 4*I2S_IRQ_CI_ID(x18)
  li x19, I2S_IRQ_CK_PRIORITY
  sw x19, 4*I2S_IRQ_CK_ID(x18)

  # set irq mask
  li x18, PLIC_THRESHOLD
  li x19, I2S_IRQ_MASK
  sw x19, 0(x18)
  
  # complete
  li x18, I2S_IRQ_CI_ID
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
