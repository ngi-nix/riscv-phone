#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#include "eos.h"
#include "timer.h"
#include "cam.h"

#include "i2c.h"
#include "ov2640_regs.h"
#include "ov2640.h"

#define XCLK_FREQ       24000000
//#define XCLK_FREQ       12000000

#define CIF_WIDTH       (400)
#define CIF_HEIGHT      (296)

#define SVGA_WIDTH      (800)
#define SVGA_HEIGHT     (600)

#define UXGA_WIDTH      (1600)
#define UXGA_HEIGHT     (1200)

static const uint8_t default_regs[][2] = {

// From Linux Driver.

    {BANK_SEL,      BANK_SEL_DSP},
    {0x2c,          0xff},
    {0x2e,          0xdf},
    {BANK_SEL,      BANK_SEL_SENSOR},
    {0x3c,          0x32},
//  {CLKRC,         CLKRC_DOUBLE | 0x02},
    {CLKRC,         0x01},
    {COM2,          COM2_OUT_DRIVE_3x},
    {REG04,         REG04_SET(REG04_HFLIP_IMG | REG04_VFLIP_IMG | REG04_VREF_EN | REG04_HREF_EN)},
    {COM8,          COM8_SET(COM8_BNDF_EN | COM8_AGC_EN | COM8_AEC_EN)},
    {COM9,          COM9_AGC_SET(COM9_AGC_GAIN_8x)},
    {0x2c,          0x0c},
    {0x33,          0x78},
    {0x3a,          0x33},
    {0x3b,          0xfb},
    {0x3e,          0x00},
    {0x43,          0x11},
    {0x16,          0x10},
    {0x39,          0x02},
    {0x35,          0x88},
    {0x22,          0x0a},
    {0x37,          0x40},
    {0x23,          0x00},
    {ARCOM2,        0xa0},
    {0x06,          0x02},
    {0x06,          0x88},
    {0x07,          0xc0},
    {0x0d,          0xb7},
    {0x0e,          0x01},
    {0x4c,          0x00},
    {0x4a,          0x81},
    {0x21,          0x99},
    {AEW,           0x40},
    {AEB,           0x38},
    {VV,            VV_AGC_TH_SET(0x08, 0x02)},
    {0x5c,          0x00},
    {0x63,          0x00},
    {FLL,           0x22},
    {COM3,          COM3_BAND_SET(COM3_BAND_AUTO)},
    {REG5D,         0x55},
    {REG5E,         0x7d},
    {REG5F,         0x7d},
    {REG60,         0x55},
    {HISTO_LOW,     0x70},
    {HISTO_HIGH,    0x80},
    {0x7c,          0x05},
    {0x20,          0x80},
    {0x28,          0x30},
    {0x6c,          0x00},
    {0x6d,          0x80},
    {0x6e,          0x00},
    {0x70,          0x02},
    {0x71,          0x94},
    {0x73,          0xc1},
    {0x3d,          0x34},
    {COM7,          COM7_RES_UXGA | COM7_ZOOM_EN},
    {0x5a,          0x57},
    {COM25,         0x00},
    {BD50,          0xbb},
    {BD60,          0x9c},
    {BANK_SEL,      BANK_SEL_DSP},
    {0xe5,          0x7f},
    {MC_BIST,       MC_BIST_RESET | MC_BIST_BOOT_ROM_SEL},
    {0x41,          0x24},
    {RESET,         RESET_JPEG | RESET_DVP},
    {0x76,          0xff},
    {0x33,          0xa0},
    {0x42,          0x20},
    {0x43,          0x18},
    {0x4c,          0x00},
    {CTRL3,         CTRL3_BPC_EN | CTRL3_WPC_EN | 0x10},
    {0x88,          0x3f},
    {0xd7,          0x03},
    {0xd9,          0x10},
    {R_DVP_SP,      R_DVP_SP_AUTO_MODE | 0x2},
    {0xc8,          0x08},
    {0xc9,          0x80},
    {BPADDR,        0x00},
    {BPDATA,        0x00},
    {BPADDR,        0x03},
    {BPDATA,        0x48},
    {BPDATA,        0x48},
    {BPADDR,        0x08},
    {BPDATA,        0x20},
    {BPDATA,        0x10},
    {BPDATA,        0x0e},
    {0x90,          0x00},
    {0x91,          0x0e},
    {0x91,          0x1a},
    {0x91,          0x31},
    {0x91,          0x5a},
    {0x91,          0x69},
    {0x91,          0x75},
    {0x91,          0x7e},
    {0x91,          0x88},
    {0x91,          0x8f},
    {0x91,          0x96},
    {0x91,          0xa3},
    {0x91,          0xaf},
    {0x91,          0xc4},
    {0x91,          0xd7},
    {0x91,          0xe8},
    {0x91,          0x20},
    {0x92,          0x00},
    {0x93,          0x06},
    {0x93,          0xe3},
    {0x93,          0x03},
    {0x93,          0x03},
    {0x93,          0x00},
    {0x93,          0x02},
    {0x93,          0x00},
    {0x93,          0x00},
    {0x93,          0x00},
    {0x93,          0x00},
    {0x93,          0x00},
    {0x93,          0x00},
    {0x93,          0x00},
    {0x96,          0x00},
    {0x97,          0x08},
    {0x97,          0x19},
    {0x97,          0x02},
    {0x97,          0x0c},
    {0x97,          0x24},
    {0x97,          0x30},
    {0x97,          0x28},
    {0x97,          0x26},
    {0x97,          0x02},
    {0x97,          0x98},
    {0x97,          0x80},
    {0x97,          0x00},
    {0x97,          0x00},
    {0xa4,          0x00},
    {0xa8,          0x00},
    {0xc5,          0x11},
    {0xc6,          0x51},
    {0xbf,          0x80},
    {0xc7,          0x10},  /* simple AWB */
    {0xb6,          0x66},
    {0xb8,          0xA5},
    {0xb7,          0x64},
    {0xb9,          0x7C},
    {0xb3,          0xaf},
    {0xb4,          0x97},
    {0xb5,          0xFF},
    {0xb0,          0xC5},
    {0xb1,          0x94},
    {0xb2,          0x0f},
    {0xc4,          0x5c},
    {0xa6,          0x00},
    {0xa7,          0x20},
    {0xa7,          0xd8},
    {0xa7,          0x1b},
    {0xa7,          0x31},
    {0xa7,          0x00},
    {0xa7,          0x18},
    {0xa7,          0x20},
    {0xa7,          0xd8},
    {0xa7,          0x19},
    {0xa7,          0x31},
    {0xa7,          0x00},
    {0xa7,          0x18},
    {0xa7,          0x20},
    {0xa7,          0xd8},
    {0xa7,          0x19},
    {0xa7,          0x31},
    {0xa7,          0x00},
    {0xa7,          0x18},
    {0x7f,          0x00},
    {0xe5,          0x1f},
    {0xe1,          0x77},
    {0xdd,          0x7f},
    {CTRL0,         CTRL0_YUV422 | CTRL0_YUV_EN | CTRL0_RGB_EN},

// OpenMV Custom.

    {BANK_SEL,      BANK_SEL_SENSOR},
    {0x0f,          0x4b},
    {COM1,          0x8f},

// End.

    {0xff,          0xff},
};

