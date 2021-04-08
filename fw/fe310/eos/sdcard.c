#include <stdlib.h>
#include <stdint.h>

#include "eos.h"
#include "timer.h"

#include "spi.h"
#include "spi_dev.h"

#include "sdcard.h"

#define SDC_POLY_CRC7           0x09
#define SDC_POLY_CRC16          0x1021

#define SDC_CMD_FLAG_CRC        0x01
#define SDC_CMD_FLAG_NOCS       0x02
#define SDC_CMD_FLAG_SKIP       0x08

#define SDC_TOKEN_START_BLK     0xfe
#define SDC_TOKEN_START_BLKM    0xfc
#define SDC_TOKEN_STOP_TRAN     0xfd

#define SDC_R1_READY            0x00

#define SDC_R1_IDLE_STATE       0x01
#define SDC_R1_ERASE_RESET      0x02
#define SDC_R1_ILLEGAL_CMD      0x04
#define SDC_R1_ERR_CMD_CRC      0x08
#define SDC_R1_ERR_ERASE_SEQ    0x10
#define SDC_R1_ERR_ADDR         0x20
#define SDC_R1_ERR_PARAM        0x40

#define SDC_NCR                 10

#define SDC_TYPE_MMC            1
#define SDC_TYPE_SDC1           2
#define SDC_TYPE_SDC2           3

#define SDC_ERR(rv)             ((rv < 0) ? rv : EOS_ERR)

/* CMD */
#define GO_IDLE_STATE           0
#define SEND_OP_COND            1
#define SEND_IF_COND            8
#define SET_BLOCKLEN            16
#define APP_CMD                 55
#define READ_OCR                58

/* ACMD */
#define SD_APP_OP_COND          41

static uint8_t sdc_crc7(uint8_t crc, uint8_t b) {
    int i;

    for (i=8; i--; b<<=1) {
        crc <<= 1;
        if ((b ^ crc) & 0x80) crc ^= SDC_POLY_CRC7;
    }
    return crc & 0x7f;
}

static uint16_t sdc_crc16(uint16_t crc, uint8_t b) {
    int i;

    crc = crc ^ ((uint16_t)b << 8);
    for (i=8; i--;) {
        if (crc & 0x8000) {
            crc = (crc << 1) ^ SDC_POLY_CRC16;
        } else {
            crc <<= 1;
        }
    }
    return crc;
}

static uint32_t sdc_nto(uint64_t start, uint32_t timeout) {
    uint32_t d = eos_time_since(start);
    return (d > timeout) ? 0 : timeout - d;
}

static void sdc_xchg(unsigned char *buffer, uint16_t len) {
}

static uint8_t sdc_xchg8(uint8_t data) {
    return eos_spi_xchg8(data, 0);
}

static uint16_t sdc_xchg16(uint16_t data) {
    return eos_spi_xchg16(data, 0);
}

static uint32_t sdc_xchg32(uint32_t data) {
    return eos_spi_xchg32(data, 0);
}

static void sdc_select(void) {
    eos_spi_cs_set();
    eos_spi_xchg8(0xff, 0);
}

static void sdc_deselect(void) {
    eos_spi_cs_clear();
    eos_spi_xchg8(0xff, 0);
}

static int sdc_xchg_cmd(uint8_t cmd, uint32_t arg, uint8_t flags) {
    int i;
    uint8_t ret;
    uint8_t crc = 0x7f;

    cmd |= 0x40;
    if (flags & SDC_CMD_FLAG_CRC) {
        crc = sdc_crc7(0, cmd);
        crc = sdc_crc7(crc, arg >> 24);
        crc = sdc_crc7(crc, arg >> 16);
        crc = sdc_crc7(crc, arg >> 8);
        crc = sdc_crc7(crc, arg);
    }
    crc = (crc << 1) | 0x01;
    sdc_xchg8(cmd);
    sdc_xchg32(arg);
    sdc_xchg8(crc);
    if (flags & SDC_CMD_FLAG_SKIP) sdc_xchg8(0xff);

    i = SDC_NCR;
    do {
        ret = sdc_xchg8(0xff);
    } while ((ret & 0x80) && --i);
    if (ret & 0x80) return EOS_ERR_BUSY;

    return ret;
}

static int sdc_ready(uint32_t timeout) {
    uint8_t d = 0;
    uint64_t start;

    if (timeout == 0) return EOS_ERR_BUSY;
    start = eos_time_get_tick();
	do {
        if (eos_time_since(start) > timeout) break;
		d = sdc_xchg8(0xff);
	} while (d != 0xff);
	if (d != 0xff) return EOS_ERR_BUSY;

    return EOS_OK;
}

static int sdc_block_read(uint8_t *buffer, uint16_t len, uint32_t timeout) {
    uint8_t token = 0xff;
    uint64_t start;

    if (timeout == 0) return EOS_ERR_BUSY;
    start = eos_time_get_tick();
	do {
        if (eos_time_since(start) > timeout) break;
		token = sdc_xchg8(0xff);
	} while (token == 0xff);
    if (token == 0xff) return EOS_ERR_BUSY;
	if (token != SDC_TOKEN_START_BLK) return EOS_ERR;

    sdc_xchg(buffer, len);
    sdc_xchg16(0xff); /* dummy CRC */

    return EOS_OK;
}

