#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "eos.h"
#include "timer.h"

#include "spi.h"
#include "spi_dev.h"

#include "sdc_crypto.h"
#include "sdcard.h"

#define SDC_TIMEOUT_CMD         500
#define SDC_TIMEOUT_READ        200
#define SDC_TIMEOUT_WRITE       500

#define SDC_POLY_CRC7           0x09
#define SDC_POLY_CRC16          0x1021

#define SDC_CMD_FLAG_CRC        0x01
#define SDC_CMD_FLAG_NOCS       0x02
#define SDC_CMD_FLAG_NOWAIT     0x04
#define SDC_CMD_FLAG_RSTUFF     0x08

#define SDC_TOKEN_START_BLK     0xfe
#define SDC_TOKEN_START_BLKM    0xfc
#define SDC_TOKEN_STOP_TRAN     0xfd

#define SDC_DRESP_MASK          0x1f
#define SDC_DRESP_ACCEPT        0x05
#define SDC_DRESP_ERR_CRC       0x0b
#define SDC_DRESP_ERR_WRITE     0x0d

#define SDC_R1_READY            0x00

#define SDC_R1_IDLE_STATE       0x01
#define SDC_R1_ERASE_RESET      0x02
#define SDC_R1_ILLEGAL_CMD      0x04
#define SDC_R1_ERR_CMD_CRC      0x08
#define SDC_R1_ERR_ERASE_SEQ    0x10
#define SDC_R1_ERR_ADDR         0x20
#define SDC_R1_ERR_PARAM        0x40

#define SDC_NCR                 10

#define SDC_ERR(rv)             ((rv < 0) ? rv : EOS_ERR)

/* CMD */
#define GO_IDLE_STATE           0
#define SEND_OP_COND            1
#define SEND_IF_COND            8
#define SEND_CSD                9
#define STOP_TRANSMISSION       12
#define SET_BLOCKLEN            16
#define READ_SINGLE_BLOCK       17
#define READ_MULTIPLE_BLOCK     18
#define WRITE_BLOCK             24
#define WRITE_MULTIPLE_BLOCK    25
#define ERASE_WR_BLK_START      32
#define ERASE_WR_BLK_END        33
#define ERASE                   38
#define APP_CMD                 55
#define READ_OCR                58

/* ACMD */
#define SD_STATUS               13
#define SET_WR_BLK_ERASE_COUNT  23
#define SD_APP_OP_COND          41

static uint8_t sdc_type = 0;

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

static uint8_t sdc_xchg8(uint8_t data) {
    return eos_spi_xchg8(data, 0);
}

static uint16_t sdc_xchg16(uint16_t data) {
    return eos_spi_xchg16(data, 0);
}

static uint32_t sdc_xchg32(uint32_t data) {
    return eos_spi_xchg32(data, 0);
}

static void sdc_buf_send(unsigned char *buffer, uint16_t len) {
    int i;

    for (i=0; i<len; i++) {
        sdc_xchg8(buffer[i]);
    }
}

static void sdc_buf_recv(unsigned char *buffer, uint16_t len) {
    int i;

    for (i=0; i<len; i++) {
        buffer[i] = sdc_xchg8(0xff);
    }
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
    if (flags & SDC_CMD_FLAG_RSTUFF) sdc_xchg8(0xff);

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

    sdc_buf_recv(buffer, len);
    sdc_xchg16(0xffff); /* dummy CRC */

    return EOS_OK;
}

static int sdc_block_write(uint8_t token, uint8_t *buffer, uint16_t len, uint32_t timeout) {
    uint8_t d;
    int rv;

    rv = sdc_ready(timeout);
    if (rv) return rv;

    sdc_xchg8(token);
    if (buffer && len) {
        sdc_buf_send(buffer, len);
        sdc_xchg16(0xffff); /* dummy CRC */

        d = sdc_xchg8(0xff); /* Response */
        if ((d & SDC_DRESP_MASK) != SDC_DRESP_ACCEPT) return EOS_ERR;
    }

    return EOS_OK;
}

