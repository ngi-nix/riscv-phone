#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <linux/spi/spidev.h>

#include <gpiod.h>
#include <pthread.h>

#include "msgq.h"
#include "tun.h"
#include "spi.h"

static pthread_t rtscts_thd;
static pthread_t worker_thd;
static pthread_t handler_thd;
static pthread_mutex_t mutex;

struct gpiod_line *gpio_rts, *gpio_cts;
static struct gpiod_line_bulk gpio_rtscts;

static MSGQueue spi_bufq;
static unsigned char *spi_bufq_array[SPI_SIZE_BUFQ];

static MSGQueue spi_msgq_in;
static unsigned char *spi_msgq_in_array[SPI_SIZE_MSGQ];

static MSGQueue spi_msgq_out;
static unsigned char *spi_msgq_out_array[SPI_SIZE_MSGQ];

static uint32_t spi_speed = 5000000;
static int spi_fd;

static int _spi_xchg(unsigned char *buffer) {
	int rv;
    uint16_t len_tx;
    uint16_t len_rx;
	struct spi_ioc_transfer	tr;

	memset(&tr, 0, sizeof(tr));
	tr.tx_buf = (unsigned long)buffer;
	tr.rx_buf = (unsigned long)buffer;
	tr.speed_hz = spi_speed,

    len_tx  = (uint16_t)buffer[1] << 8;
    len_tx |= (uint16_t)buffer[2] & 0xFF;
    if (len_tx > SPI_MTU) return SPI_ERR;

    len_tx += SPI_SIZE_HDR;
    // esp32 dma workaraund
    if (len_tx < 8) {
        len_tx = 8;
    } else if (len_tx % 4 != 0) {
        len_tx = (len_tx / 4 + 1) * 4;
    }

	tr.len = len_tx;

    pthread_mutex_lock(&mutex);
	rv = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
	if (rv < 0) return SPI_ERR_MSG;

    len_rx  = (uint16_t)buffer[1] << 8;
    len_rx |= (uint16_t)buffer[2] & 0xFF;
    if (len_rx > SPI_MTU) return SPI_ERR;

    len_rx += SPI_SIZE_HDR;
    if (len_rx > len_tx) {
        tr.tx_buf = (unsigned long)NULL;
        tr.rx_buf = (unsigned long)(buffer + len_tx);

        len_tx = len_rx - len_tx;
        // esp32 dma workaraund
        if (len_tx < 8) {
            len_tx = 8;
        } else if (len_tx % 4 != 0) {
            len_tx = (len_tx / 4 + 1) * 4;
        }

    	tr.len = len_tx;

        pthread_mutex_lock(&mutex);
    	rv = ioctl(spi_fd, SPI_IOC_MESSAGE(1), &tr);
    	if (rv < 0) return SPI_ERR_MSG;
    }

    return SPI_OK;
}

static void *spi_rtscts_handler(void *arg) {
    MSGQueue *msgq_out = &spi_msgq_out;
    int i, rv;
    struct gpiod_line_bulk evt_lines;
    struct gpiod_line_event event;

    while (1) {
        rv = gpiod_line_event_wait_bulk(&gpio_rtscts, NULL, &evt_lines);
        if (rv < 0) continue;

        for (i=0; i<gpiod_line_bulk_num_lines(&evt_lines); i++) {
            struct gpiod_line *line;

            line = gpiod_line_bulk_get_line(&evt_lines, i);
            rv = gpiod_line_event_read(line, &event);
            if (rv || (event.event_type != GPIOD_LINE_EVENT_RISING_EDGE)) continue;

            switch (gpiod_line_offset(line)) {
                case SPI_GPIO_RTS:
                    pthread_mutex_lock(&msgq_out->mutex);
                    pthread_cond_signal(&msgq_out->cond);
                    pthread_mutex_unlock(&msgq_out->mutex);
                    break;
                case SPI_GPIO_CTS:
                    pthread_mutex_unlock(&mutex);
                    break;
            }
        }
    }
    return NULL;
}

static void *spi_worker(void *arg) {
    MSGQueue *bufq = &spi_bufq;
    MSGQueue *msgq_in = &spi_msgq_in;
    MSGQueue *msgq_out = &spi_msgq_out;
    int rv;
    unsigned char *buffer;

    while (1) {
        pthread_mutex_lock(&msgq_out->mutex);
        buffer = msgq_pop(msgq_out);
        if ((buffer == NULL) && (gpiod_line_get_value(gpio_rts) == 1)) {
            pthread_mutex_lock(&bufq->mutex);
            buffer = msgq_pop(bufq);
            pthread_mutex_unlock(&bufq->mutex);
        }
        if (buffer == NULL) {
            pthread_cond_wait(&msgq_out->cond, &msgq_out->mutex);
            buffer = msgq_pop(msgq_out);
        }
        pthread_mutex_unlock(&msgq_out->mutex);
        if (buffer) {
            rv = _spi_xchg(buffer);
            if (rv || (buffer[0] == 0)) {
                buffer[1] = 0;
                buffer[2] = 0;
                pthread_mutex_lock(&bufq->mutex);
                rv = msgq_push(bufq, buffer);
                pthread_mutex_unlock(&bufq->mutex);
            } else {
                pthread_mutex_lock(&msgq_in->mutex);
                rv = msgq_push(msgq_in, buffer);
                pthread_cond_signal(&msgq_in->cond);
                pthread_mutex_unlock(&msgq_in->mutex);
            }
        }
    }
    return NULL;
}

