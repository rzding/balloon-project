/**
 * @file temp.c
 * @brief MAX31865 RTD driver — configuration (F4.1), RTD read (F4.2), API (F4.3).
 */

#include "temp.h"

#include "error_flags.h"
#include "main.h"
#include "spi_bus.h"

/** Configuration register (datasheet address 0x00). */
#define TEMP_REG_CONFIG             0x00u

/** RTD MSB / LSB registers. */
#define TEMP_REG_RTD_MSB            0x01u
#define TEMP_REG_RTD_LSB            0x02u

/** Fault status register. */
#define TEMP_REG_FAULT_STATUS       0x07u

/** VBIAS on (bit 7). */
#define TEMP_CFG_VBIAS              0x80u

/** 1-shot conversion trigger (bit 5; self-clearing). */
#define TEMP_CFG_1SHOT              0x20u

/** 3-wire RTD (bit 4). */
#define TEMP_CFG_3WIRE              0x10u

/** Fault status clear — write-1-to-clear (bit 1). */
#define TEMP_CFG_FAULT_CLR          0x02u

/** Write value: VBIAS | 3WIRE | FAULT_CLR (normally off, 60 Hz, no 1-shot). */
#define TEMP_CFG_WRITE              (TEMP_CFG_VBIAS | TEMP_CFG_3WIRE | TEMP_CFG_FAULT_CLR)

/** 1-shot trigger: VBIAS | 1SHOT | 3WIRE (no fault clear on each sample). */
#define TEMP_CFG_ONESHOT_WRITE      (TEMP_CFG_VBIAS | TEMP_CFG_1SHOT | TEMP_CFG_3WIRE)

/** Expected read-back sticky bits after init (VBIAS | 3WIRE). */
#define TEMP_CFG_READBACK_EXPECT    (TEMP_CFG_VBIAS | TEMP_CFG_3WIRE)

/** Mask for read-back compare (exclude write-only / self-clearing bits). */
#define TEMP_CFG_READBACK_MASK      TEMP_CFG_READBACK_EXPECT

/** Bias settle after VBIAS on (datasheet ≥10.5τ; 10 ms typical). */
#define TEMP_VBIAS_SETTLE_MS        10u

/** Max 1-shot conversion time @ 60 Hz filter (datasheet 52 ms; rounded up). */
#define TEMP_CONV_60HZ_DELAY_MS     60u

/** Bytes per RTD burst read (addr + MSB + LSB). */
#define TEMP_RTD_BURST_BYTES        3u

/**
 * MAX31865 address bit-7 convention (datasheet Table 1): write = reg | 0x80,
 * read = reg with MSB clear. This is inverted relative to the ICM-42688-P /
 * RFM95W convention in spi_bus_read_reg8 / spi_bus_write_reg8, so this driver
 * frames its own transfers via spi_bus_transfer.
 */
#define TEMP_SPI_REG_WRITE_BIT      0x80u

/** Bytes per single register access (address + data). */
#define TEMP_REG_TRANSFER_BYTES     2u

/** Finite HAL SPI timeout for register access. */
#define TEMP_SPI_TIMEOUT_MS         100u

static bool s_ok;

static bool temp_read_reg(uint8_t reg, uint8_t *value)
{
  uint8_t tx[TEMP_REG_TRANSFER_BYTES];
  uint8_t rx[TEMP_REG_TRANSFER_BYTES];
  bool ok;

  if (value == NULL)
  {
    return false;
  }

  tx[0] = (uint8_t)(reg & (uint8_t)~TEMP_SPI_REG_WRITE_BIT);
  tx[1] = 0u;

  if (!spi_bus_set_mode(SPI_BUS_MODE1_POLARITY, SPI_BUS_MODE1_PHASE))
  {
    return false;
  }

  ok = spi_bus_transfer(Temp_CS_GPIO_Port, Temp_CS_Pin, tx, rx,
                        TEMP_REG_TRANSFER_BYTES, TEMP_SPI_TIMEOUT_MS);

  (void)spi_bus_set_mode(SPI_BUS_MODE0_POLARITY, SPI_BUS_MODE0_PHASE);

  if (!ok)
  {
    return false;
  }

  *value = rx[1];
  return true;
}