// Looks really bad.
//static const uint8_t cif_regs[][2] = {
//    {BANK_SEL,  BANK_SEL_SENSOR},
//    {COM7,      COM7_RES_CIF},
//    {COM1,      0x06 | 0x80},
//    {HSTART,    0x11},
//    {HSTOP,     0x43},
//    {VSTART,    0x01}, // 0x01 fixes issue with garbage pixels in the image...
//    {VSTOP,     0x97},
//    {REG32,     0x09},
//    {BANK_SEL,  BANK_SEL_DSP},
//    {RESET,     RESET_DVP},
//    {SIZEL,     SIZEL_HSIZE8_11_SET(CIF_WIDTH) | SIZEL_HSIZE8_SET(CIF_WIDTH) | SIZEL_VSIZE8_SET(CIF_HEIGHT)},
//    {HSIZE8,    HSIZE8_SET(CIF_WIDTH)},
//    {VSIZE8,    VSIZE8_SET(CIF_HEIGHT)},
//    {CTRL2,     CTRL2_DCW_EN | CTRL2_SDE_EN | CTRL2_UV_AVG_EN | CTRL2_CMX_EN | CTRL2_UV_ADJ_EN},
//    {0,         0},
//};

static const uint8_t svga_regs[][2] = {
    {BANK_SEL,  BANK_SEL_SENSOR},
    {COM7,      COM7_RES_SVGA},
    {COM1,      0x0A | 0x80},
    {HSTART,    0x11},
    {HSTOP,     0x43},
    {VSTART,    0x01}, // 0x01 fixes issue with garbage pixels in the image...
    {VSTOP,     0x97},
    {REG32,     0x09},
    {BANK_SEL,  BANK_SEL_DSP},
    {RESET,     RESET_DVP},
    {SIZEL,     SIZEL_HSIZE8_11_SET(SVGA_WIDTH) | SIZEL_HSIZE8_SET(SVGA_WIDTH) | SIZEL_VSIZE8_SET(SVGA_HEIGHT)},
    {HSIZE8,    HSIZE8_SET(SVGA_WIDTH)},
    {VSIZE8,    VSIZE8_SET(SVGA_HEIGHT)},
    {CTRL2,     CTRL2_DCW_EN | CTRL2_SDE_EN | CTRL2_UV_AVG_EN | CTRL2_CMX_EN | CTRL2_UV_ADJ_EN},
    {0xff,      0xff},
};

