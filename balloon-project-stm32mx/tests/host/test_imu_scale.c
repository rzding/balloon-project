/**
 * @file test_imu_scale.c
 * @brief Host unit tests for IMU scale helpers and endian unpack (F2.3).
 */

#include <math.h>
#include <stdio.h>

#include "imu.h"

static int failures = 0;

static void assert_near(float got, float expected, float tol, const char *msg)
{
  if (fabsf(got - expected) > tol)
  {
    printf("FAIL %s: got %f expected %f (tol %f)\n", msg, got, expected, tol);
    failures++;
  }
}

static void assert_i16(int16_t got, int16_t expected, const char *msg)
{
  if (got != expected)
  {
    printf("FAIL %s: got %d expected %d\n", msg, (int)got, (int)expected);
    failures++;
  }
}

int main(void)
{
  assert_near(imu_accel_raw_to_mps2(2048), 9.80665f, 0.001f, "1g accel");
  assert_near(imu_accel_raw_to_mps2(0), 0.0f, 0.001f, "0g accel");
  assert_near(imu_gyro_raw_to_dps(1640), 100.0f, 0.01f, "100 dps gyro");
  assert_near(imu_gyro_raw_to_dps(0), 0.0f, 0.01f, "0 dps gyro");

  assert_i16(imu_be_bytes_to_i16(0xFFu, 0x00u), -256, "BE -256");
  assert_i16(imu_be_bytes_to_i16(0x00u, 0x01u), 1, "BE +1");
  assert_i16(imu_be_bytes_to_i16(0x08u, 0x00u), 2048, "BE 2048");

  if (failures == 0)
  {
    printf("All IMU scale tests passed.\n");
    return 0;
  }

  printf("%d test(s) failed.\n", failures);
  return 1;
}
