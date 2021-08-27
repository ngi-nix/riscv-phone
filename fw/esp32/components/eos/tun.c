#include <string.h>
#include <stdio.h>

#include <lwip/pbuf.h>
#include <lwip/netif.h>
#include <lwip/tcpip.h>
#include <lwip/etharp.h>

#include "app.h"
#include "tun.h"

static ip4_addr_t ipaddr, netmask, gw;
static struct netif netif_tun;

static err_t ESP_IRAM_ATTR tun_output(struct netif *netif, struct pbuf *p, const struct ip4_addr *ipaddr) {
    unsigned char *buf;
    struct pbuf *q;

    for (q = p; q != NULL; q = q->next) {
        if (q->len > EOS_APP_MTU) continue;

        buf = eos_app_alloc();
        memcpy(buf, q->payload, q->len);
        eos_app_send(EOS_APP_MTYPE_TUN, buf, q->len);
    }

    return ERR_OK;
}

static void ESP_IRAM_ATTR tun_input(unsigned char mtype, unsigned char *buffer, uint16_t len) {
    struct netif *netif = &netif_tun;
    struct pbuf *p;
    int rv;

    if (!netif_is_up(netif)) return;

    p = pbuf_alloc(PBUF_RAW, len, PBUF_RAM);
    if (p == NULL) return;
    memcpy(p->payload, buffer, len);
    rv = netif->input(p, netif);
    if (rv != ERR_OK) {
        pbuf_free(p);
    }
}

static err_t tun_init(struct netif *netif) {
    netif->name[0] = 't';
    netif->name[1] = 'n';
    netif->hostname = NULL;
    netif->output = tun_output;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_LINK_UP;
    netif->mtu = 1500;

    return ERR_OK;
}

void eos_tun_init(void) {
    IP4_ADDR(&gw, 0,0,0,0);
    IP4_ADDR(&ipaddr, 192,168,10,2);
    IP4_ADDR(&netmask, 255,255,255,0);

    netif_add(&netif_tun, &ipaddr, &netmask, &gw, NULL, tun_init, tcpip_input);
    netif_set_up(&netif_tun);
    eos_app_set_handler(EOS_APP_MTYPE_TUN, tun_input);
}