static const uint8_t uxga_regs[][2] = {
    {BANK_SEL,  BANK_SEL_SENSOR},
    {COM7,      COM7_RES_UXGA},
    {COM1,      0x0F | 0x80},
    {HSTART,    0x11},
    {HSTOP,     0x75},
    {VSTART,    0x01},
    {VSTOP,     0x97},
    {REG32,     0x36},
    {BANK_SEL,  BANK_SEL_DSP},
    {RESET,     RESET_DVP},
    {SIZEL,     SIZEL_HSIZE8_11_SET(UXGA_WIDTH) | SIZEL_HSIZE8_SET(UXGA_WIDTH) | SIZEL_VSIZE8_SET(UXGA_HEIGHT)},
    {HSIZE8,    HSIZE8_SET(UXGA_WIDTH)},
    {VSIZE8,    VSIZE8_SET(UXGA_HEIGHT)},
    {CTRL2,     CTRL2_DCW_EN | CTRL2_SDE_EN | CTRL2_UV_AVG_EN | CTRL2_CMX_EN | CTRL2_UV_ADJ_EN},
    {0xff,      0xff},
};

static const uint8_t yuv422_regs[][2] = {
    {BANK_SEL,      BANK_SEL_DSP},
    {R_BYPASS,      R_BYPASS_DSP_EN},
    {IMAGE_MODE,    IMAGE_MODE_YUV422},
    {0xd7,          0x03},
    {0x33,          0xa0},
    {0xe5,          0x1f},
    {0xe1,          0x67},
    {RESET,         0x00},
    {R_BYPASS,      R_BYPASS_DSP_EN},
    {0xff,          0xff},
};

static const uint8_t rgb565_regs[][2] = {
    {BANK_SEL,      BANK_SEL_DSP},
    {R_BYPASS,      R_BYPASS_DSP_EN},
    {IMAGE_MODE,    IMAGE_MODE_RGB565},
    {0xd7,          0x03},
    {RESET,         0x00},
    {R_BYPASS,      R_BYPASS_DSP_EN},
    {0xff,          0xff},
};

static const uint8_t bayer_regs[][2] = {
    {BANK_SEL,      BANK_SEL_DSP},
    {R_BYPASS,      R_BYPASS_DSP_EN},
    {IMAGE_MODE,    IMAGE_MODE_RAW10},
    {0xd7,          0x03},
    {RESET,         0x00},
    {R_BYPASS,      R_BYPASS_DSP_EN},
    {0xff,          0xff},
};

static const uint8_t jpeg_regs[][2] = {
    {BANK_SEL,      BANK_SEL_DSP},
    {R_BYPASS,      R_BYPASS_DSP_EN},
    {IMAGE_MODE,    IMAGE_MODE_JPEG_EN},
    {0xd7,          0x03},
    {RESET,         0x00},
    {R_BYPASS,      R_BYPASS_DSP_EN},
    {0xff,          0xff},
};