static int sdc_block_write(uint8_t token, uint8_t *buffer, uint16_t len, uint32_t timeout) {
    uint8_t d;
    int rv;

    rv = sdc_ready(timeout);
    if (rv) return rv;

    sdc_xchg8(token);
    if (buffer && len) {
        sdc_xchg(buffer, len);
        sdc_xchg16(0xff); /* dummy CRC */

        d = sdc_xchg8(0xff); /* Response */
        if ((d & 0x1f) != 0x05) return EOS_ERR;
    }

    return EOS_OK;
}

static int sdc_cmd(uint8_t cmd, uint32_t arg, uint8_t flags, uint32_t timeout) {
    int do_cs = !(flags & SDC_CMD_FLAG_NOCS);
    int rv;

    if (do_cs) sdc_select();
    rv = sdc_ready(timeout);
    if (rv) {
        if (do_cs) sdc_deselect();
        return rv;
    }
    rv = sdc_xchg_cmd(cmd, arg, flags);
    if (do_cs) sdc_deselect();
    return rv;
}

static int sdc_acmd(uint8_t cmd, uint32_t arg, uint8_t flags, uint32_t timeout) {
    int rv;
    uint64_t start;

    if (flags & SDC_CMD_FLAG_NOCS) return EOS_ERR;

    start = eos_time_get_tick();
    rv = sdc_cmd(APP_CMD, 0, flags & SDC_CMD_FLAG_CRC, timeout);
    if (rv & ~SDC_R1_IDLE_STATE) return rv;

    return sdc_cmd(cmd, arg, flags, sdc_nto(start, timeout));
}

static int sdc_init(uint32_t timeout, uint8_t *type, uint8_t *lba) {
    uint8_t _type, _lba;
    int rv, i;
    uint8_t ocr[4];
    uint64_t start;

    _type = 0;
    _lba = 0;

    start = eos_time_get_tick();
    sdc_select();
	for (i=10; i--;) sdc_xchg8(0xff);     /* 80 dummy cycles */
    rv = sdc_cmd(GO_IDLE_STATE, 0, SDC_CMD_FLAG_CRC | SDC_CMD_FLAG_NOCS, timeout);
    if (rv != SDC_R1_IDLE_STATE) return SDC_ERR(rv);
    sdc_deselect();

    sdc_select();
    rv = sdc_cmd(SEND_IF_COND, 0x1aa, SDC_CMD_FLAG_CRC | SDC_CMD_FLAG_NOCS, sdc_nto(start, timeout));
    switch (rv) {
        case SDC_R1_IDLE_STATE:
    		for (i=0; i<4; i++) {
                ocr[i] = sdc_xchg8(0xff);    /* get R7 resp */
            }
            sdc_deselect();
    		if (ocr[2] == 0x01 && ocr[3] == 0xaa) {     /* Check voltage range: 2.7-3.6V */
                do {
                    rv = sdc_acmd(SD_APP_OP_COND, 0x40000000, 0, sdc_nto(start, timeout));
                } while (rv == SDC_R1_IDLE_STATE);
                if (rv != SDC_R1_READY) return SDC_ERR(rv);

                sdc_select();
                rv = sdc_cmd(READ_OCR, 0, SDC_CMD_FLAG_NOCS, sdc_nto(start, timeout));
                if (rv == SDC_R1_READY) {
    				for (i=0; i<4; i++) {
                        ocr[i] = sdc_xchg8(0xff);
                    }
                    sdc_deselect();
                } else {
                    sdc_deselect();
                    return SDC_ERR(rv);
                }

                _type = SDC_TYPE_SDC2;
                if (ocr[0] & 0xc0) _lba = 1;
            }
            break;

        case SDC_R1_IDLE_STATE | SDC_R1_ILLEGAL_CMD:
            sdc_deselect();
            rv = sdc_acmd(SD_APP_OP_COND, 0, 0, sdc_nto(start, timeout));
            switch (rv) {
                case SDC_R1_IDLE_STATE:
                    do  {
                        rv = sdc_acmd(SD_APP_OP_COND, 0, 0, sdc_nto(start, timeout));
                    } while (rv == SDC_R1_IDLE_STATE);
                    if (rv != SDC_R1_READY) return SDC_ERR(rv);
                case SDC_R1_READY:
                    _type = SDC_TYPE_SDC1;
                    break;

                default:
                    do {
                        rv = sdc_cmd(SEND_OP_COND, 0, 0, sdc_nto(start, timeout));
                    } while (rv == SDC_R1_IDLE_STATE);
                    if (rv != SDC_R1_READY) return SDC_ERR(rv);
                    _type = SDC_TYPE_MMC;
                    break;

            }
            break;

        default:
            sdc_deselect();
            return SDC_ERR(rv);
    }

    if (_type && !_lba) {
        rv = sdc_cmd(SET_BLOCKLEN, 512, 0, sdc_nto(start, timeout));
        if (rv != SDC_R1_READY) return SDC_ERR(rv);
    }
    *type = _type;
    *lba = _lba;
    return EOS_OK;
}

#include <stdio.h>

void eos_sdc_init(void) {
    uint8_t type, lba;
    int rv;

    eos_spi_select(EOS_SPI_DEV_SDC);
    rv = sdc_init(5000, &type, &lba);
    if (rv) {
        printf("SDC ERROR\n");
    } else {
        printf("SDC OK:%d %d\n", type, lba);
    }
    eos_spi_deselect();
}
