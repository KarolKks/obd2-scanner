/*-----------------------------------------------------------------------*/
/* Low level disk I/O module for FatFs                                   */
/*-----------------------------------------------------------------------*/

#include "ff.h"
#include "diskio.h"

/* Definitions of physical drive number for each drive */
#define DEV_MMC   0   /* MMC/SD card drive number */

/* Disk status */
static volatile DSTATUS Stat = STA_NOINIT;

/* Get Drive Status */
DSTATUS disk_status (
    BYTE pdrv        /* Physical drive number */
)
{
    if (pdrv != DEV_MMC) {
        return STA_NOINIT;
    }
    return Stat;
}

/* Initialize a Drive */
DSTATUS disk_initialize (
    BYTE pdrv        /* Physical drive number */
)
{
    if (pdrv != DEV_MMC) {
        return STA_NOINIT;
    }

    /* TODO: Insert SD card SPI initialization here */
    Stat &= ~STA_NOINIT;

    return Stat;
}

/* Read Sector(s) */
DRESULT disk_read (
    BYTE pdrv,       /* Physical drive number */
    BYTE *buff,      /* Data buffer to store read data */
    LBA_t sector,    /* Start sector in LBA */
    UINT count       /* Number of sectors to read */
)
{
    if (pdrv != DEV_MMC) {
        return RES_PARERR;
    }

    (void)buff;
    (void)sector;
    (void)count;

    /* TODO: Insert SD card SPI read sectors implementation here */

    return RES_OK;
}

/* Write Sector(s) */
#if FF_FS_READONLY == 0
DRESULT disk_write (
    BYTE pdrv,          /* Physical drive number */
    const BYTE *buff,   /* Data to be written */
    LBA_t sector,       /* Start sector in LBA */
    UINT count          /* Number of sectors to write */
)
{
    if (pdrv != DEV_MMC) {
        return RES_PARERR;
    }

    (void)buff;
    (void)sector;
    (void)count;

    /* TODO: Insert SD card SPI write sectors implementation here */

    return RES_OK;
}
#endif

/* Miscellaneous Functions */
DRESULT disk_ioctl (
    BYTE pdrv,       /* Physical drive number */
    BYTE cmd,        /* Control code */
    void *buff       /* Buffer to send/receive control data */
)
{
    if (pdrv != DEV_MMC) {
        return RES_PARERR;
    }

    (void)buff;

    switch (cmd) {
    case CTRL_SYNC:
        /* Flush disk cache */
        return RES_OK;

    case GET_SECTOR_COUNT:
        /* Return sector count */
        return RES_OK;

    case GET_SECTOR_SIZE:
        /* Return sector size (if variable sector sizes enabled) */
        return RES_OK;

    case GET_BLOCK_SIZE:
        /* Return erase block size */
        return RES_OK;

    default:
        return RES_PARERR;
    }
}