static bool temp_write_reg(uint8_t reg, uint8_t value)
{
  uint8_t tx[TEMP_REG_TRANSFER_BYTES];
  bool ok;

  tx[0] = (uint8_t)(reg | TEMP_SPI_REG_WRITE_BIT);
  tx[1] = value;

  if (!spi_bus_set_mode(SPI_BUS_MODE1_POLARITY, SPI_BUS_MODE1_PHASE))
  {
    return false;
  }

  ok = spi_bus_transfer(Temp_CS_GPIO_Port, Temp_CS_Pin, tx, NULL,
                        TEMP_REG_TRANSFER_BYTES, TEMP_SPI_TIMEOUT_MS);

  (void)spi_bus_set_mode(SPI_BUS_MODE0_POLARITY, SPI_BUS_MODE0_PHASE);
  return ok;
}

static void temp_set_ok(bool ok)
{
  s_ok = ok;
  error_flags_set_temp_ok(ok);
}

static bool temp_configure(void)
{
  uint8_t readback = 0u;

  if (!temp_write_reg(TEMP_REG_CONFIG, TEMP_CFG_WRITE))
  {
    return false;
  }

  HAL_Delay(TEMP_VBIAS_SETTLE_MS);

  if (!temp_read_reg(TEMP_REG_CONFIG, &readback))
  {
    return false;
  }

  return (readback & TEMP_CFG_READBACK_MASK) == TEMP_CFG_READBACK_EXPECT;
}

static bool temp_trigger_oneshot(void)
{
  return temp_write_reg(TEMP_REG_CONFIG, TEMP_CFG_ONESHOT_WRITE);
}

static bool temp_burst_read_rtd(uint8_t *msb, uint8_t *lsb)
{
  uint8_t tx[TEMP_RTD_BURST_BYTES];
  uint8_t rx[TEMP_RTD_BURST_BYTES];
  bool ok;

  if (msb == NULL || lsb == NULL)
  {
    return false;
  }

  tx[0] = (uint8_t)(TEMP_REG_RTD_MSB & (uint8_t)~TEMP_SPI_REG_WRITE_BIT);
  tx[1] = 0u;
  tx[2] = 0u;

  if (!spi_bus_set_mode(SPI_BUS_MODE1_POLARITY, SPI_BUS_MODE1_PHASE))
  {
    return false;
  }

  ok = spi_bus_transfer(Temp_CS_GPIO_Port, Temp_CS_Pin, tx, rx, TEMP_RTD_BURST_BYTES,
                        TEMP_SPI_TIMEOUT_MS);

  (void)spi_bus_set_mode(SPI_BUS_MODE0_POLARITY, SPI_BUS_MODE0_PHASE);

  if (!ok)
  {
    return false;
  }

  *msb = rx[1];
  *lsb = rx[2];
  return true;
}

bool temp_init(void)
{
  if (!temp_configure())
  {
    temp_set_ok(false);
    return false;
  }

  temp_set_ok(true);
  return true;
}

bool temp_read_raw(temp_raw_t *out)
{
  uint8_t msb = 0u;
  uint8_t lsb = 0u;

  if (out == NULL)
  {
    return false;
  }

  if (!temp_trigger_oneshot())
  {
    temp_set_ok(false);
    return false;
  }

  HAL_Delay(TEMP_CONV_60HZ_DELAY_MS);

  if (!temp_burst_read_rtd(&msb, &lsb))
  {
    temp_set_ok(false);
    return false;
  }

  if (!temp_rtd_unpack(msb, lsb, out))
  {
    temp_set_ok(false);
    return false;
  }

  if (out->fault)
  {
    (void)temp_read_reg(TEMP_REG_FAULT_STATUS, &out->fault_status);
    temp_set_ok(false);
    return false;
  }

  if (out->adc == 0u)
  {
    temp_set_ok(false);
    return false;
  }

  temp_set_ok(true);
  return true;
}

bool temp_read(temp_sample_t *out)
{
  temp_raw_t raw;

  if (out == NULL)
  {
    return false;
  }

  if (!temp_read_raw(&raw))
  {
    return false;
  }

  if (!temp_sample_from_raw(&raw, out))
  {
    temp_set_ok(false);
    return false;
  }

  temp_set_ok(true);
  return true;
}

bool temp_is_ok(void)
{
  return s_ok;
}
