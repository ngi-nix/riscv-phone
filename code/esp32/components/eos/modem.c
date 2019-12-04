#include <stdio.h>
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <driver/uart.h>
#include <driver/gpio.h>
#include <esp_log.h>


#include "eos.h"

#define BUF_SIZE        1024
#define UART_GPIO_DTR   32
static QueueHandle_t uart_queue;

static void uart_event_task(void *pvParameters) {
    uart_event_t event;
    size_t len;

    // Reserve a buffer and process incoming data
    uint8_t *data = (uint8_t *) malloc(BUF_SIZE);

    while (1) {
        /* Waiting for UART event.
         */
        if (xQueueReceive(uart_queue, (void * )&event, (portTickType)portMAX_DELAY)) {
            switch (event.type) {
                case UART_DATA:
                    /* Event of UART receiving data
                     */
                    len = 0;
                    uart_get_buffered_data_len(UART_NUM_2, &len);
                    if (len) {
                        len = uart_read_bytes(UART_NUM_2, data, len, 100 / portTICK_RATE_MS);
                        // eos_net_send(EOS_FE310_CMD_MODEM_DATA, data, len);
                    }
                    break;
                default:
                    break;
            }
        }
    }
    free(data);
    vTaskDelete(NULL);
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
    uart_set_pin(UART_NUM_2, 17, 16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(UART_NUM_2, BUF_SIZE, BUF_SIZE, 10, &uart_queue, 0);

    // Configuration for the DTR/RI lines
    gpio_config_t io_conf;

    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = ((uint64_t)1 << UART_GPIO_DTR);
    gpio_config(&io_conf);
    gpio_set_level(UART_GPIO_DTR, 1);

    // Create a task to handle uart event from ISR
    xTaskCreate(uart_event_task, "uart_event_task", 2048, NULL, EOS_IRQ_PRIORITY_UART, NULL);
}

ssize_t eos_modem_write(void *data, size_t size) {
    return uart_write_bytes(UART_NUM_2, (const char *)data, size);
}
