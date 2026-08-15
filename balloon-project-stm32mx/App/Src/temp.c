/**
 * @file temp.c
 * @brief MAX31865 RTD driver — configuration (F4.1).
 */

#include "temp.h"

#include "error_flags.h"
#include "main.h"
#include "spi_bus.h"

/** Configuration register (datasheet address 0x00). */
#define TEMP_REG_CONFIG             0x00u

/** VBIAS on (bit 7). */
#define TEMP_CFG_VBIAS              0x80u

/** 3-wire RTD (bit 4). */
#define TEMP_CFG_3WIRE              0x10u

/** Fault status clear — write-1-to-clear (bit 1). */
#define TEMP_CFG_FAULT_CLR          0x02u

/** Write value: VBIAS | 3WIRE | FAULT_CLR (normally off, 60 Hz, no 1-shot). */
#define TEMP_CFG_WRITE              (TEMP_CFG_VBIAS | TEMP_CFG_3WIRE | TEMP_CFG_FAULT_CLR)

/** Expected read-back sticky bits after init (VBIAS | 3WIRE). */
#define TEMP_CFG_READBACK_EXPECT    (TEMP_CFG_VBIAS | TEMP_CFG_3WIRE)

/** Mask for read-back compare (exclude write-only / self-clearing bits). */
#define TEMP_CFG_READBACK_MASK      TEMP_CFG_READBACK_EXPECT

/** Bias settle after VBIAS on (datasheet ≥10.5τ; 10 ms typical). */
#define TEMP_VBIAS_SETTLE_MS        10u

/** Finite HAL SPI timeout for register access. */
#define TEMP_SPI_TIMEOUT_MS         100u

static bool s_ok;

static bool temp_read_reg(uint8_t reg, uint8_t *value)
{
  return spi_bus_read_reg8(Temp_CS_GPIO_Port, Temp_CS_Pin, reg, value,
                           TEMP_SPI_TIMEOUT_MS);
}

static bool temp_write_reg(uint8_t reg, uint8_t value)
{
  return spi_bus_write_reg8(Temp_CS_GPIO_Port, Temp_CS_Pin, reg, value,
                            TEMP_SPI_TIMEOUT_MS);
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

bool temp_is_ok(void)
{
  return s_ok;
}
