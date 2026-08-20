/**
 * @file lora.c
 * @brief RFM95W-915S2 (SX1276) LoRa driver — reset and probe (F7.1).
 */

#include "lora.h"

#include "error_flags.h"
#include "main.h"
#include "spi_bus.h"

/** SX1276 RegVersion (datasheet address 0x42). */
#define LORA_REG_VERSION            0x42u

/** Expected RegVersion for SX1276 silicon. */
#define LORA_VERSION_EXPECT         0x12u

/** Active-low RESET pulse width (ms); >100 us per datasheet. */
#define LORA_RESET_PULSE_MS         1u

/** Post-reset POR wait before SPI (ms); datasheet >= 5 ms. */
#define LORA_RESET_POR_MS           10u

/** Finite HAL SPI timeout for register access. */
#define LORA_SPI_TIMEOUT_MS         100u

static bool s_ok;
static uint8_t s_version;

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

  if (!spi_bus_read_reg8(LoRa_CS_GPIO_Port, LoRa_CS_Pin, LORA_REG_VERSION, &id,
                         LORA_SPI_TIMEOUT_MS))
  {
    return false;
  }

  s_version = id;
  return id == LORA_VERSION_EXPECT;
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
