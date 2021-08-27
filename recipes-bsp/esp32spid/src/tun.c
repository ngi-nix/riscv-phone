#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <linux/if.h>
#include <linux/if_tun.h>

#include <pthread.h>

#include "spi.h"
#include "tun.h"

static pthread_t read_thd;

static int tun_fd;
static char tun_name[IFNAMSIZ];

static int tun_alloc(char *dev, int flags) {
    struct ifreq ifr;
    int fd, err;
    char *clonedev = "/dev/net/tun";

    /* Arguments taken by the function:
     *
     * char *dev: the name of an interface (or '\0'). MUST have enough
     *   space to hold the interface name if '\0' is passed
     * int flags: interface flags (eg, IFF_TUN etc.)
     */

    fd = open(clonedev, O_RDWR);
    if (fd < 0) {
        return fd;
    }

    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = flags;  /* IFF_TUN or IFF_TAP, plus maybe IFF_NO_PI */

    if (*dev) {
        /* if a device name was specified, put it in the structure; otherwise,
         * the kernel will try to allocate the "next" device of the
         * specified type */
        strncpy(ifr.ifr_name, dev, IFNAMSIZ);
    }

    /* try to create the device */
    err = ioctl(fd, TUNSETIFF, (void *) &ifr);
    if (err < 0) {
        close(fd);
        return err;
    }

    /* if the operation was successful, write back the name of the
     * interface to the variable "dev", so the caller can know
     * it. Note that the caller MUST reserve space in *dev (see calling
     * code below) */
    strcpy(dev, ifr.ifr_name);

    /* this is the special file descriptor that the caller will use to talk
     * with the virtual interface */
    return fd;
}

void *tun_read(void *arg) {
    unsigned char *buffer;
    int len;

    while (1) {
        buffer = spi_alloc();
        if (buffer == NULL) continue;
        len = read(tun_fd, buffer + SPI_SIZE_HDR, SPI_SIZE_BUF - SPI_SIZE_HDR);
        if (len < 0) {
            perror("tun read");
            continue;
        }
        spi_xchg(SPI_MTYPE_TUN, buffer, len);
    }
}

int tun_write(unsigned char *buffer, uint16_t len) {
    return write(tun_fd, buffer, len);
}

int tun_init(void) {
    int rv;

    strcpy(tun_name, "tun0");
    tun_fd = tun_alloc(tun_name, IFF_TUN | IFF_NO_PI);
    if (tun_fd < 0) return TUN_ERR;

    return TUN_OK;
}
