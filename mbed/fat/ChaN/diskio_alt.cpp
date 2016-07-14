/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2014        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "diskio.h"
#include "mbed_debug.h"
#include "FATFileSystem.h"
#include "SDFileSystem.h"

using namespace mbed;

const PinName SD_MOSI   = PC_12;
const PinName SD_MISO   = PC_11;
const PinName SD_SCK    = PC_10;
const PinName SD_NSS    = PD_1;

SDFileSystem sd(SD_MOSI, SD_MISO, SD_SCK, SD_NSS, "sd");

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
    BYTE pdrv        /* Physical drive nmuber to identify the drive */
)
{
    return (DSTATUS)sd.disk_status();
}

/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
    BYTE pdrv        /* Physical drive nmuber to identify the drive */
)
{
    return (DSTATUS)sd.disk_initialize();
}

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
    BYTE pdrv,       /* Physical drive nmuber to identify the drive */
    BYTE* buff,      /* Data buffer to store read data */
    DWORD sector,    /* Sector address in LBA */
    UINT count       /* Number of sectors to read */
)
{
    if (sd.disk_read((uint8_t*)buff, sector, count))
        return RES_PARERR;
    else
        return RES_OK;
}

/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if _USE_WRITE
DRESULT disk_write (
    BYTE pdrv,           /* Physical drive nmuber to identify the drive */
    const BYTE* buff,    /* Data to be written */
    DWORD sector,        /* Sector address in LBA */
    UINT count           /* Number of sectors to write */
)
{
    if (sd.disk_write((uint8_t*)buff, sector, count))
        return RES_PARERR;
    else
        return RES_OK;
}
#endif

/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

#if _USE_IOCTL
DRESULT disk_ioctl (
    BYTE pdrv,        /* Physical drive nmuber (0..) */
    BYTE cmd,         /* Control code */
    void* buff        /* Buffer to send/receive control data */
)
{
    switch(cmd) {
        case CTRL_SYNC:
            if(sd.disk_sync()) {
                return RES_ERROR;
            }
            return RES_OK;
        case GET_SECTOR_COUNT:
            {
                DWORD res = sd.disk_sectors();
                if(res > 0) {
                    *((DWORD*)buff) = res; // minimum allowed
                    return RES_OK;
                } else {
                    return RES_ERROR;
                }
            }
        case GET_BLOCK_SIZE:
            *((DWORD*)buff) = 1; // default when not known
            return RES_OK;

    }
    return RES_PARERR;
}
#endif
