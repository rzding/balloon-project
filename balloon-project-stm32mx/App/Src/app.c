/**
 * @file app.c
 * @brief Application entry point implementation.
 */

#include "app.h"

#include "error_flags.h"

bool app_init(void)
{
  error_flags_init();
  return true;
}

void app_run(void)
{
  /* Subsystem faults must not stop the superloop; F8+ mission tick runs regardless. */
  (void)error_flags_get();
}
