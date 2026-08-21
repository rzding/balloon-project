/**
 * @file spi_bus.c
 * @brief Shared SPI1 bus owner — clock policy (F1.1), transfer (F1.2), register helpers (F1.3).
 */

#include "spi_bus.h"

#define SPI_BUS_REG_READ_BIT (1u << 7)

static SPI_HandleTypeDef *spi_bus_hspi;
static bool spi_bus_busy;

static void spi_bus_cs_set(GPIO_TypeDef *cs_port, uint16_t cs_pin, GPIO_PinState state)
{
  HAL_GPIO_WritePin(cs_port, cs_pin, state);
}

static bool spi_bus_apply_prescaler(uint32_t baudrate_prescaler)
{
  if (spi_bus_hspi == NULL)
  {
    return false;
  }

  __HAL_SPI_DISABLE(spi_bus_hspi);
  spi_bus_hspi->Init.BaudRatePrescaler = baudrate_prescaler;

  if (HAL_SPI_Init(spi_bus_hspi) != HAL_OK)
  {
    return false;
  }

  return true;
}

bool spi_bus_init(SPI_HandleTypeDef *hspi)
{
  if (hspi == NULL)
  {
    return false;
  }

  spi_bus_hspi = hspi;
  return spi_bus_apply_prescaler(SPI_BUS_DEFAULT_PRESCALER);
}

bool spi_bus_set_prescaler(uint32_t baudrate_prescaler)
{
  return spi_bus_apply_prescaler(baudrate_prescaler);
}

static bool spi_bus_apply_mode(uint32_t polarity, uint32_t phase)
{
  if (spi_bus_hspi == NULL)
  {
    return false;
  }

  __HAL_SPI_DISABLE(spi_bus_hspi);
  spi_bus_hspi->Init.CLKPolarity = polarity;
  spi_bus_hspi->Init.CLKPhase = phase;

  if (HAL_SPI_Init(spi_bus_hspi) != HAL_OK)
  {
    return false;
  }

  return true;
}

bool spi_bus_set_mode(uint32_t polarity, uint32_t phase)
{
  return spi_bus_apply_mode(polarity, phase);
}

bool spi_bus_transfer(GPIO_TypeDef *cs_port, uint16_t cs_pin,
                      const uint8_t *tx, uint8_t *rx,
                      uint16_t len, uint32_t timeout_ms)
{
  HAL_StatusTypeDef status;

  if (spi_bus_hspi == NULL || len == 0U) // removed the check for cs_port  
  {
    return false;
  }

  if (tx == NULL && rx == NULL)
  {
    return false;
  }

  if (spi_bus_busy)
  {
    return false;
  }

  spi_bus_busy = true;
  
  if (cs_port != NULL) {
    spi_bus_cs_set(cs_port, cs_pin, GPIO_PIN_RESET); // Pulls the CS pin low to select the SPI device
  }

  if (tx != NULL && rx != NULL)
  {
    status = HAL_SPI_TransmitReceive(spi_bus_hspi, tx, rx, len, timeout_ms); // Transmit and receive data over SPI
  }
  else if (tx != NULL)
  {
    status = HAL_SPI_Transmit(spi_bus_hspi, tx, len, timeout_ms); // Transmit data over SPI
  }
  else
  {
    status = HAL_SPI_Receive(spi_bus_hspi, rx, len, timeout_ms); // Receive data over SPI
  }

  if (cs_port != NULL) {
    spi_bus_cs_set(cs_port, cs_pin, GPIO_PIN_SET); // Pulls the CS pin high to deselect the SPI device
  }

  if (status != HAL_OK)
  {
    (void)HAL_SPI_Abort(spi_bus_hspi);
    spi_bus_busy = false;
    return false;
  }

  spi_bus_busy = false;
  return true;
}

bool spi_bus_read_reg8(GPIO_TypeDef *cs_port, uint16_t cs_pin,
                       uint8_t reg, uint8_t *value, uint32_t timeout_ms)
{
  uint8_t tx[2];
  uint8_t rx[2];

  if (value == NULL)
  {
    return false;
  }

  tx[0] = (uint8_t)(reg | SPI_BUS_REG_READ_BIT);
  tx[1] = 0U;

  if (!spi_bus_transfer(cs_port, cs_pin, tx, rx, 2U, timeout_ms))
  {
    return false;
  }

  *value = rx[1];
  return true;
}

bool spi_bus_write_reg8(GPIO_TypeDef *cs_port, uint16_t cs_pin,
                        uint8_t reg, uint8_t value, uint32_t timeout_ms)
{
  uint8_t tx[2];

  tx[0] = (uint8_t)(reg & (uint8_t)~SPI_BUS_REG_READ_BIT);
  tx[1] = value;

  return spi_bus_transfer(cs_port, cs_pin, tx, NULL, 2U, timeout_ms);
}
