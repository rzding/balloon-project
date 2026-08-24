/**
 * @file sd_spi.c
 * @brief SPI-mode SD block driver for FatFs (SDHC/SDXC; 512-byte sectors).
 *
 * CS policy (roadmap §3.3 / F6): SD SPI frames need CS held for the whole
 * cmd/R1/data/CRC sequence — not the IMU-style per-transfer toggle. We acquire
 * the shared spi_bus, assert microSD_CS for that transaction only, transfer with
 * cs_port NULL, then deassert CS, clock one dummy 0xFF, and release. CS is never
 * left low between FatFs operations.
 */

#include "sd_spi.h"
#include "spi_bus.h"
#include "main.h"

/* SD SPI init must be <= 400 kHz (APB2 100 MHz / 256 ~ 390 kHz). */
#define SD_SPI_INIT_PRESCALER SPI_BAUDRATEPRESCALER_256

static bool s_sd_cs_held;

static void SD_Select(void)
{
  if (!s_sd_cs_held)
  {
    if (!spi_bus_acquire())
    {
      return;
    }
    s_sd_cs_held = true;
  }
  HAL_GPIO_WritePin(microSD_CS_GPIO_Port, microSD_CS_Pin, GPIO_PIN_RESET);
}

static void SD_Deselect(void)
{
  HAL_GPIO_WritePin(microSD_CS_GPIO_Port, microSD_CS_Pin, GPIO_PIN_SET);

  if (s_sd_cs_held)
  {
    uint8_t dummy = 0xFF;
    /* Spec: one dummy clock byte after CS rises; still inside acquire. */
    (void)spi_bus_transfer(NULL, 0, &dummy, NULL, 1, 10);
    s_sd_cs_held = false;
    (void)spi_bus_release();
  }
}

static uint8_t SD_TxRxByte(uint8_t data)
{
  uint8_t rx_data = 0xFF;
  (void)spi_bus_transfer(NULL, 0, &data, &rx_data, 1, 100);
  return rx_data;
}

#define CMD0   (0)
#define CMD8   (8)
#define CMD17  (17)
#define CMD24  (24)
#define CMD55  (55)
#define ACMD41 (41 | 0x80)

static uint8_t SD_SendCmd(uint8_t cmd, uint32_t arg)
{
  uint8_t n;
  uint8_t res;

  if (cmd & 0x80)
  {
    cmd &= 0x7F;
    res = SD_SendCmd(CMD55, 0);
    if (res > 1)
    {
      return res;
    }
  }

  SD_Select();

  SD_TxRxByte(cmd | 0x40);
  SD_TxRxByte((uint8_t)(arg >> 24));
  SD_TxRxByte((uint8_t)(arg >> 16));
  SD_TxRxByte((uint8_t)(arg >> 8));
  SD_TxRxByte((uint8_t)arg);

  n = 0x01;
  if (cmd == CMD0)
  {
    n = 0x95;
  }
  if (cmd == CMD8)
  {
    n = 0x87;
  }
  SD_TxRxByte(n);

  n = 10;
  do
  {
    res = SD_TxRxByte(0xFF);
  } while ((res & 0x80) && --n);

  return res;
}

static void SD_RestoreClock(void)
{
  (void)spi_bus_set_prescaler(SPI_BUS_DEFAULT_PRESCALER);
}

DSTATUS SD_SPI_Init(BYTE pdrv)
{
  uint8_t n;
  uint8_t ocr[4];
  uint16_t timeout;
  DSTATUS st = STA_NOINIT;

  (void)pdrv;

  if (!spi_bus_set_prescaler(SD_SPI_INIT_PRESCALER))
  {
    return STA_NOINIT;
  }

  HAL_Delay(10);
  HAL_GPIO_WritePin(microSD_CS_GPIO_Port, microSD_CS_Pin, GPIO_PIN_SET);

  /* >= 74 clocks with CS high before CMD0. */
  if (!spi_bus_acquire())
  {
    SD_RestoreClock();
    return STA_NOINIT;
  }
  for (n = 10; n; n--)
  {
    SD_TxRxByte(0xFF);
  }
  (void)spi_bus_release();

  if (SD_SendCmd(CMD0, 0) == 1)
  {
    if (SD_SendCmd(CMD8, 0x1AA) == 1)
    {
      for (n = 0; n < 4; n++)
      {
        ocr[n] = SD_TxRxByte(0xFF);
      }

      if (ocr[2] == 0x01 && ocr[3] == 0xAA)
      {
        for (timeout = 1000; timeout; timeout--)
        {
          if (SD_SendCmd(ACMD41, 1UL << 30) == 0)
          {
            break;
          }
          HAL_Delay(1);
        }

        if (timeout)
        {
          st = 0;
        }
      }
    }
  }

  SD_Deselect();
  SD_RestoreClock();
  return st;
}