static int sdc_cmd(uint8_t cmd, uint32_t arg, uint8_t flags, uint32_t timeout) {
    int do_cs = !(flags & SDC_CMD_FLAG_NOCS);
    int do_wait = !(flags & SDC_CMD_FLAG_NOWAIT);
    int rv = EOS_OK;

    if (do_cs) sdc_select();
    if (do_wait) rv = sdc_ready(timeout);
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

    start = eos_time_get_tick();
    rv = sdc_cmd(APP_CMD, 0, flags, timeout);
    if (rv & ~SDC_R1_IDLE_STATE) return rv;

    if (flags & SDC_CMD_FLAG_NOCS) {
        sdc_deselect();
        sdc_select();
    }
    return sdc_cmd(cmd, arg, flags, sdc_nto(start, timeout));
}

static int sdc_init(uint32_t timeout) {
    int rv, i;
    uint8_t _type;
    uint8_t ocr[4];
    uint64_t start;
    start = eos_time_get_tick();

    eos_time_sleep(100);
    for (i=10; i--;) sdc_xchg8(0xff);     /* 80 dummy cycles */

    rv = sdc_cmd(GO_IDLE_STATE, 0, SDC_CMD_FLAG_CRC, SDC_TIMEOUT_CMD);
    if (rv != SDC_R1_IDLE_STATE) return SDC_ERR(rv);

    sdc_select();
    rv = sdc_cmd(SEND_IF_COND, 0x1aa, SDC_CMD_FLAG_CRC | SDC_CMD_FLAG_NOCS, sdc_nto(start, timeout));
    switch (rv) {
        case SDC_R1_IDLE_STATE:
            for (i=0; i<4; i++) {   /* R7 response */
                ocr[i] = sdc_xchg8(0xff);
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
                    for (i=0; i<4; i++) {   /* R7 response */
                        ocr[i] = sdc_xchg8(0xff);
                    }
                    sdc_deselect();
                } else {
                    sdc_deselect();
                    return SDC_ERR(rv);
                }

                _type = EOS_SDC_TYPE_SDC2;
                if (ocr[0] & 0xc0) _type |= EOS_SDC_CAP_BLK;
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
                    _type = EOS_SDC_TYPE_SDC1;
                    break;

                default:
                    do {
                        rv = sdc_cmd(SEND_OP_COND, 0, 0, sdc_nto(start, timeout));
                    } while (rv == SDC_R1_IDLE_STATE);
                    if (rv != SDC_R1_READY) return SDC_ERR(rv);
                    _type = EOS_SDC_TYPE_MMC;
                    break;

            }
            break;

        default:
            sdc_deselect();
            return SDC_ERR(rv);
    }

    if (!(_type & EOS_SDC_CAP_BLK)) {
        rv = sdc_cmd(SET_BLOCKLEN, 512, 0, sdc_nto(start, timeout));
        if (rv != SDC_R1_READY) return SDC_ERR(rv);
    }

    if (_type & EOS_SDC_TYPE_SDC) {
        uint8_t csd[16];

        sdc_select();
        rv = sdc_cmd(SEND_CSD, 0, SDC_CMD_FLAG_NOCS, sdc_nto(start, timeout));
        if (rv == SDC_R1_READY) {
            rv = sdc_block_read(csd, 16, sdc_nto(start, timeout));
        } else {
            rv = SDC_ERR(rv);
        }
        sdc_deselect();
        if (rv) return rv;

        for (i=0; i<16; i++) {
            printf("%.2x ", csd[i]);
        }
        printf("\n");
        if (csd[10] & 0x40) _type |= EOS_SDC_CAP_ERASE_EN;
    }

    eos_spi_set_div(EOS_SPI_DEV_SDC, 5);
    sdc_type = _type;
    return EOS_OK;
}

