/**
 * @file imu.c
 * @brief ICM-42688-P IMU driver — identity probe (F2.1).
 */

#include "imu.h"

#include "error_flags.h"
#include "main.h"
#include "spi_bus.h"

/** WHO_AM_I register address (Bank 0). */
#define IMU_REG_WHO_AM_I      0x75u

/** Expected WHO_AM_I value for ICM-42688-P (not ICM-42688-V / 0xDB). */
#define IMU_WHO_AM_I_EXPECT   0x47u

/** Finite HAL SPI timeout for register access. */
#define IMU_SPI_TIMEOUT_MS    100u

static bool s_ok;

bool imu_init(void)
{
  uint8_t id = 0u;

  if (!spi_bus_read_reg8(IMU_CS_GPIO_Port, IMU_CS_Pin,
                         IMU_REG_WHO_AM_I, &id, IMU_SPI_TIMEOUT_MS)
      || id != IMU_WHO_AM_I_EXPECT)
  {
    s_ok = false;
    error_flags_set_imu_ok(false);
    return false;
  }

  s_ok = true;
  error_flags_set_imu_ok(true);
  return true;
}

bool imu_is_ok(void)
{
  return s_ok;
}