#define NUM_BRIGHTNESS_LEVELS (5)
static const uint8_t brightness_regs[NUM_BRIGHTNESS_LEVELS + 1][5] = {
    {BPADDR, BPDATA, BPADDR, BPDATA, BPDATA},
    {0x00, 0x04, 0x09, 0x00, 0x00}, /* -2 */
    {0x00, 0x04, 0x09, 0x10, 0x00}, /* -1 */
    {0x00, 0x04, 0x09, 0x20, 0x00}, /*  0 */
    {0x00, 0x04, 0x09, 0x30, 0x00}, /* +1 */
    {0x00, 0x04, 0x09, 0x40, 0x00}, /* +2 */
};

#define NUM_CONTRAST_LEVELS (5)
static const uint8_t contrast_regs[NUM_CONTRAST_LEVELS + 1][7] = {
    {BPADDR, BPDATA, BPADDR, BPDATA, BPDATA, BPDATA, BPDATA},
    {0x00, 0x04, 0x07, 0x20, 0x18, 0x34, 0x06}, /* -2 */
    {0x00, 0x04, 0x07, 0x20, 0x1c, 0x2a, 0x06}, /* -1 */
    {0x00, 0x04, 0x07, 0x20, 0x20, 0x20, 0x06}, /*  0 */
    {0x00, 0x04, 0x07, 0x20, 0x24, 0x16, 0x06}, /* +1 */
    {0x00, 0x04, 0x07, 0x20, 0x28, 0x0c, 0x06}, /* +2 */
};

#define NUM_SATURATION_LEVELS (5)
static const uint8_t saturation_regs[NUM_SATURATION_LEVELS + 1][5] = {
    {BPADDR, BPDATA, BPADDR, BPDATA, BPDATA},
    {0x00, 0x02, 0x03, 0x28, 0x28}, /* -2 */
    {0x00, 0x02, 0x03, 0x38, 0x38}, /* -1 */
    {0x00, 0x02, 0x03, 0x48, 0x48}, /*  0 */
    {0x00, 0x02, 0x03, 0x58, 0x58}, /* +1 */
    {0x00, 0x02, 0x03, 0x68, 0x68}, /* +2 */
};

static int reg_read(int8_t reg, uint8_t *data) {
    return eos_i2c_read8(OV2640_ADDR, reg, data, 1);
}

static int reg_write(uint8_t reg, uint8_t data) {
    return eos_i2c_write8(OV2640_ADDR, reg, &data, 1);
}

static int regarr_write(const uint8_t (*regs)[2]) {
    int i, rv;

    i = 0;
    rv = EOS_OK;

    while ((regs[i][0] != 0xff) || (regs[i][1] != 0xff)) {
        if (!rv) rv = reg_write(regs[i][0], regs[i][1]);
        i++;
    }

    return rv;
}

int eos_ov2640_init(void) {
    int rv;

    // Reset all registers
    rv = reg_write(BANK_SEL, BANK_SEL_SENSOR);
    if (rv) return rv;

    rv = reg_write(COM7, COM7_SRST);
    if (rv) return rv;

    // Delay 5 ms
    eos_time_sleep(5);

    // Write default regsiters
    rv = regarr_write(default_regs);
    if (rv) return rv;

    // Delay 300 ms
    eos_time_sleep(300);

    return EOS_OK;
}

int eos_ov2640_sleep(int enable) {
    uint8_t reg;
    int rv;

    rv = reg_write(BANK_SEL, BANK_SEL_SENSOR);
    if (rv) return rv;

    rv = reg_read(COM2, &reg);
    if (rv) return rv;

    if (enable) {
        reg |= COM2_STDBY;
    } else {
        reg &= ~COM2_STDBY;
    }

    // Write back register
    return reg_write(COM2, reg);
}

int eos_ov2640_set_pixfmt(pixformat_t fmt) {
    const uint8_t (*regs)[2];

    switch (fmt) {
        case PIXFORMAT_RGB565:
            regs = rgb565_regs;
            break;
        case PIXFORMAT_YUV422:
        case PIXFORMAT_GRAYSCALE:
            regs = yuv422_regs;
            break;
        case PIXFORMAT_BAYER:
            regs = bayer_regs;
            break;
        case PIXFORMAT_JPEG:
            regs = jpeg_regs;
            break;
        default:
            return EOS_ERR;
    }

    return regarr_write(regs);
}

