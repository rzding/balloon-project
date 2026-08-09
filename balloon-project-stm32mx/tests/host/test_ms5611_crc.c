/**
 * @file test_ms5611_crc.c
 * @brief Host unit tests for MS5611 PROM CRC4 (F3.1).
 */

#include <stdio.h>

#include "baro.h"

static int failures = 0;

static void assert_u8(uint8_t got, uint8_t expected, const char *msg)
{
  if (got != expected)
  {
    printf("FAIL %s: got %u expected %u\n", msg, (unsigned)got, (unsigned)expected);
    failures++;
  }
}

static void assert_bool(bool got, bool expected, const char *msg)
{
  if (got != expected)
  {
    printf("FAIL %s: got %d expected %d\n", msg, (int)got, (int)expected);
    failures++;
  }
}

int main(void)
{
  /* AN520 CRC CODE NOTES: coeffs 0x3132..0x4344, word 7 = 0x4500 → CRC 0xB. */
  const uint16_t prom_crc[BARO_PROM_WORD_COUNT] = {
      0x3132u, 0x3334u, 0x3536u, 0x3738u, 0x3940u, 0x4142u, 0x4344u, 0x4500u};

  /* Same coeffs; word 7 low nibble holds stored CRC (0xB). */
  const uint16_t prom_ok[BARO_PROM_WORD_COUNT] = {
      0x3132u, 0x3334u, 0x3536u, 0x3738u, 0x3940u, 0x4142u, 0x4344u, 0x450Bu};

  uint16_t prom_bad[BARO_PROM_WORD_COUNT];
  uint8_t i;

  assert_u8(baro_crc4(prom_crc), 0x0Bu, "CRC4 AN520 vector");
  assert_bool(baro_prom_crc_ok(prom_ok), true, "PROM CRC ok AN520 vector");

  for (i = 0u; i < BARO_PROM_WORD_COUNT; i++)
  {
    prom_bad[i] = prom_ok[i];
  }
  prom_bad[BARO_PROM_C1_INDEX] = 0xFFFFu;
  assert_bool(baro_prom_crc_ok(prom_bad), false, "PROM CRC fail mutated C1");

  if (failures == 0)
  {
    printf("All MS5611 CRC tests passed.\n");
    return 0;
  }

  printf("%d test(s) failed.\n", failures);
  return 1;
}
