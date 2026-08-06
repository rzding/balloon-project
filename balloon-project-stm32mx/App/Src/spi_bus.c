/**
 * @file spi_bus.c
 * @brief Shared SPI1 bus owner — clock policy implementation (F1.1).
 */

#include "spi_bus.h"

static SPI_HandleTypeDef *spi_bus_hspi;

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
