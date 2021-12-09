#include <sys/cdefs.h>

#include "encoding.h"
#include "platform.h"
#include "prci_driver.h"

extern void eos_trap_entry();

__attribute__((constructor))
void metal_init(void) {
    SPI0_REG(SPI_REG_SCKDIV) = 8;

    PRCI_use_default_clocks();
    PRCI_use_pll(PLL_REFSEL_HFXOSC, 0, 1, 31, 1, -1, -1, -1);

    write_csr(mtvec, &eos_trap_entry);
    if (read_csr(misa) & (1 << ('F' - 'A'))) { // if F extension is present
        write_csr(mstatus, MSTATUS_FS); // allow FPU instructions without trapping
        write_csr(fcsr, 0); // initialize rounding mode, undefined at reset
    }
}

__attribute__((section(".init")))
void __metal_synchronize_harts() {
}
