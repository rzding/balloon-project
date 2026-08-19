/**
 * @file test_max31865_cvd.c
 * @brief Host unit tests for MAX31865 RTD unpack, resistance, CVD, and sample API (F4.2–F4.3).
 */

#include <math.h>
#include <stdio.h>

#include "temp.h"

static int failures = 0;

static void assert_u16(uint16_t got, uint16_t expected, const char *msg)
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

static void assert_near(float got, float expected, float tol, const char *msg)
{
  if (fabsf(got - expected) > tol)
  {
    printf("FAIL %s: got %f expected %f (tol %f)\n", msg, got, expected, tol);
    failures++;
  }
}

static void assert_near_i16(int16_t got, int16_t expected, int16_t tol, const char *msg)
{
  int32_t diff = (int32_t)got - (int32_t)expected;
  if (diff < 0)
  {
    diff = -diff;
  }

  if (diff > (int32_t)tol)
  {
    printf("FAIL %s: got %d expected %d (tol %d)\n", msg, (int)got, (int)expected, (int)tol);
    failures++;
  }
}

int main(void)
{
  temp_raw_t raw;
  temp_sample_t sample;

  /* RTD unpack: MSB 0x12, LSB 0x34 → adc 0x091A, no fault. */
  assert_bool(temp_rtd_unpack(0x12u, 0x34u, &raw), true, "unpack ok");
  assert_u16(raw.adc, 0x091Au, "unpack adc");
  assert_bool(raw.fault, false, "unpack no fault");

  /* LSB bit 0 set → fault true. */
  assert_bool(temp_rtd_unpack(0x12u, 0x35u, &raw), true, "fault unpack ok");
  assert_bool(raw.fault, true, "unpack fault bit");

  /* ADC → ohms: ~0 °C at RREF 4300 (adc 7616 ≈ 1000 Ω). */
  assert_near(temp_rtd_adc_to_ohm(7616u), 1000.0f, 1.0f, "adc to ohm 0C");

  /* CVD inverse: IEC PT1000 table values (10× PT100). */
  {
    float temp_c = 0.0f;

    assert_bool(temp_pt1000_ohm_to_c(1000.0f, &temp_c), true, "1000 ohm ok");
    assert_near(temp_c, 0.0f, 0.05f, "1000 ohm -> 0C");

    assert_bool(temp_pt1000_ohm_to_c(1385.06f, &temp_c), true, "1385.06 ohm ok");
    assert_near(temp_c, 100.0f, 0.05f, "1385.06 ohm -> 100C");

    assert_bool(temp_pt1000_ohm_to_c(803.1f, &temp_c), true, "803.1 ohm ok");
    assert_near(temp_c, -50.0f, 0.05f, "803.1 ohm -> -50C");
  }

  /* Forward/inverse round-trip at cold HAB-relevant temperature. */
  {
    float rt = temp_pt1000_c_to_ohm(-60.0f);
    float temp_c = 0.0f;

    assert_bool(temp_pt1000_ohm_to_c(rt, &temp_c), true, "round-trip -60C ok");
    assert_near(temp_c, -60.0f, 0.05f, "round-trip -60C");
  }

  /* F4.3: temp_sample_from_raw composition. */
  {
    const uint16_t adc_0c = (uint16_t)lroundf((TEMP_PT1000_R0_OHM / TEMP_RREF_OHM) *
                                              TEMP_RTD_ADC_FULL_SCALE);
    const float rt_100c = temp_pt1000_c_to_ohm(100.0f);
    const uint16_t adc_100c =
        (uint16_t)lroundf((rt_100c / TEMP_RREF_OHM) * TEMP_RTD_ADC_FULL_SCALE);

    raw.adc = adc_0c;
    raw.fault = false;
    raw.fault_status = 0u;
    assert_bool(temp_sample_from_raw(&raw, &sample), true, "sample 0C ok");
    assert_near_i16(sample.temp_centi_c, 0, 5, "sample 0C centi");

    raw.adc = adc_100c;
    assert_bool(temp_sample_from_raw(&raw, &sample), true, "sample 100C ok");
    assert_near_i16(sample.temp_centi_c, 10000, 5, "sample 100C centi");

    raw.fault = true;
    assert_bool(temp_sample_from_raw(&raw, &sample), false, "sample fault");

    raw.fault = false;
    raw.adc = 0u;
    assert_bool(temp_sample_from_raw(&raw, &sample), false, "sample zero adc");

    assert_bool(temp_sample_from_raw(NULL, &sample), false, "sample null raw");
    assert_bool(temp_sample_from_raw(&raw, NULL), false, "sample null out");
  }

  if (failures == 0)
  {
    printf("All MAX31865 CVD tests passed.\n");
    return 0;
  }

  printf("%d test(s) failed.\n", failures);
  return 1;
}
