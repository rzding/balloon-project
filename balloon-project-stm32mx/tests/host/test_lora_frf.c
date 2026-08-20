/**
 * @file test_lora_frf.c
 * @brief Host unit tests for SX1276 Hz → Frf conversion (F7.2).
 */

#include <stdio.h>
#include <stdint.h>

#include "lora.h"

static int failures = 0;

static void assert_u32(uint32_t got, uint32_t expected, const char *msg)
{
  if (got != expected)
  {
    printf("FAIL %s: got 0x%06lX expected 0x%06lX\n", msg,
           (unsigned long)got, (unsigned long)expected);
    failures++;
  }
}

int main(void)
{
  /* 915.0 MHz US ISM locked default (SX1276 §5.5.2 integer Frf). */
  assert_u32(lora_hz_to_frf(915000000u), 0xE4C000u, "915 MHz Frf");

  /* Second vector: 868.0 MHz (EU ISM reference). */
  assert_u32(lora_hz_to_frf(868000000u), 0xD90000u, "868 MHz Frf");

  if (failures == 0)
  {
    printf("All LoRa Frf tests passed.\n");
    return 0;
  }

  printf("%d test(s) failed.\n", failures);
  return 1;
}