void eos_sdc_init(uint8_t wakeup_cause) {
    int rv;

    eos_spi_select(EOS_SPI_DEV_SDC);
    rv = sdc_init(5000);
    if (rv) {
        printf("SDC ERROR\n");
    } else {
        printf("SDC OK:%x\n", sdc_type);
    }
    eos_spi_deselect();
}

uint8_t eos_sdc_type(void) {
    return sdc_type & EOS_SDC_TYPE_MASK;
}

uint8_t eos_sdc_cap(void) {
    return sdc_type & EOS_SDC_CAP_MASK;
}

int eos_sdc_get_sect_count(uint32_t timeout, uint32_t *sectors) {
    int rv;
    uint8_t csd[16];
    uint64_t start = eos_time_get_tick();

    sdc_select();
    rv = sdc_cmd(SEND_CSD, 0, SDC_CMD_FLAG_NOCS, timeout);
    if (rv == SDC_R1_READY) {
        rv = sdc_block_read(csd, 16, sdc_nto(start, timeout));
    } else {
        rv = SDC_ERR(rv);
    }
    sdc_deselect();
    if (rv) return rv;

    if ((csd[0] >> 6) == 1) {   /* SDCv2 */
        *sectors = (csd[9] + (csd[8] << 8) + ((csd[7] & 0x3f) << 16) + 1) << 10;
    } else {                    /* SDCv1 or MMC*/
        uint8_t n = (csd[5] & 0x0f) + (csd[10] >> 7) + ((csd[9] & 0x03) << 1) + 2;
        *sectors = (csd[8] >> 6) + (csd[7] << 2) + ((csd[6] & 0x03) << 10) + 1;
        *sectors = *sectors << (n - 9);
    }

    return EOS_OK;
}

int eos_sdc_get_blk_size(uint32_t timeout, uint32_t *size) {
    int rv;
    uint8_t rbl[64];    /* SD Status or CSD register */
    uint64_t start = eos_time_get_tick();

    sdc_select();
    if (sdc_type & EOS_SDC_TYPE_SDC2) {
        rv = sdc_acmd(SD_STATUS, 0, SDC_CMD_FLAG_NOCS, timeout);
        sdc_xchg8(0xff);    /* R2 response */
    } else {
        rv = sdc_cmd(SEND_CSD, 0, SDC_CMD_FLAG_NOCS, timeout);
    }
    if (rv == SDC_R1_READY) {
        rv = sdc_block_read(rbl, (sdc_type & EOS_SDC_TYPE_SDC2) ? 64 : 16, sdc_nto(start, timeout));
    } else {
        rv = SDC_ERR(rv);
    }
    sdc_deselect();
    if (rv) return rv;

    if (sdc_type & EOS_SDC_TYPE_SDC2) {
        *size = 16UL << (rbl[10] >> 4);
    } else if (sdc_type & EOS_SDC_TYPE_SDC1) {
        *size = (((rbl[10] & 0x3f) << 1) + (rbl[11] >> 7) + 1) << ((rbl[13] >> 6) - 1);
    } else {
        *size = (((rbl[10] & 0x7c) >> 2) + 1) * (((rbl[11] & 0x03) << 3) + (rbl[11] >> 5) + 1);
    }

    return EOS_OK;
}

int eos_sdc_sync(uint32_t timeout) {
    int rv;

    sdc_select();
    rv = sdc_ready(timeout);
    sdc_deselect();

    return rv;
}

