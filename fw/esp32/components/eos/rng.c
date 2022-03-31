#include <esp_system.h>
#include <esp_log.h>
#include <esp_err.h>

#include "net.h"
#include "rng.h"

static const char *TAG = "EOS RNG";

static void rng_handler(unsigned char type, unsigned char *buffer, uint16_t buf_len) {
    uint16_t rng_len = 0;

    if (buf_len < sizeof(uint16_t)) goto rng_handler_fin;

    rng_len  = (uint16_t)buffer[0] << 8;
    rng_len |= (uint16_t)buffer[1];
    if (rng_len > EOS_NET_MTU) {
        rng_len = 0;
        goto rng_handler_fin;
    }
    esp_fill_random(buffer, rng_len);

rng_handler_fin:
    eos_net_reply(EOS_NET_MTYPE_RNG, buffer, rng_len);
}

void eos_rng_init(void) {
    eos_net_set_handler(EOS_NET_MTYPE_RNG, rng_handler);
    ESP_LOGI(TAG, "INIT");
}

