#pragma once

/**
 * @file sd_spi.h
 * @brief FatFs diskio glue for microSD over SPI1 (512-byte sectors, SDHC/SDXC).
 */

#include "diskio.h"

DSTATUS SD_SPI_Init(BYTE pdrv);
DSTATUS SD_SPI_Status(BYTE pdrv);
DRESULT SD_SPI_ReadBlocks(BYTE pdrv, BYTE *buff, DWORD sector, UINT count);
DRESULT SD_SPI_WriteBlocks(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count);
DRESULT SD_SPI_Ioctl(BYTE pdrv, BYTE cmd, void *buff);