int eos_sdc_erase(uint32_t blk_start, uint32_t blk_end, uint32_t timeout) {
    int rv;
    uint64_t start;

    if (!(sdc_type & EOS_SDC_TYPE_SDC)) return EOS_ERR;
    if (!(sdc_type & EOS_SDC_CAP_ERASE_EN)) return EOS_ERR;
    if (!(sdc_type & EOS_SDC_CAP_BLK)) {
        blk_start *= 512;
        blk_end *= 512;
    }

    start = eos_time_get_tick();
    rv = sdc_cmd(ERASE_WR_BLK_START, blk_start, 0, timeout);
    if (rv != SDC_R1_READY) return SDC_ERR(rv);

    rv = sdc_cmd(ERASE_WR_BLK_END, blk_end, 0, sdc_nto(start, timeout));
    if (rv != SDC_R1_READY) return SDC_ERR(rv);

    rv = sdc_cmd(ERASE, 0, 0, sdc_nto(start, timeout));
    if (rv != SDC_R1_READY) return SDC_ERR(rv);

    return eos_sdc_sync(sdc_nto(start, timeout));
}

int eos_sdc_sect_read(uint32_t sect, unsigned int count, uint8_t *buffer) {
    int rv;
    uint64_t start;
    uint8_t cmd = ((count == 1) ? READ_SINGLE_BLOCK : READ_MULTIPLE_BLOCK);

    if (!(sdc_type & EOS_SDC_CAP_BLK)) sect *= 512;
    start = eos_time_get_tick();

    sdc_select();
    rv = sdc_cmd(cmd, sect, SDC_CMD_FLAG_NOCS, SDC_TIMEOUT_CMD);
    if (rv == SDC_R1_READY) {
        int _rv = SDC_R1_READY;

        while (count) {
            rv = sdc_block_read(buffer, 512, SDC_TIMEOUT_READ);
            if (rv) break;
            eos_sdcc_decrypt(sect, buffer);
            buffer += 512;
            count--;
        }
        if (cmd == READ_MULTIPLE_BLOCK) _rv = sdc_cmd(STOP_TRANSMISSION, 0, SDC_CMD_FLAG_NOCS | SDC_CMD_FLAG_NOWAIT | SDC_CMD_FLAG_RSTUFF, 0);
        if (!rv && (_rv != SDC_R1_READY)) rv = SDC_ERR(_rv);
    } else {
        rv = SDC_ERR(rv);
    }
    sdc_deselect();

    return rv;
}

int eos_sdc_sect_write(uint32_t sect, unsigned int count, uint8_t *buffer) {
    int rv;
    uint64_t start;

    if (!(sdc_type & EOS_SDC_CAP_BLK)) sect *= 512;
    start = eos_time_get_tick();

    if (count == 1) {
        sdc_select();
        rv = sdc_cmd(WRITE_BLOCK, sect, SDC_CMD_FLAG_NOCS, SDC_TIMEOUT_CMD);
        if (rv == SDC_R1_READY) {
            eos_sdcc_encrypt(sect, buffer);
            rv = sdc_block_write(SDC_TOKEN_START_BLK, buffer, 512, SDC_TIMEOUT_WRITE);
        } else {
            rv = SDC_ERR(rv);
        }
        sdc_deselect();
    } else {
        if (sdc_type & EOS_SDC_TYPE_SDC) {
            rv = sdc_acmd(SET_WR_BLK_ERASE_COUNT, count, 0, SDC_TIMEOUT_CMD);
            if (rv != SDC_R1_READY) return SDC_ERR(rv);
        }

        sdc_select();
        rv = sdc_cmd(WRITE_MULTIPLE_BLOCK, sect, SDC_CMD_FLAG_NOCS, SDC_TIMEOUT_CMD);
        if (rv == SDC_R1_READY) {
            int _rv;

            while (count) {
                eos_sdcc_encrypt(sect, buffer);
                rv = sdc_block_write(SDC_TOKEN_START_BLKM, buffer, 512, SDC_TIMEOUT_WRITE);
                if (rv) break;
                buffer += 512;
                count--;
            }
            _rv = sdc_block_write(SDC_TOKEN_STOP_TRAN, NULL, 0, SDC_TIMEOUT_WRITE);
            if (!rv && (_rv != SDC_R1_READY)) rv = SDC_ERR(_rv);
        } else {
            rv = SDC_ERR(rv);
        }
        sdc_deselect();
    }

    return rv;
}