static void *spi_handler(void *arg) {
    MSGQueue *bufq = &spi_bufq;
    MSGQueue *msgq_in = &spi_msgq_in;
    unsigned char *buffer;
    unsigned char mtype;
    uint16_t len;
    int rv;

    while (1) {
        pthread_mutex_lock(&msgq_in->mutex);
        buffer = msgq_pop(msgq_in);
        if (buffer == NULL) {
            pthread_cond_wait(&msgq_in->cond, &msgq_in->mutex);
            buffer = msgq_pop(msgq_in);
        }
        pthread_mutex_unlock(&msgq_in->mutex);
        if (buffer) {
            mtype = buffer[0];
            len  = (uint16_t)buffer[1] << 8;
            len |= (uint16_t)buffer[2] & 0xFF;

            switch (mtype) {
                case SPI_MTYPE_TUN:
                    tun_write(buffer + SPI_SIZE_HDR, len);
                    break;
            }
            buffer[0] = 0;
            buffer[1] = 0;
            buffer[2] = 0;
            pthread_mutex_lock(&bufq->mutex);
            rv = msgq_push(bufq, buffer);
            pthread_mutex_unlock(&bufq->mutex);
        }
    }
}

unsigned char *spi_alloc(void) {
    MSGQueue *bufq = &spi_bufq;
    unsigned char *ret;

    pthread_mutex_lock(&bufq->mutex);
    ret = msgq_pop(bufq);
    pthread_mutex_unlock(&bufq->mutex);

    return ret;
}

void spi_free(unsigned char *buffer) {
    MSGQueue *bufq = &spi_bufq;

    buffer[0] = 0;
    buffer[1] = 0;
    buffer[2] = 0;
    pthread_mutex_lock(&bufq->mutex);
    msgq_push(bufq, buffer);
    pthread_mutex_unlock(&bufq->mutex);
}

int spi_xchg(unsigned char mtype, unsigned char *buffer, uint16_t len) {
	int rv;
    MSGQueue *msgq_out = &spi_msgq_out;

    buffer[0] = mtype;
    buffer[1] = len >> 8;
    buffer[2] = len & 0xFF;

    pthread_mutex_lock(&msgq_out->mutex);
    rv = msgq_push(msgq_out, buffer);
    pthread_cond_signal(&msgq_out->cond);
    pthread_mutex_unlock(&msgq_out->mutex);

    return rv;
}

int spi_init(char *fname) {
    int rv, i;
    struct gpiod_chip *gpio_chip;

	spi_fd = open(fname, O_RDWR);
	if (spi_fd < 0) return SPI_ERR_OPEN;

	rv = ioctl(spi_fd, SPI_IOC_WR_MAX_SPEED_HZ, &spi_speed);
	if (rv == -1) return SPI_ERR;

    gpio_chip = gpiod_chip_open_by_name(SPI_GPIO_BANK);
    if (gpio_chip == NULL) return SPI_ERR;

    gpio_rts = gpiod_chip_get_line(gpio_chip, SPI_GPIO_RTS);
    gpio_cts = gpiod_chip_get_line(gpio_chip, SPI_GPIO_CTS);
    if ((gpio_rts == NULL) || (gpio_cts == NULL)) return SPI_ERR;

    gpiod_line_bulk_init(&gpio_rtscts);
    gpiod_line_bulk_add(&gpio_rtscts, gpio_rts);
    gpiod_line_bulk_add(&gpio_rtscts, gpio_cts);
    rv = gpiod_line_request_bulk_rising_edge_events(&gpio_rtscts, "rtscts");
    if (rv) return SPI_ERR;

    msgq_init(&spi_bufq, spi_bufq_array, SPI_SIZE_BUFQ);
    msgq_init(&spi_msgq_in, spi_msgq_in_array, SPI_SIZE_MSGQ);
    msgq_init(&spi_msgq_out, spi_msgq_out_array, SPI_SIZE_MSGQ);
    for (i=0; i<SPI_SIZE_BUFQ; i++) {
        msgq_push(&spi_bufq, malloc(SPI_SIZE_BUF));
    }

    rv = pthread_mutex_init(&mutex, NULL);
    rv = pthread_create(&rtscts_thd, NULL, spi_rtscts_handler, NULL);
    rv = pthread_create(&worker_thd, NULL, spi_worker, NULL);
    rv = pthread_create(&handler_thd, NULL, spi_handler, NULL);

    return SPI_OK;
}

int main(int argc, char *argv[]) {
    int rv;

    rv = spi_init("/dev/spidev0.0");
    if (rv) printf("SPI INIT ERR\n");
    rv = tun_init();
    if (rv) printf("TUN INIT ERR\n");
    tun_read(NULL);
}
