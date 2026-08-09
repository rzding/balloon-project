/**
 * @file baro.c
 * @brief MS5611-01BA03 barometer driver — reset and PROM (F3.1).
 */

#include "baro.h"

#include "error_flags.h"
#include "main.h"
#include "spi_bus.h"

/** MS5611 reset command (datasheet). */
#define BARO_CMD_RESET              0x1Eu

/** First PROM read command (addresses 0..7 at 0xA0, 0xA2, … 0xAE). */
#define BARO_CMD_PROM_READ_BASE     0xA0u

/** Minimum reset-to-PROM-ready delay (datasheet: 2.8 ms). */
#define BARO_RESET_DELAY_MS         3u

/** Bytes per PROM read transfer (command + 16-bit result, MSB first). */
#define BARO_PROM_TRANSFER_BYTES    3u

/** Finite HAL SPI timeout for barometer access. */
#define BARO_SPI_TIMEOUT_MS         100u

static bool s_ok;
static uint16_t s_prom[BARO_PROM_WORD_COUNT];

static void baro_set_ok(bool ok)
{
  s_ok = ok;
  error_flags_set_baro_ok(ok);
}

static bool baro_send_reset(void)
{
  uint8_t cmd = BARO_CMD_RESET;

  return spi_bus_transfer(BARO_CS_GPIO_Port, BARO_CS_Pin, &cmd, NULL, 1u,
                          BARO_SPI_TIMEOUT_MS);
}

static bool baro_read_prom_word(uint8_t index, uint16_t *value)
{
  uint8_t tx[BARO_PROM_TRANSFER_BYTES];
  uint8_t rx[BARO_PROM_TRANSFER_BYTES];
  uint8_t cmd = (uint8_t)(BARO_CMD_PROM_READ_BASE + (index * 2u));

  if (value == NULL)
  {
    return false;
  }

  tx[0] = cmd;
  tx[1] = 0u;
  tx[2] = 0u;

  if (!spi_bus_transfer(BARO_CS_GPIO_Port, BARO_CS_Pin, tx, rx,
                        BARO_PROM_TRANSFER_BYTES, BARO_SPI_TIMEOUT_MS))
  {
    return false;
  }

  *value = (uint16_t)(((uint16_t)rx[1] << 8) | (uint16_t)rx[2]);
  return true;
}

static bool baro_read_prom(uint16_t prom[BARO_PROM_WORD_COUNT])
{
  uint8_t i;

  if (prom == NULL)
  {
    return false;
  }

  for (i = 0u; i < BARO_PROM_WORD_COUNT; i++)
  {
    if (!baro_read_prom_word(i, &prom[i]))
    {
      return false;
    }

    if (prom[i] == 0u)
    {
      return false;
    }
  }

  return true;
}

static bool baro_coefficients_valid(const uint16_t prom[BARO_PROM_WORD_COUNT])
{
  uint8_t i;

  for (i = BARO_PROM_C1_INDEX; i <= BARO_PROM_C6_INDEX; i++)
  {
    if (prom[i] == 0u)
    {
      return false;
    }
  }

  return true;
}

bool baro_init(void)
{
  uint8_t i;

  for (i = 0u; i < BARO_PROM_WORD_COUNT; i++)
  {
    s_prom[i] = 0u;
  }

  if (!baro_send_reset())
  {
    baro_set_ok(false);
    return false;
  }

  HAL_Delay(BARO_RESET_DELAY_MS);

  if (!baro_read_prom(s_prom))
  {
    baro_set_ok(false);
    return false;
  }

  if (!baro_coefficients_valid(s_prom))
  {
    baro_set_ok(false);
    return false;
  }

  if (!baro_prom_crc_ok(s_prom))
  {
    baro_set_ok(false);
    return false;
  }

  baro_set_ok(true);
  return true;
}

bool baro_is_ok(void)
{
  return s_ok;
}
