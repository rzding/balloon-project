/**
 * @file lora.c
 * @brief RFM95W-915S2 (SX1276) LoRa driver — reset/probe (F7.1), modem config (F7.2).
 */

#include "lora.h"

#include "error_flags.h"
#include "main.h"
#include "spi_bus.h"

/** SX1276 register addresses (LoRa mode). */
#define LORA_REG_OPMODE             0x01u
#define LORA_REG_FRF_MSB            0x06u
#define LORA_REG_FRF_MID            0x07u
#define LORA_REG_FRF_LSB            0x08u
#define LORA_REG_PACONFIG           0x09u
#define LORA_REG_OCP                0x0Bu
#define LORA_REG_PREAMBLE_MSB       0x20u
#define LORA_REG_PREAMBLE_LSB       0x21u
#define LORA_REG_MODEMCONFIG1       0x1Du
#define LORA_REG_MODEMCONFIG2       0x1Eu
#define LORA_REG_MODEMCONFIG3       0x26u
#define LORA_REG_SYNCWORD           0x39u
#define LORA_REG_VERSION            0x42u

/** Expected RegVersion for SX1276 silicon. */
#define LORA_VERSION_EXPECT         0x12u

/** OpMode: Sleep + LoRa (LongRangeMode set only in Sleep). */
#define LORA_OPMODE_SLEEP_LORA      0x80u

/** OpMode: Standby + LoRa. */
#define LORA_OPMODE_STANDBY_LORA    0x81u

/** ModemConfig1: BW125 kHz | CR4/5 | explicit header. */
#define LORA_MODEMCONFIG1           0x72u

/** ModemConfig2: SF8 | CRC on | no TxContinuous. */
#define LORA_MODEMCONFIG2           0x84u

/** ModemConfig3: AGC auto on, no LDR optimize (SF8/BW125). */
#define LORA_MODEMCONFIG3           0x04u

/** PaConfig: PA_BOOST | OutputPower=15 (+17 dBm). */
#define LORA_PACONFIG               0x8Fu

/** Ocp: on, trim ~100 mA (45 + 11*5). */
#define LORA_OCP                    0x2Bu

/** Delay after Sleep+LoRa before LoRa register writes (ms). */
#define LORA_SLEEP_SETTLE_MS        1u

/** Active-low RESET pulse width (ms); >100 us per datasheet. */
#define LORA_RESET_PULSE_MS         1u

/** Post-reset POR wait before SPI (ms); datasheet >= 5 ms. */
#define LORA_RESET_POR_MS           10u

/** Finite HAL SPI timeout for register access. */
#define LORA_SPI_TIMEOUT_MS         100u

static bool s_ok;
static uint8_t s_version;

static bool lora_read_reg(uint8_t reg, uint8_t *value)
{
  return spi_bus_read_reg8(LoRa_CS_GPIO_Port, LoRa_CS_Pin, reg, value,
                           LORA_SPI_TIMEOUT_MS);
}

static bool lora_write_reg(uint8_t reg, uint8_t value)
{
  return spi_bus_write_reg8(LoRa_CS_GPIO_Port, LoRa_CS_Pin, reg, value,
                            LORA_SPI_TIMEOUT_MS);
}

static bool lora_write_verify(uint8_t reg, uint8_t value)
{
  uint8_t readback = 0u;

  if (!lora_write_reg(reg, value))
  {
    return false;
  }

  if (!lora_read_reg(reg, &readback))
  {
    return false;
  }

  return readback == value;
}

static void lora_set_ok(bool ok)
{
  s_ok = ok;
  error_flags_set_lora_ok(ok);
}

static void lora_hw_reset(void)
{
  HAL_GPIO_WritePin(LoRa_RESET_GPIO_Port, LoRa_RESET_Pin, GPIO_PIN_RESET);
  HAL_Delay(LORA_RESET_PULSE_MS);
  HAL_GPIO_WritePin(LoRa_RESET_GPIO_Port, LoRa_RESET_Pin, GPIO_PIN_SET);
  HAL_Delay(LORA_RESET_POR_MS);
}

static bool lora_check_version(void)
{
  uint8_t id = 0u;

  if (!lora_read_reg(LORA_REG_VERSION, &id))
  {
    return false;
  }

  s_version = id;
  return id == LORA_VERSION_EXPECT;
}

static bool lora_write_frf(uint32_t frf)
{
  if (!lora_write_reg(LORA_REG_FRF_MSB, (uint8_t)((frf >> 16) & 0xFFu)))
  {
    return false;
  }

  if (!lora_write_reg(LORA_REG_FRF_MID, (uint8_t)((frf >> 8) & 0xFFu)))
  {
    return false;
  }

  return lora_write_reg(LORA_REG_FRF_LSB, (uint8_t)(frf & 0xFFu));
}

static bool lora_verify_frf(uint32_t frf)
{
  uint8_t msb = 0u;
  uint8_t mid = 0u;
  uint8_t lsb = 0u;

  if (!lora_read_reg(LORA_REG_FRF_MSB, &msb))
  {
    return false;
  }

  if (!lora_read_reg(LORA_REG_FRF_MID, &mid))
  {
    return false;
  }

  if (!lora_read_reg(LORA_REG_FRF_LSB, &lsb))
  {
    return false;
  }

  return (((uint32_t)msb << 16) | ((uint32_t)mid << 8) | (uint32_t)lsb) == frf;
}

static bool lora_configure(void)
{
  const uint32_t frf = lora_hz_to_frf(LORA_FREQ_HZ);

  if (!lora_write_verify(LORA_REG_OPMODE, LORA_OPMODE_SLEEP_LORA))
  {
    return false;
  }

  HAL_Delay(LORA_SLEEP_SETTLE_MS);

  if (!lora_write_frf(frf))
  {
    return false;
  }

  if (!lora_write_verify(LORA_REG_MODEMCONFIG1, LORA_MODEMCONFIG1))
  {
    return false;
  }

  if (!lora_write_verify(LORA_REG_MODEMCONFIG2, LORA_MODEMCONFIG2))
  {
    return false;
  }

  if (!lora_write_verify(LORA_REG_MODEMCONFIG3, LORA_MODEMCONFIG3))
  {
    return false;
  }

  if (!lora_write_verify(LORA_REG_SYNCWORD, LORA_SYNC_WORD))
  {
    return false;
  }

  if (!lora_write_verify(LORA_REG_PREAMBLE_MSB, 0x00u))
  {
    return false;
  }

  if (!lora_write_verify(LORA_REG_PREAMBLE_LSB, (uint8_t)LORA_PREAMBLE_LEN))
  {
    return false;
  }

  if (!lora_write_verify(LORA_REG_PACONFIG, LORA_PACONFIG))
  {
    return false;
  }

  if (!lora_write_verify(LORA_REG_OCP, LORA_OCP))
  {
    return false;
  }

  if (!lora_write_verify(LORA_REG_OPMODE, LORA_OPMODE_STANDBY_LORA))
  {
    return false;
  }

  if (!lora_verify_frf(frf))
  {
    return false;
  }

  return true;
}

bool lora_init(void)
{
  s_version = 0u;

  lora_hw_reset();

  if (!lora_check_version())
  {
    lora_set_ok(false);
    return false;
  }

  if (!lora_configure())
  {
    lora_set_ok(false);
    return false;
  }

  lora_set_ok(true);
  return true;
}

bool lora_is_ok(void)
{
  return s_ok;
}

uint8_t lora_get_version(void)
{
  return s_version;
}