int eos_ov2640_set_framesize(framesize_t framesize) {
    const uint8_t (*regs)[2];
    uint16_t sensor_w = 0;
    uint16_t sensor_h = 0;
    uint16_t w = _eos_cam_resolution[framesize][0];
    uint16_t h = _eos_cam_resolution[framesize][1];
    int rv;

    if ((w % 4) || (h % 4) || (w > UXGA_WIDTH) || (h > UXGA_HEIGHT)) { // w/h must be divisble by 4
        return EOS_ERR;
    }

    // Looks really bad.
    /* if ((w <= CIF_WIDTH) && (h <= CIF_HEIGHT)) {
        regs = cif_regs;
        sensor_w = CIF_WIDTH;
        sensor_h = CIF_HEIGHT;
    } else */ if ((w <= SVGA_WIDTH) && (h <= SVGA_HEIGHT)) {
        regs = svga_regs;
        sensor_w = SVGA_WIDTH;
        sensor_h = SVGA_HEIGHT;
    } else {
        regs = uxga_regs;
        sensor_w = UXGA_WIDTH;
        sensor_h = UXGA_HEIGHT;
    }

    // Write setup regsiters
    rv = regarr_write(regs);
    if (rv) return rv;

    uint64_t tmp_div = IM_MIN(sensor_w / w, sensor_h / h);
    uint16_t log_div = IM_MIN(IM_LOG2(tmp_div) - 1, 3);
    uint16_t div = 1 << log_div;
    uint16_t w_mul = w * div;
    uint16_t h_mul = h * div;
    uint16_t x_off = (sensor_w - w_mul) / 2;
    uint16_t y_off = (sensor_h - h_mul) / 2;

    rv = EOS_OK;
    if (!rv) rv = reg_write(CTRLI, CTRLI_LP_DP | CTRLI_V_DIV_SET(log_div) | CTRLI_H_DIV_SET(log_div));
    if (!rv) rv = reg_write(HSIZE, HSIZE_SET(w_mul));
    if (!rv) rv = reg_write(VSIZE, VSIZE_SET(h_mul));
    if (!rv) rv = reg_write(XOFFL, XOFFL_SET(x_off));
    if (!rv) rv = reg_write(YOFFL, YOFFL_SET(y_off));
    if (!rv) rv = reg_write(VHYX, VHYX_HSIZE_SET(w_mul) | VHYX_VSIZE_SET(h_mul) | VHYX_XOFF_SET(x_off) | VHYX_YOFF_SET(y_off));
    if (!rv) rv = reg_write(TEST, TEST_HSIZE_SET(w_mul));
    if (!rv) rv = reg_write(ZMOW, ZMOW_OUTW_SET(w));
    if (!rv) rv = reg_write(ZMOH, ZMOH_OUTH_SET(h));
    if (!rv) rv = reg_write(ZMHH, ZMHH_OUTW_SET(w) | ZMHH_OUTH_SET(h));
    if (!rv) rv = reg_write(R_DVP_SP, div);
    if (!rv) rv = reg_write(RESET, 0x00);

    return rv;
}

int eos_ov2640_set_contrast(int level) {
    int rv = EOS_OK;

    level += (NUM_CONTRAST_LEVELS / 2) + 1;
    if (level <= 0 || level > NUM_CONTRAST_LEVELS) {
        return EOS_ERR;
    }

    /* Switch to DSP register bank */
    rv = reg_write(BANK_SEL, BANK_SEL_DSP);

    /* Write contrast registers */
    for (int i=0; i<sizeof(contrast_regs[0])/sizeof(contrast_regs[0][0]); i++) {
        if (!rv) rv = reg_write(contrast_regs[0][i], contrast_regs[level][i]);
    }

    return rv;
}

int eos_ov2640_set_brightness(int level) {
    int rv = EOS_OK;

    level += (NUM_BRIGHTNESS_LEVELS / 2) + 1;
    if (level <= 0 || level > NUM_BRIGHTNESS_LEVELS) {
        return EOS_ERR;
    }

    /* Switch to DSP register bank */
    rv = reg_write(BANK_SEL, BANK_SEL_DSP);

    /* Write brightness registers */
    for (int i=0; i<sizeof(brightness_regs[0])/sizeof(brightness_regs[0][0]); i++) {
        if (!rv) rv = reg_write(brightness_regs[0][i], brightness_regs[level][i]);
    }

    return rv;
}

