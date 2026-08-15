/**
 * @file baro.c
 * @brief MS5611-01BA03 barometer driver — reset/PROM (F3.1), conversion (F3.2),
 * compensation helpers in baro.h (F3.3), baro_read API (F3.4).
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

/** ADC read command (datasheet). */
#define BARO_CMD_ADC_READ           0x00u

/** D1 pressure conversion, OSR 4096 (0x40 | 0x08). */
#define BARO_CMD_CONV_D1_OSR4096    0x48u

/** D2 temperature conversion, OSR 4096 (0x50 | 0x08). */
#define BARO_CMD_CONV_D2_OSR4096    0x58u

/** Max conversion time OSR 4096 (datasheet 9.04 ms); rounded up for SysTick. */
#define BARO_CONV_OSR4096_DELAY_MS  10u

/** Bytes per ADC read transfer (command + 24-bit result, MSB first). */
#define BARO_ADC_TRANSFER_BYTES     4u

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

static bool baro_send_command(uint8_t cmd)
{
  return spi_bus_transfer(BARO_CS_GPIO_Port, BARO_CS_Pin, &cmd, NULL, 1u,
                          BARO_SPI_TIMEOUT_MS);
}

static bool baro_read_adc(uint32_t *adc)
{
  uint8_t tx[BARO_ADC_TRANSFER_BYTES];
  uint8_t rx[BARO_ADC_TRANSFER_BYTES];
  uint8_t i;

  if (adc == NULL)
  {
    return false;
  }

  for (i = 0u; i < BARO_ADC_TRANSFER_BYTES; i++)
  {
    tx[i] = BARO_CMD_ADC_READ;
  }

  if (!spi_bus_transfer(BARO_CS_GPIO_Port, BARO_CS_Pin, tx, rx,
                        BARO_ADC_TRANSFER_BYTES, BARO_SPI_TIMEOUT_MS))
  {
    return false;
  }

  *adc = baro_be_bytes_to_u24(rx[1], rx[2], rx[3]);
  return true;
}

static bool baro_convert_and_read_adc(uint8_t conv_cmd, uint32_t *adc)
{
  if (adc == NULL)
  {
    return false;
  }

  if (!baro_send_command(conv_cmd))
  {
    return false;
  }

  HAL_Delay(BARO_CONV_OSR4096_DELAY_MS);

  if (!baro_read_adc(adc))
  {
    return false;
  }

  return *adc != 0u;
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

bool baro_read_raw(baro_raw_t *out)
{
  if (out == NULL)
  {
    return false;
  }

  if (!baro_convert_and_read_adc(BARO_CMD_CONV_D1_OSR4096, &out->d1))
  {
    baro_set_ok(false);
    return false;
  }

  if (!baro_convert_and_read_adc(BARO_CMD_CONV_D2_OSR4096, &out->d2))
  {
    baro_set_ok(false);
    return false;
  }

  baro_set_ok(true);
  return true;
}

bool baro_read(baro_sample_t *out)
{
  baro_raw_t raw;

  if (out == NULL)
  {
    return false;
  }

  if (!baro_coefficients_valid(s_prom))
  {
    baro_set_ok(false);
    return false;
  }

  if (!baro_read_raw(&raw))
  {
    return false;
  }

  if (!baro_sample_from_raw(s_prom, &raw, out))
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
