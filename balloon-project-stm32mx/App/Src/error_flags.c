/**
 * @file error_flags.c
 * @brief Subsystem health flag storage and accessors.
 */

#include "error_flags.h"

static uint32_t s_flags;

void error_flags_init(void)
{
  s_flags = 0u;
}

void error_flags_set(uint32_t mask)
{
  s_flags |= mask;
}

void error_flags_clear(uint32_t mask)
{
  s_flags &= ~mask;
}

bool error_flags_test(uint32_t mask)
{
  return (s_flags & mask) != 0u;
}

uint32_t error_flags_get(void)
{
  return s_flags;
}
