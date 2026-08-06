/**
 * @file spi_bus.c
 * @brief Shared SPI1 bus owner — clock policy (F1.1) and transfer API (F1.2).
 */

#include "spi_bus.h"

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

bool spi_bus_transfer(GPIO_TypeDef *cs_port, uint16_t cs_pin,
                      const uint8_t *tx, uint8_t *rx,
                      uint16_t len, uint32_t timeout_ms)
{
  HAL_StatusTypeDef status;

  if (spi_bus_hspi == NULL || cs_port == NULL || len == 0U)
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
  spi_bus_cs_set(cs_port, cs_pin, GPIO_PIN_RESET);

  if (tx != NULL && rx != NULL)
  {
    status = HAL_SPI_TransmitReceive(spi_bus_hspi, tx, rx, len, timeout_ms);
  }
  else if (tx != NULL)
  {
    status = HAL_SPI_Transmit(spi_bus_hspi, tx, len, timeout_ms);
  }
  else
  {
    status = HAL_SPI_Receive(spi_bus_hspi, rx, len, timeout_ms);
  }

  spi_bus_cs_set(cs_port, cs_pin, GPIO_PIN_SET);

  if (status != HAL_OK)
  {
    (void)HAL_SPI_Abort(spi_bus_hspi);
    spi_bus_busy = false;
    return false;
  }

  spi_bus_busy = false;
  return true;
}
