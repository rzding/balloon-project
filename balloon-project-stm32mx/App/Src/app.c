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
  /* Mission tick and driver polling arrive in later phases (F8+). */
}
