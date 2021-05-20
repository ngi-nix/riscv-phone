#include <stdlib.h>
#include <stdint.h>

#include "eos.h"
#include "spi.h"
#include "cam.h"

const int _eos_cam_resolution[][2] = {
    {0,    0   },
    // C/SIF Resolutions
    {88,   72  },    /* QQCIF     */
    {176,  144 },    /* QCIF      */
    {352,  288 },    /* CIF       */
    {88,   60  },    /* QQSIF     */
    {176,  120 },    /* QSIF      */
    {352,  240 },    /* SIF       */
    // VGA Resolutions
    {40,   30  },    /* QQQQVGA   */
    {80,   60  },    /* QQQVGA    */
    {160,  120 },    /* QQVGA     */
    {320,  240 },    /* QVGA      */
    {640,  480 },    /* VGA       */
    {60,   40  },    /* HQQQVGA   */
    {120,  80  },    /* HQQVGA    */
    {240,  160 },    /* HQVGA     */
    // FFT Resolutions
    {64,   32  },    /* 64x32     */
    {64,   64  },    /* 64x64     */
    {128,  64  },    /* 128x64    */
    {128,  128 },    /* 128x128   */
    {320,  320 },    /* 128x128   */
    // Other
    {128,  160 },    /* LCD       */
    {128,  160 },    /* QQVGA2    */
    {720,  480 },    /* WVGA      */
    {752,  480 },    /* WVGA2     */
    {800,  600 },    /* SVGA      */
    {1024, 768 },    /* XGA       */
    {1280, 1024},    /* SXGA      */
    {1600, 1200},    /* UXGA      */
    {1280, 720 },    /* HD        */
    {1920, 1080},    /* FHD       */
    {2560, 1440},    /* QHD       */
    {2048, 1536},    /* QXGA      */
    {2560, 1600},    /* WQXGA     */
    {2592, 1944},    /* WQXGA2    */
};

#define CAM_REG_CAPTURE_CTRL        0x01
#define CAM_REG_FIFO_CTRL           0x04

#define CAM_REG_GPIO_DIR            0x05
#define CAM_REG_GPIO_WR             0x06
#define CAM_REG_GPIO_RD             0x45

#define CAM_REG_STATUS              0x41

#define CAM_REG_VER                 0x40

#define CAM_REG_READ_BURST          0x3c
#define CAM_REG_READ_BYTE           0x3d

#define CAM_REG_FIFO_SIZE1          0x42
#define CAM_REG_FIFO_SIZE2          0x43
#define CAM_REG_FIFO_SIZE3          0x44

#define CAM_VAL_FIFO_CTRL_CLEAR     0x01
#define CAM_VAL_FIFO_CTRL_START     0x02
#define CAM_VAL_FIFO_CTRL_RSTWR     0x10
#define CAM_VAL_FIFO_CTRL_RSTRD     0x20

#define CAM_VAL_STATUS_VSYNC        0x01
#define CAM_VAL_STATUS_SHUTTER      0x02
#define CAM_VAL_STATUS_CPTDONE      0x08

#define CAM_VAL_GPIO_RST            0x01
#define CAM_VAL_GPIO_PWRDN          0x02
#define CAM_VAL_GPIO_PWREN          0x04

static uint8_t reg_read(uint8_t addr) {
    uint8_t ret;

    eos_spi_cs_set();
    eos_spi_xchg8(addr, 0);
    ret = eos_spi_xchg8(0, 0);
    eos_spi_cs_clear();

    return ret;
}

static void reg_write(uint8_t addr, uint8_t val) {
    eos_spi_cs_set();
    eos_spi_xchg8(addr | 0x80, 0);
    eos_spi_xchg8(val, 0);
    eos_spi_cs_clear();
}

void eos_cam_capture(void) {
    reg_write(CAM_REG_FIFO_CTRL, CAM_VAL_FIFO_CTRL_START);
}

int eos_cam_capture_done(void) {
    return !!(reg_read(CAM_REG_STATUS) & CAM_VAL_STATUS_CPTDONE);
}

void eos_cam_capture_wait(void) {
    int done = 0;
    
    while (!done) {
        done = eos_cam_capture_done();
    }
}

uint32_t eos_cam_fbuf_size(void) {
    uint32_t ret;
    
	ret = reg_read(CAM_REG_FIFO_SIZE1);
    ret |= reg_read(CAM_REG_FIFO_SIZE2) << 8;
    ret |= (reg_read(CAM_REG_FIFO_SIZE3) & 0x7f) << 16;
    return ret;
}

void eos_cam_fbuf_read(uint8_t *buffer, uint16_t sz, int first) {
    int i;
    
    eos_spi_cs_set();
    eos_spi_xchg8(CAM_REG_READ_BURST, 0);
    if (first) eos_spi_xchg8(0, 0);

    for (i=0; i<sz; i++) {
        buffer[i] = eos_spi_xchg8(0, 0);
    }
    eos_spi_cs_clear();
}

void eos_cam_fbuf_done(void) {
    reg_write(CAM_REG_FIFO_CTRL, CAM_VAL_FIFO_CTRL_CLEAR);
}
