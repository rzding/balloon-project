/**
 * @file test_ms5611_adc.c
 * @brief Host unit tests for MS5611 24-bit ADC unpack (F3.2).
 */

#include <stdio.h>

#include "baro.h"

static int failures = 0;

static void assert_u32(uint32_t got, uint32_t expected, const char *msg)
{
  if (got != expected)
  {
    printf("FAIL %s: got %lu expected %lu\n", msg, (unsigned long)got,
           (unsigned long)expected);
    failures++;
  }
}

int main(void)
{
  assert_u32(baro_be_bytes_to_u24(0x12u, 0x34u, 0x56u), 0x123456u, "BE 0x123456");
  assert_u32(baro_be_bytes_to_u24(0x00u, 0x00u, 0x00u), 0u, "BE zero");
  assert_u32(baro_be_bytes_to_u24(0xFFu, 0xFFu, 0xFFu), 0xFFFFFFu, "BE max 24-bit");

  if (failures == 0)
  {
    printf("All MS5611 ADC unpack tests passed.\n");
    return 0;
  }

  printf("%d test(s) failed.\n", failures);
  return 1;
}