int eos_ov2640_set_saturation(int level) {
    int rv = EOS_OK;

    level += (NUM_SATURATION_LEVELS / 2) + 1;
    if (level <= 0 || level > NUM_SATURATION_LEVELS) {
        return EOS_ERR;
    }

    /* Switch to DSP register bank */
    rv = reg_write(BANK_SEL, BANK_SEL_DSP);

    /* Write saturation registers */
    for (int i=0; i<sizeof(saturation_regs[0])/sizeof(saturation_regs[0][0]); i++) {
        if (!rv) rv = reg_write(saturation_regs[0][i], saturation_regs[level][i]);
    }

    return rv;
}

int eos_ov2640_set_gainceiling(gainceiling_t gainceiling) {
    int rv = EOS_OK;

    /* Switch to SENSOR register bank */
    rv = reg_write(BANK_SEL, BANK_SEL_SENSOR);

    /* Write gain ceiling register */
    if (!rv) rv = reg_write(COM9, COM9_AGC_SET(gainceiling));

    return rv;
}

int eos_ov2640_set_quality(int qs) {
    int rv = EOS_OK;

    /* Switch to DSP register bank */
    rv = reg_write(BANK_SEL, BANK_SEL_DSP);

    /* Write QS register */
    if (!rv) rv = reg_write(QS, qs);

    return rv;
}

int eos_ov2640_set_colorbar(int enable) {
    int rv = EOS_OK;
    uint8_t reg;

    rv = reg_write(BANK_SEL, BANK_SEL_SENSOR);
    if (rv) return rv;

    rv = reg_read(COM7, &reg);
    if (rv) return rv;

    if (enable) {
        reg |= COM7_COLOR_BAR;
    } else {
        reg &= ~COM7_COLOR_BAR;
    }

    return reg_write(COM7, reg);
}

int eos_ov2640_set_auto_gain(int enable, float gain_db, float gain_db_ceiling) {
    int rv = EOS_OK;
    uint8_t reg;

    rv = reg_write(BANK_SEL, BANK_SEL_SENSOR);
    if (rv) return rv;

    rv = reg_read(COM8, &reg);
    if (rv) return rv;

    rv = reg_write(COM8, (reg & (~COM8_AGC_EN)) | (enable ? COM8_AGC_EN : 0));
    if (rv) return rv;

    rv = EOS_OK;
    if (enable && (!isnanf(gain_db_ceiling)) && (!isinff(gain_db_ceiling))) {
        float gain_ceiling = IM_MAX(IM_MIN(expf((gain_db_ceiling / 20.0) * logf(10.0)), 128.0), 2.0);

        if (!rv) rv = reg_read(COM9, &reg);
        if (!rv) rv = reg_write(COM9, (reg & 0x1F) | (((int)ceilf(log2f(gain_ceiling)) - 1) << 5));
    }

    if (!enable && (!isnanf(gain_db)) && (!isinff(gain_db))) {
        float gain = IM_MAX(IM_MIN(expf((gain_db / 20.0) * logf(10.0)), 32.0), 1.0);

        int gain_temp = roundf(log2f(IM_MAX(gain / 2.0, 1.0)));
        int gain_hi = 0xF >> (4 - gain_temp);
        int gain_lo = IM_MIN(roundf(((gain / (1 << gain_temp)) - 1.0) * 16.0), 15);

        if (!rv) rv = reg_write(GAIN, (gain_hi << 4) | (gain_lo << 0));
    }

    return rv;
}

int eos_ov2640_get_gain_db(float *gain_db) {
    int rv = EOS_OK;
    uint8_t reg, gain;

    rv = reg_write(BANK_SEL, BANK_SEL_SENSOR);
    if (rv) return rv;

    rv = reg_read(COM8, &reg);
    if (rv) return rv;

    // DISABLED
    // if (reg & COM8_AGC_EN) {
    //     rv = reg_write(COM8, reg & (~COM8_AGC_EN));
    //     if (rv) return rv;
    // }
    // DISABLED

    rv = reg_read(GAIN, &gain);
    if (rv) return rv;

    // DISABLED
    // if (reg & COM8_AGC_EN) {
    //     rv = reg_write(COM8, reg | COM8_AGC_EN);
    //     if (rv) return rv;
    // }
    // DISABLED

    int hi_gain = 1 << (((gain >> 7) & 1) + ((gain >> 6) & 1) + ((gain >> 5) & 1) + ((gain >> 4) & 1));
    float lo_gain = 1.0 + (((gain >> 0) & 0xF) / 16.0);
    *gain_db = 20.0 * (logf(hi_gain * lo_gain) / logf(10.0));

    return EOS_OK;
}

