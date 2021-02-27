#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <eos.h>
#include <net.h>
#include <cell.h>

#include "status.h"
#include "cell_dev.h"

static void cell_dev_handler(unsigned char type, unsigned char *buffer, uint16_t len) {
    switch (type) {
        case EOS_CELL_MTYPE_READY:
            app_status_msg_set("Modem ready", 1);
            break;
    }
    eos_net_free(buffer, 0);
}

void app_cell_dev_init(void) {
    eos_cell_set_handler(EOS_CELL_MTYPE_DEV, cell_dev_handler);
}
