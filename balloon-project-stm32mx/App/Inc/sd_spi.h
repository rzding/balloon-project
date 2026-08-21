#pragma once
// Tells compiler to only read this file once, even if it's included multiple times in the project.  

// Pulls in the DSTATUS (Disk Status) and DRESULT (Disk Result) types from FatFs (pulling from FATFS library)
#include "diskio.h" 

/* 
 * HARDWARE ABSTRACTION LAYER FOR SD CARD OVER SPI
 * pdrv:   Physical drive number (usually 0 since we only have one SD card)
 * buff:   Pointer to the memory array where data is stored
 * sector: The 4096-byte chunk on the SD card we want to access
 * count:  How many sectors to read/write in a row
 */
DSTATUS SD_SPI_Init(BYTE pdrv);
DSTATUS SD_SPI_Status(BYTE pdrv);
DRESULT SD_SPI_ReadBlocks(BYTE pdrv, BYTE *buff, DWORD sector, UINT count);
DRESULT SD_SPI_WriteBlocks(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count);
DRESULT SD_SPI_Ioctl(BYTE pdrv, BYTE cmd, void *buff);

// Tells the computer that the above functions exist somewhere else