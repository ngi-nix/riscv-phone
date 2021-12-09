#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <eos.h>
#include <cam.h>

#include <i2c.h>
#include <i2c/ov2640.h>

#include <eve/eve.h>
#include <eve/eve_kbd.h>
#include <eve/eve_font.h>

#include <eve/screen/window.h>
#include <eve/screen/page.h>
#include <eve/screen/form.h>

#include <eve/widget/widgets.h>

#include <app/app_root.h>

#include <board.h>

#include "cam.h"

#include <stdio.h>

static int cam_init = 0;
static int cam_capture = 0;

#define CHUNK_SIZE  512

#define CAM_W       640
#define CAM_H       480

static void transfer_chunk(uint8_t *fbuf, size_t size, int first, int last, uint32_t addr) {
    int rv;

    eos_cam_fbuf_read(fbuf, size, first);
    if (last) {
        eos_cam_fbuf_done();
        eos_cam_capture();
    }

    eos_spi_select(EOS_SPI_DEV_EVE);
    eve_cmd_burst_start();
    if (first) eve_cmd(CMD_LOADIMAGE, "ww+", addr, EVE_OPT_NODL);
    eve_cmd_write(fbuf, size);
    if (last) eve_cmd_end();
    eve_cmd_burst_end();
    rv = eve_cmd_exec(last);
    if (rv) printf("CMD EXEC ERR\n");
    if (!last) eos_spi_select(EOS_SPI_DEV_CAM);
}

static void transfer_img(uint32_t addr) {
    int i;
    uint32_t fb_size;
    uint32_t fb_div;
    uint32_t fb_mod;
    uint8_t fbuf[CHUNK_SIZE];

    cam_capture = 1;
    fb_size = eos_cam_fbuf_size();

    fb_div = fb_size / CHUNK_SIZE;
    fb_mod = fb_size % CHUNK_SIZE;
    for (i=0; i<fb_div; i++) {
        transfer_chunk(fbuf, CHUNK_SIZE, i == 0, (fb_mod == 0) && (i + 1 == fb_div), addr);
    }
    if (fb_mod) {
        transfer_chunk(fbuf, fb_mod, fb_size < CHUNK_SIZE, 1, addr);
    }
    printf("CAPTURE DONE. ADDR:%x SIZE:%d\n", addr, fb_size);
}

static void user_handler(unsigned char type, unsigned char *buffer, uint16_t size) {
    eos_spi_select(EOS_SPI_DEV_CAM);
    if (eos_cam_capture_done()) {
        EVEWindowRoot *root = app_root();
        uint32_t addr = root->mem_next;

        transfer_img(addr);
        eve_window_root_draw(root);
    }
    eos_spi_deselect();
    eos_evtq_push(EOS_EVT_USER, NULL, 0);
}

static void image_draw(EVEFreeWidget *widget) {
    EVEWindowRoot *root = widget->w.page->v.window->root;
    uint32_t addr = root->mem_next;

    if (cam_capture) {
        // eve_freew_tag(widget);
        eve_cmd_dl(TAG_MASK(0));
        eve_cmd_dl(BEGIN(EVE_BITMAPS));
        // eve_cmd_dl(BITMAP_HANDLE(15));
        eve_cmd_dl(BITMAP_SOURCE(addr));
        eve_cmd_dl(BITMAP_LAYOUT(EVE_RGB565, CAM_W * 2, CAM_H));
        eve_cmd_dl(BITMAP_LAYOUT_H(CAM_W * 2, CAM_H));
        eve_cmd_dl(BITMAP_SIZE(EVE_NEAREST, EVE_BORDER, EVE_BORDER, CAM_H, CAM_W));
        eve_cmd_dl(BITMAP_SIZE_H(CAM_H, CAM_W));
        eve_cmd(CMD_LOADIDENTITY, "");
        eve_cmd(CMD_TRANSLATE, "ww", CAM_H * 65536, 0);
        eve_cmd(CMD_ROTATE, "w", 90 * 65536 / 360);
        eve_cmd(CMD_SETMATRIX, "");
        eve_cmd_dl(VERTEX2F(0, 0));
        eve_cmd_dl(TAG_MASK(1));
    }
}

static int image_touch(EVEFreeWidget *widget, EVETouch *touch, uint16_t evt) {
    return 0;
}

void fbuf_print(uint8_t *fbuf, size_t size) {
    int i;

    for (i=0; i<size; i++) {
        if (i % 128 == 0) printf("\n");
        printf("%.2x", fbuf[i]);
    }
}

void app_cam(EVEWindow *window, EVEViewStack *stack) {
    EVEWidgetSpec spec[] = {
        {
            .widget.type = EVE_WIDGET_TYPE_FREE,
            .widget.g.h = CAM_W,
            .widget.spec.free.draw = image_draw,
            .widget.spec.free.touch = image_touch,
        },
    };
    EVEForm *form = eve_form_create(window, stack, spec, 1, NULL, NULL, app_cam_close);
    int rv = EOS_OK;

    eve_gpio_set(EVE_GPIO_CAM, 1);
    eos_time_sleep(100);

    eos_i2c_speed(100000);
    eos_i2c_start();
    rv = eos_ov2640_init();
    if (!rv) rv = eos_ov2640_set_pixfmt(PIXFORMAT_JPEG);
    if (!rv) rv = eos_ov2640_set_framesize(FRAMESIZE_VGA);
    eos_i2c_stop();
    eos_i2c_speed(EOS_I2C_SPEED);

    if (!rv) {
        printf("CAM INIT\n");
    } else {
        printf("CAM INIT ERR:%d\n", rv);
    }
    eos_evtq_set_handler(EOS_EVT_USER, user_handler);
    eos_evtq_push(EOS_EVT_USER, NULL, 0);
    eos_spi_select(EOS_SPI_DEV_CAM);
    eos_cam_capture();
    eos_spi_select(EOS_SPI_DEV_EVE);
}

void app_cam_close(EVEForm *form) {
    eve_form_destroy(form);
    eve_gpio_set(EVE_GPIO_CAM, 0);
    eos_evtq_get(EOS_EVT_USER, NULL, NULL);
    eos_evtq_set_handler(EOS_EVT_USER, NULL);
}
