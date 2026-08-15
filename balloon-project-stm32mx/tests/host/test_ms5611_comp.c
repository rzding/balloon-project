/**
 * @file test_ms5611_comp.c
 * @brief Host unit tests for MS5611 compensation and ISA altitude (F3.3).
 */

#include <math.h>
#include <stdio.h>

#include "baro.h"

static int failures = 0;

static void assert_i32(int32_t got, int32_t expected, const char *msg)
{
  if (got != expected)
  {
    printf("FAIL %s: got %ld expected %ld\n", msg, (long)got, (long)expected);
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

static void assert_near(float got, float expected, float tol, const char *msg)
{
  if (fabsf(got - expected) > tol)
  {
    printf("FAIL %s: got %f expected %f (tol %f)\n", msg, got, expected, tol);
    failures++;
  }
}

static int32_t first_order_temp_centi_c(const uint16_t prom[BARO_PROM_WORD_COUNT], uint32_t d2)
{
  int64_t dt = (int64_t)d2 - ((int64_t)prom[BARO_PROM_C5_INDEX] * 256);

  return (int32_t)(2000 + ((dt * (int64_t)prom[BARO_PROM_C6_INDEX]) >> 23));
}

int main(void)
{
  /* Datasheet B3 worked example (TEMP >= 20 C, second-order off). */
  const uint16_t prom_ds[BARO_PROM_WORD_COUNT] = {
      0x0000u, 40127u, 36924u, 23317u, 23282u, 33464u, 28312u, 0x0000u};
  const uint32_t d1_ds = 9085466u;
  const uint32_t d2_ds = 8569150u;
  baro_comp_t comp;

  assert_bool(baro_compensate(prom_ds, d1_ds, d2_ds, &comp), true, "datasheet compensate ok");
  assert_i32(comp.temp_centi_c, 2007, "datasheet TEMP");
  assert_i32(comp.pressure_pa, 100009, "datasheet P");

  /* Second-order: cold D2 so first-order TEMP < 20 C. */
  {
    const uint32_t d2_cold = 8000000u;
    int32_t temp_first;

    assert_bool(baro_compensate(prom_ds, d1_ds, d2_cold, &comp), true, "cold compensate ok");
    temp_first = first_order_temp_centi_c(prom_ds, d2_cold);
    if (temp_first >= 2000)
    {
      printf("FAIL cold vector setup: first-order TEMP %ld not below 2000\n", (long)temp_first);
      failures++;
    }
    if (comp.temp_centi_c >= temp_first)
    {
      printf("FAIL second-order: compensated TEMP %ld not less than first-order %ld\n",
             (long)comp.temp_centi_c, (long)temp_first);
      failures++;
    }
  }

  /* ICAO ISA altitude helper. */
  assert_near(baro_pressure_pa_to_alt_m(101325.0f), 0.0f, 1.0f, "ISA sea level");
  assert_near(baro_pressure_pa_to_alt_m(89875.0f), 1000.0f, 5.0f, "ISA 1000 m");
  assert_near(baro_pressure_pa_to_alt_m(22632.0f), 11000.0f, 5.0f, "ISA tropopause");

  /* Reject invalid inputs. */
  assert_bool(baro_compensate(prom_ds, 0u, d2_ds, &comp), false, "reject d1 zero");
  assert_bool(baro_compensate(prom_ds, d1_ds, d2_ds, NULL), false, "reject null out");

  if (failures == 0)
  {
    printf("All MS5611 compensation tests passed.\n");
    return 0;
  }

  printf("%d test(s) failed.\n", failures);
  return 1;
}
