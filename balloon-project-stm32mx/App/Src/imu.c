/**
 * @file imu.c
 * @brief ICM-42688-P IMU driver — identity (F2.1), config (F2.2), sample read (F2.3), health (F2.4).
 */

#include "imu.h"

#include "error_flags.h"
#include "main.h"
#include "spi_bus.h"

/** WHO_AM_I register address (Bank 0). */
#define IMU_REG_WHO_AM_I            0x75u
#define IMU_REG_DEVICE_CONFIG       0x11u
#define IMU_REG_INTF_CONFIG0        0x4Cu
#define IMU_REG_INTF_CONFIG1        0x4Du
#define IMU_REG_PWR_MGMT0           0x4Eu
#define IMU_REG_GYRO_CONFIG0        0x4Fu
#define IMU_REG_ACCEL_CONFIG0       0x50u
#define IMU_REG_ACCEL_DATA_X1       0x1Fu

#define IMU_SPI_REG_READ_BIT        0x80u
#define IMU_BURST_DATA_BYTES        12u
#define IMU_BURST_TRANSFER_BYTES    (1u + IMU_BURST_DATA_BYTES)

/** Expected WHO_AM_I value for ICM-42688-P (not ICM-42688-V / 0xDB). */
#define IMU_WHO_AM_I_EXPECT         0x47u

#define IMU_SOFT_RESET              0x01u
#define IMU_INTF_CONFIG0_SPI        0x03u   /* UI_SIFS_CFG: disable I2C */
#define IMU_INTF_CONFIG1_AFSR_MASK  0xC0u
#define IMU_INTF_CONFIG1_AFSR_DIS   0x40u
#define IMU_PWR_LN_BOTH              0x0Fu   /* gyro LN | accel LN */
#define IMU_CFG0_FS_ODR_100HZ       0x08u   /* ±16 g / ±2000 dps @ 100 Hz */

/** Finite HAL SPI timeout for register access. */
#define IMU_SPI_TIMEOUT_MS          100u

static bool s_ok;

static bool imu_read_reg(uint8_t reg, uint8_t *value)
{
  return spi_bus_read_reg8(IMU_CS_GPIO_Port, IMU_CS_Pin, reg, value, IMU_SPI_TIMEOUT_MS);
}

static bool imu_write_reg(uint8_t reg, uint8_t value)
{
  return spi_bus_write_reg8(IMU_CS_GPIO_Port, IMU_CS_Pin, reg, value, IMU_SPI_TIMEOUT_MS);
}

static bool imu_write_verify(uint8_t reg, uint8_t value)
{
  uint8_t readback = 0u;

  if (!imu_write_reg(reg, value))
  {
    return false;
  }

  if (!imu_read_reg(reg, &readback))
  {
    return false;
  }

  return readback == value;
}

static void imu_set_ok(bool ok)
{
  s_ok = ok;
  error_flags_set_imu_ok(ok);
}

static bool imu_check_who_am_i(void)
{
  uint8_t id = 0u;

  if (!imu_read_reg(IMU_REG_WHO_AM_I, &id))
  {
    return false;
  }

  return id == IMU_WHO_AM_I_EXPECT;
}

static bool imu_soft_reset(void)
{
  if (!imu_write_reg(IMU_REG_DEVICE_CONFIG, IMU_SOFT_RESET))
  {
    return false;
  }

  HAL_Delay(2);
  return true;
}

static bool imu_configure(void)
{
  uint8_t intf1 = 0u;
  uint8_t intf1_new = 0u;

  if (!imu_write_verify(IMU_REG_INTF_CONFIG0, IMU_INTF_CONFIG0_SPI))
  {
    return false;
  }

  if (!imu_read_reg(IMU_REG_INTF_CONFIG1, &intf1))
  {
    return false;
  }

  intf1_new = (intf1 & ~IMU_INTF_CONFIG1_AFSR_MASK) | IMU_INTF_CONFIG1_AFSR_DIS;

  if (!imu_write_verify(IMU_REG_INTF_CONFIG1, intf1_new))
  {
    return false;
  }

  if (!imu_write_verify(IMU_REG_GYRO_CONFIG0, IMU_CFG0_FS_ODR_100HZ))
  {
    return false;
  }

  if (!imu_write_verify(IMU_REG_ACCEL_CONFIG0, IMU_CFG0_FS_ODR_100HZ))
  {
    return false;
  }

  if (!imu_write_verify(IMU_REG_PWR_MGMT0, IMU_PWR_LN_BOTH))
  {
    return false;
  }

  HAL_Delay(1);
  return true;
}

bool imu_init(void)
{
  if (!imu_check_who_am_i())
  {
    imu_set_ok(false);
    return false;
  }

  if (!imu_soft_reset())
  {
    imu_set_ok(false);
    return false;
  }

  if (!imu_check_who_am_i())
  {
    imu_set_ok(false);
    return false;
  }

  if (!imu_configure())
  {
    imu_set_ok(false);
    return false;
  }

  imu_set_ok(true);
  return true;
}

bool imu_read(imu_sample_t *out)
{
  uint8_t tx[IMU_BURST_TRANSFER_BYTES];
  uint8_t rx[IMU_BURST_TRANSFER_BYTES];
  uint8_t i;

  if (out == NULL)
  {
    return false;
  }

  for (i = 0u; i < IMU_BURST_TRANSFER_BYTES; i++)
  {
    tx[i] = 0u;
  }

  tx[0] = (uint8_t)(IMU_REG_ACCEL_DATA_X1 | IMU_SPI_REG_READ_BIT);

  if (!spi_bus_transfer(IMU_CS_GPIO_Port, IMU_CS_Pin, tx, rx,
                        IMU_BURST_TRANSFER_BYTES, IMU_SPI_TIMEOUT_MS))
  {
    imu_set_ok(false);
    return false;
  }

  out->ax = imu_be_bytes_to_i16(rx[1], rx[2]);
  out->ay = imu_be_bytes_to_i16(rx[3], rx[4]);
  out->az = imu_be_bytes_to_i16(rx[5], rx[6]);
  out->gx = imu_be_bytes_to_i16(rx[7], rx[8]);
  out->gy = imu_be_bytes_to_i16(rx[9], rx[10]);
  out->gz = imu_be_bytes_to_i16(rx[11], rx[12]);

  imu_set_ok(true);
  return true;
}

bool imu_is_ok(void)
{
  return s_ok;
}