DSTATUS SD_SPI_Status(BYTE pdrv)
{
  (void)pdrv;
  /*
   * Roadmap §2: detect high = card present (not switch-to-GND polarity).
   */
  if (HAL_GPIO_ReadPin(microSD_detect_GPIO_Port, microSD_detect_Pin) == GPIO_PIN_SET)
  {
    return 0;
  }
  return STA_NODISK;
}

DRESULT SD_SPI_ReadBlocks(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
  uint16_t timeout;

  (void)pdrv;

  for (UINT i = 0; i < count; i++)
  {
    if (SD_SendCmd(CMD17, sector + i) != 0)
    {
      SD_Deselect();
      return RES_ERROR;
    }

    timeout = 10000;
    while (SD_TxRxByte(0xFF) != 0xFE)
    {
      if (--timeout == 0)
      {
        SD_Deselect();
        return RES_ERROR;
      }
    }

    for (uint16_t j = 0; j < 512; j++)
    {
      *buff++ = SD_TxRxByte(0xFF);
    }

    SD_TxRxByte(0xFF);
    SD_TxRxByte(0xFF);
  }

  SD_Deselect();
  return RES_OK;
}

DRESULT SD_SPI_WriteBlocks(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
  uint16_t timeout;

  (void)pdrv;

  for (UINT i = 0; i < count; i++)
  {
    if (SD_SendCmd(CMD24, sector + i) != 0)
    {
      SD_Deselect();
      return RES_ERROR;
    }

    SD_TxRxByte(0xFF);
    SD_TxRxByte(0xFE);

    for (uint16_t j = 0; j < 512; j++)
    {
      SD_TxRxByte(*buff++);
    }

    SD_TxRxByte(0xFF);
    SD_TxRxByte(0xFF);

    if ((SD_TxRxByte(0xFF) & 0x1F) != 0x05)
    {
      SD_Deselect();
      return RES_ERROR;
    }

    timeout = 10000;
    while (SD_TxRxByte(0xFF) == 0x00)
    {
      if (--timeout == 0)
      {
        SD_Deselect();
        return RES_ERROR;
      }
    }
  }

  SD_Deselect();
  return RES_OK;
}

DRESULT SD_SPI_Ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
  DRESULT res = RES_ERROR;
  uint8_t csd[16];
  uint32_t c_size;

  if (pdrv != 0)
  {
    return RES_PARERR;
  }

  switch (cmd)
  {
    case CTRL_SYNC:
      SD_Select();
      if (SD_TxRxByte(0xFF) == 0xFF)
      {
        res = RES_OK;
      }
      SD_Deselect();
      break;

    case GET_SECTOR_COUNT:
      if (SD_SendCmd(9, 0) == 0)
      {
        uint16_t timeout = 10000;
        while (SD_TxRxByte(0xFF) != 0xFE && --timeout)
        {
        }

        if (timeout)
        {
          for (uint8_t i = 0; i < 16; i++)
          {
            csd[i] = SD_TxRxByte(0xFF);
          }
          SD_TxRxByte(0xFF);
          SD_TxRxByte(0xFF);

          /* SDHC/SDXC CSD v2.0 C_SIZE → sector count (flight: SDHC only). */
          c_size = ((uint32_t)(csd[7] & 0x3F) << 16) | ((uint16_t)csd[8] << 8) | csd[9];
          *(DWORD *)buff = (c_size + 1) * 1024;
          res = RES_OK;
        }
      }
      SD_Deselect();
      break;

    case GET_SECTOR_SIZE:
      *(WORD *)buff = 512;
      res = RES_OK;
      break;

    case GET_BLOCK_SIZE:
      *(DWORD *)buff = 1;
      res = RES_OK;
      break;

    default:
      res = RES_PARERR;
      break;
  }

  return res;
}