int eos_ov2640_set_auto_exposure(int enable, int exposure_us) {
    int rv = EOS_OK;
    uint8_t reg;

    rv = reg_write(BANK_SEL, BANK_SEL_SENSOR);
    if (rv) return rv;

    rv = reg_read(COM8, &reg);
    if (rv) return rv;

    rv = reg_write(COM8, COM8_SET_AEC(reg, (enable != 0)));
    if (rv) return rv;

    if (!enable && (exposure_us >= 0)) {
        rv = reg_read(COM7, &reg);
        if (rv) return rv;

        int t_line = 0;

        if (COM7_GET_RES(reg) == COM7_RES_UXGA) t_line = 1600 + 322;
        if (COM7_GET_RES(reg) == COM7_RES_SVGA) t_line = 800 + 390;
        if (COM7_GET_RES(reg) == COM7_RES_CIF) t_line = 400 + 195;

        rv = reg_read(CLKRC, &reg);
        if (rv) return rv;

        int pll_mult = ((reg & CLKRC_DOUBLE) ? 2 : 1) * 3;
        int clk_rc = (reg & CLKRC_DIVIDER_MASK) + 2;

        rv = reg_write(BANK_SEL, BANK_SEL_DSP);
        if (rv) return rv;

        rv = reg_read(IMAGE_MODE, &reg);
        if (rv) return rv;

        int t_pclk = 0;

        if (IMAGE_MODE_GET_FMT(reg) == IMAGE_MODE_YUV422) t_pclk = 2;
        if (IMAGE_MODE_GET_FMT(reg) == IMAGE_MODE_RAW10) t_pclk = 1;
        if (IMAGE_MODE_GET_FMT(reg) == IMAGE_MODE_RGB565) t_pclk = 2;

        int exposure = IM_MAX(IM_MIN(((exposure_us*(((XCLK_FREQ/clk_rc)*pll_mult)/1000000))/t_pclk)/t_line,0xFFFF),0x0000);

        if (!rv) rv = reg_write(BANK_SEL, BANK_SEL_SENSOR);

        if (!rv) rv = reg_read(REG04, &reg);
        if (!rv) rv = reg_write(REG04, (reg & 0xFC) | ((exposure >> 0) & 0x3));

        if (!rv) rv = reg_read(AEC, &reg);
        if (!rv) rv = reg_write(AEC, (reg & 0x00) | ((exposure >> 2) & 0xFF));

        if (!rv) rv = reg_read(REG45, &reg);
        if (!rv) rv = reg_write(REG45, (reg & 0xC0) | ((exposure >> 10) & 0x3F));
    }

    return rv;
}

int eos_ov2640_get_exposure_us(int *exposure_us) {
    int rv = EOS_OK;
    uint8_t reg, aec_10, aec_92, aec_1510;

    rv = reg_write(BANK_SEL, BANK_SEL_SENSOR);
    if (rv) return rv;

    rv = reg_read(COM8, &reg);
    if (rv) return rv;

    // DISABLED
    // if (reg & COM8_AEC_EN) {
    //     rv = reg_write(COM8, reg & (~COM8_AEC_EN));
    //     if (rv) return rv;
    // }
    // DISABLED

    rv = reg_read(REG04, &aec_10);
    if (rv) return rv;

    rv = reg_read(AEC, &aec_92);
    if (rv) return rv;

    rv = reg_read(REG45, &aec_1510);
    if (rv) return rv;

    // DISABLED
    // if (reg & COM8_AEC_EN) {
    //     rv = reg_write(COM8, reg | COM8_AEC_EN);
    //     if (rv) return rv;
    // }
    // DISABLED

    rv = reg_read(COM7, &reg);
    if (rv) return rv;

    int t_line = 0;

    if (COM7_GET_RES(reg) == COM7_RES_UXGA) t_line = 1600 + 322;
    if (COM7_GET_RES(reg) == COM7_RES_SVGA) t_line = 800 + 390;
    if (COM7_GET_RES(reg) == COM7_RES_CIF) t_line = 400 + 195;

    rv = reg_read(CLKRC, &reg);
    if (rv) return rv;

    int pll_mult = ((reg & CLKRC_DOUBLE) ? 2 : 1) * 3;
    int clk_rc = (reg & CLKRC_DIVIDER_MASK) + 2;

    rv = reg_write(BANK_SEL, BANK_SEL_DSP);
    if (rv) return rv;

    rv = reg_read(IMAGE_MODE, &reg);
    if (rv) return rv;

    int t_pclk = 0;

    if (IMAGE_MODE_GET_FMT(reg) == IMAGE_MODE_YUV422) t_pclk = 2;
    if (IMAGE_MODE_GET_FMT(reg) == IMAGE_MODE_RAW10) t_pclk = 1;
    if (IMAGE_MODE_GET_FMT(reg) == IMAGE_MODE_RGB565) t_pclk = 2;

    uint16_t exposure = ((aec_1510 & 0x3F) << 10) + ((aec_92 & 0xFF) << 2) + ((aec_10 & 0x3) << 0);
    *exposure_us = (exposure*t_line*t_pclk)/(((XCLK_FREQ/clk_rc)*pll_mult)/1000000);

    return EOS_OK;
}

