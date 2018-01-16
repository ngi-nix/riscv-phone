#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include "encoding.h"
#include "platform.h"
#include "plic/plic_driver.h"
#include "interrupt.h"

// Global Instance data for the PLIC
// for use by the PLIC Driver.
static plic_instance_t plic;

static eos_intr_fptr_t ext_interrupt_handler[PLIC_NUM_INTERRUPTS];

void handle_m_ext_interrupt(void) {
    plic_source int_num  = PLIC_claim_interrupt(&plic);
    if ((int_num >=1 ) && (int_num < PLIC_NUM_INTERRUPTS) && (ext_interrupt_handler[int_num])) {
        ext_interrupt_handler[int_num]();
    } else {
        printf("handle_m_ext_interrupt error\n\n");
        exit(1 + (uintptr_t) int_num);
    }
    PLIC_complete_interrupt(&plic, int_num);
}

void eos_intr_init(void) {
    for (int i = 0; i < PLIC_NUM_INTERRUPTS; i++){
        ext_interrupt_handler[i] = NULL;
    }

    /**************************************************************************
    * Set up the PLIC
    **************************************************************************/
    PLIC_init(&plic,
 	    PLIC_CTRL_ADDR,
 	    PLIC_NUM_INTERRUPTS,
 	    PLIC_NUM_PRIORITIES);

    // Enable Global (PLIC) interrupts.
    set_csr(mie, MIP_MEIP);

    // Enable all interrupts
    set_csr(mstatus, MSTATUS_MIE);
}

void eos_intr_set_handler(uint8_t int_num, uint8_t priority, eos_intr_fptr_t handler) {
    ext_interrupt_handler[int_num] = handler;
    PLIC_enable_interrupt(&plic, int_num);
    PLIC_set_priority(&plic, int_num, priority);
}
