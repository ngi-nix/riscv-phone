#include <stdio.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <driver/uart.h>
#include <driver/gpio.h>
#include <esp_log.h>

#include "eos.h"
#include "net.h"
#include "cell.h"

#define MIN(X, Y)               (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y)               (((X) > (Y)) ? (X) : (Y))

#define UART_BUF_SIZE   1024

#define UART_GPIO_TXD   16
#define UART_GPIO_RXD   17
#define UART_GPIO_DTR   32
#define UART_GPIO_RI    35


#define UART_EVENT_MODE_NONE    0
#define UART_EVENT_MODE_PPP     1
#define UART_EVENT_MODE_RELAY   2

static QueueHandle_t uart_queue;
// static uint8_t *uart_data[UART_BUF_SIZE];

static void uart_event_task(void *pvParameters) {
    char mode = 0;
    uart_event_t event;
    size_t len;
    unsigned char *buf;

    while (1) {
        /* Waiting for UART event.
         */
        if (xQueueReceive(uart_queue, (void * )&event, (portTickType)portMAX_DELAY)) {
            switch (event.type) {
                case UART_DATA:
                    /* Event of UART receiving data
                     */
                    switch (mode) {
                        case UART_EVENT_MODE_PPP:
                            break;

                        case UART_EVENT_MODE_RELAY:
                            buf = eos_net_alloc();
                            buf[0] = EOS_CELL_MTYPE_DATA;
                            len = uart_read_bytes(UART_NUM_2, buf+1, MIN(event.size, EOS_NET_SIZE_BUF-1), 100 / portTICK_RATE_MS);
                            eos_net_send(EOS_NET_MTYPE_CELL, buf, len, 0);
                            break;

                        default:
                            break;
                    }

                case UART_EVENT_MAX:
                    /* Mode change
                     */
                    mode = (char)event.size;
                    break;

                default:
                    break;
            }
        }
    }
    vTaskDelete(NULL);
}

static void modem_handler(unsigned char _mtype, unsigned char *buffer, uint16_t size) {
    uint8_t mtype = buffer[0];

    switch (mtype) {
        case EOS_CELL_MTYPE_DATA:
            eos_modem_write(buffer+1, size-1);
            break;
        case EOS_CELL_MTYPE_DATA_START:
            eos_modem_set_mode(UART_EVENT_MODE_RELAY);
            break;
        case EOS_CELL_MTYPE_DATA_STOP:
            eos_modem_set_mode(0);
            break;
    }
}

void eos_modem_init(void) {
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
       .baud_rate = 115200,
       .data_bits = UART_DATA_8_BITS,
       .parity    = UART_PARITY_DISABLE,
       .stop_bits = UART_STOP_BITS_1,
       .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(UART_NUM_2, &uart_config);
    uart_set_pin(UART_NUM_2, UART_GPIO_TXD, UART_GPIO_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_2, UART_BUF_SIZE, UART_BUF_SIZE, 10, &uart_queue, 0);

    // Configuration for the DTR/RI lines
    gpio_config_t io_conf;

    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = ((uint64_t)1 << UART_GPIO_DTR);
    gpio_config(&io_conf);
    gpio_set_level(UART_GPIO_DTR, 1);

    // Create a task to handle uart event from ISR
    xTaskCreate(uart_event_task, "uart_event_task", 2048, NULL, EOS_IRQ_PRIORITY_UART, NULL);

    eos_net_set_handler(EOS_NET_MTYPE_CELL, modem_handler);
}

ssize_t eos_modem_write(void *data, size_t size) {
    return uart_write_bytes(UART_NUM_2, (const char *)data, size);
}

void eos_modem_set_mode(char mode) {
    uart_event_t evt;
    evt.type = UART_EVENT_MAX;  /* my type */
    evt.size = mode;
    xQueueSend(&uart_queue, (void *)&evt, portMAX_DELAY);
}
