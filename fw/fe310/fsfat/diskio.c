/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "ff.h"         /* Obtains integer types */
#include "diskio.h"     /* Declarations of disk functions */

#include <eos/sdcard.h>

#define TIMEOUT         1000
#define TIMEOUT_TRIM    60000

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
    BYTE pdrv           /* Physical drive nmuber to identify the drive */
)
{
    if (pdrv || (eos_sdc_type() == 0)) return STA_NOINIT;
    return 0;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
    BYTE pdrv           /* Physical drive nmuber to identify the drive */
)
{
    if (pdrv || (eos_sdc_type() == 0)) return STA_NOINIT;
    return 0;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
    BYTE pdrv,          /* Physical drive nmuber to identify the drive */
    BYTE *buff,         /* Data buffer to store read data */
    LBA_t sector,       /* Start sector in LBA */
    UINT count          /* Number of sectors to read */
)
{
    int rv;

    if (pdrv) return RES_PARERR;
    if (eos_sdc_type() == 0) return RES_NOTRDY;

    rv = eos_sdc_sect_read(sector, count, (uint8_t *)buff);
    return rv ? RES_ERROR : RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (
    BYTE pdrv,          /* Physical drive nmuber to identify the drive */
    const BYTE *buff,   /* Data to be written */
    LBA_t sector,       /* Start sector in LBA */
    UINT count          /* Number of sectors to write */
)
{
    int rv;

    if (pdrv) return RES_PARERR;
    if (eos_sdc_type() == 0) return RES_NOTRDY;

    rv = eos_sdc_sect_write(sector, count, (uint8_t *)buff);
    return rv ? RES_ERROR : RES_OK;
}

#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
    BYTE pdrv,          /* Physical drive nmuber (0..) */
    BYTE cmd,           /* Control code */
    void *buff          /* Buffer to send/receive control data */
)
{
    DRESULT res;
    uint32_t ret;
    int rv;

    if (pdrv) return RES_PARERR;
    if (eos_sdc_type() == 0) return RES_NOTRDY;

    res = RES_ERROR;

    switch (cmd) {
        case CTRL_SYNC:
            rv = eos_sdc_sync(TIMEOUT);
            if (!rv) res = RES_OK;
            break;

        case GET_SECTOR_COUNT:
            rv = eos_sdc_get_sect_count(TIMEOUT, &ret);
            if (!rv) {
                *(LBA_t*)buff = ret;
                res = RES_OK;
            }
            break;

        case GET_BLOCK_SIZE:
            rv = eos_sdc_get_blk_size(TIMEOUT, &ret);
            if (!rv) {
                *(DWORD*)buff = ret;
                res = RES_OK;
            }
            break;

        case CTRL_TRIM:
            rv = eos_sdc_erase(((LBA_t*)buff)[0], ((LBA_t*)buff)[1], TIMEOUT_TRIM);
            if (!rv) res = RES_OK;
            break;
    }

    return res;
}