int eos_ov2640_set_auto_whitebal(int enable, float r_gain_db, float g_gain_db, float b_gain_db) {
    int rv = EOS_OK;
    uint8_t reg;

    rv = reg_write(BANK_SEL, BANK_SEL_DSP);
    if (rv) return rv;

    rv = reg_read(CTRL1, &reg);
    if (rv) return rv;

    rv = reg_write(CTRL1, (reg & (~CTRL1_AWB)) | (enable ? CTRL1_AWB : 0));
    if (rv) return rv;

    if (!enable && (!isnanf(r_gain_db)) && (!isnanf(g_gain_db)) && (!isnanf(b_gain_db))
                && (!isinff(r_gain_db)) && (!isinff(g_gain_db)) && (!isinff(b_gain_db))) {
    }

    return rv;
}

int eos_ov2640_set_hmirror(int enable) {
    int rv = EOS_OK;
    uint8_t reg;

    rv = reg_write(BANK_SEL, BANK_SEL_SENSOR);
    if (rv) return rv;

    rv = reg_read(REG04, &reg);
    if (rv) return rv;

    if (!enable) { // Already mirrored.
        reg |= REG04_HFLIP_IMG;
    } else {
        reg &= ~REG04_HFLIP_IMG;
    }

    return reg_write(REG04, reg);
}

int eos_ov2640_set_vflip(int enable) {
    int rv = EOS_OK;
    uint8_t reg;

    rv = reg_write(BANK_SEL, BANK_SEL_SENSOR);
    if (rv) return rv;

    rv = reg_read(REG04, &reg);
    if (rv) return rv;

    if (!enable) { // Already flipped.
        reg |= REG04_VFLIP_IMG | REG04_VREF_EN;
    } else {
        reg &= ~(REG04_VFLIP_IMG | REG04_VREF_EN);
    }

    return reg_write(REG04, reg);
}

int eos_ov2640_set_effect(sde_t sde) {
    int rv = EOS_OK;

    switch (sde) {
        case SDE_NEGATIVE:
            if (!rv) rv = reg_write(BANK_SEL, BANK_SEL_DSP);
            if (!rv) rv = reg_write(BPADDR, 0x00);
            if (!rv) rv = reg_write(BPDATA, 0x40);
            if (!rv) rv = reg_write(BPADDR, 0x05);
            if (!rv) rv = reg_write(BPDATA, 0x80);
            if (!rv) rv = reg_write(BPDATA, 0x80);
            break;
        case SDE_NORMAL:
            if (!rv) rv = reg_write(BANK_SEL, BANK_SEL_DSP);
            if (!rv) rv = reg_write(BPADDR, 0x00);
            if (!rv) rv = reg_write(BPDATA, 0x00);
            if (!rv) rv = reg_write(BPADDR, 0x05);
            if (!rv) rv = reg_write(BPDATA, 0x80);
            if (!rv) rv = reg_write(BPDATA, 0x80);
            break;
        default:
            return EOS_ERR;
    }

    return rv;
}
