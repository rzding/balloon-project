/**
 * @file app.c
 * @brief Application entry point implementation.
 */

#include "app.h"

#include "baro.h"
#include "error_flags.h"
#include "gps.h"
#include "imu.h"
#include "stm32f4xx_hal.h"
#include "temp.h"

bool app_init(void)
{
  error_flags_init();
  (void)imu_init();  /* fail-soft: false does not abort app_init */
  (void)baro_init(); /* fail-soft: false does not abort app_init */
  (void)temp_init(); /* fail-soft: false does not abort app_init */
  (void)gps_init();  /* fail-soft: false does not abort app_init */
  return true;
}

void app_run(void)
{
#ifdef APP_BENCH_BUS_EXERCISE
  static uint32_t s_bench_last_ms;

  uint32_t now = HAL_GetTick();
  if ((now - s_bench_last_ms) >= 1000u)
  {
    imu_sample_t imu;
    baro_sample_t baro;
    temp_sample_t temp;

    s_bench_last_ms = now;
    (void)imu_read(&imu);
    (void)baro_read(&baro);
    (void)temp_read(&temp);
  }
#endif

  /* Subsystem faults must not stop the superloop; F8+ mission tick runs regardless. */
  (void)gps_poll();
  (void)error_flags_get();
}
