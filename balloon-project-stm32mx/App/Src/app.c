/**
 * @file app.c
 * @brief Application entry point implementation.
 */

#include "app.h"

#include "baro.h"
#include "error_flags.h"
#include "imu.h"
#include "temp.h"

bool app_init(void)
{
  error_flags_init();
  (void)imu_init();  /* fail-soft: false does not abort app_init */
  (void)baro_init(); /* fail-soft: false does not abort app_init */
  (void)temp_init(); /* fail-soft: false does not abort app_init */
  return true;
}

void app_run(void)
{
  /* Subsystem faults must not stop the superloop; F8+ mission tick runs regardless. */
  (void)error_flags_get();
}
