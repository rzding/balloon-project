/**
 * @file test_mission_sm.c
 * @brief Host unit tests for mission state transitions (F8.1) + edge profiles (F8.4).
 *
 * Covers happy-path PAD→…→BEACON, BURST latch, freefall, skip-FLOAT burst, and
 * negative gates (no arm, no false ASCENT/FLOAT/LANDED, short freefall).
 */

#include <stdio.h>
#include <string.h>

#include "mission.h"

static int failures = 0;

static void assert_true(bool got, const char *msg)
{
  if (!got)
  {
    printf("FAIL %s\n", msg);
    failures++;
  }
}

static void assert_state(mission_state_t got, mission_state_t expected, const char *msg)
{
  if (got != expected)
  {
    printf("FAIL %s: got %d expected %d\n", msg, (int)got, (int)expected);
    failures++;
  }
}

static void feed(uint32_t t_ms, float alt_m, bool alt_ok, bool source_ok,
                 float ax, float ay, float az, bool imu_ok)
{
  mission_input_t in;

  memset(&in, 0, sizeof(in));
  in.time_ms = t_ms;
  in.alt_m = alt_m;
  in.alt_valid = alt_ok;
  in.altitude_source_ok = source_ok;
  in.accel_g[0] = ax;
  in.accel_g[1] = ay;
  in.accel_g[2] = az;
  in.imu_valid = imu_ok;
  mission_update(&in);
}

/** 1 g on Z (not freefall). */
static void feed_alt(uint32_t t_ms, float alt_m, bool source_ok)
{
  feed(t_ms, alt_m, true, source_ok, 0.0f, 0.0f, 1.0f, true);
}

/** Hold constant altitude so EMA rate decays toward zero. */
static void hold_alt(uint32_t *t_ms, float alt_m, uint32_t duration_ms)
{
  const uint32_t end = *t_ms + duration_ms;
  for (; *t_ms <= end; *t_ms += 1000u)
  {
    feed_alt(*t_ms, alt_m, true);
  }
}

/** Climb at +2 m/s until armed→ascent (caller already in ARMED). */
static void climb_to_ascent(uint32_t *t_ms, float *alt_m)
{
  const uint32_t end = *t_ms + MISSION_ASCENT_SUSTAIN_MS + 3000u;
  for (; *t_ms <= end; *t_ms += 1000u)
  {
    *alt_m += 2.0f;
    feed_alt(*t_ms, *alt_m, true);
  }
}

static void test_full_profile(void)
{
  uint32_t t = 0u;
  float alt = 200.0f;

  mission_init();
  assert_state(mission_get_state(), MISSION_STATE_PAD, "start PAD");

  /* PAD→ARMED */
  for (; t <= MISSION_ARM_HOLD_MS; t += 1000u)
  {
    feed_alt(t, alt, true);
  }
  assert_state(mission_get_state(), MISSION_STATE_ARMED, "PAD→ARMED");

  /* ARMED→ASCENT */
  climb_to_ascent(&t, &alt);
  assert_state(mission_get_state(), MISSION_STATE_ASCENT, "ARMED→ASCENT");

  /* Gradual climb to float altitude (avoids rate spike), then hold for EMA + sustain. */
  while (alt < MISSION_FLOAT_ALT_M + 100.0f)
  {
    alt += 2.0f;
    feed_alt(t, alt, true);
    t += 1000u;
  }
  hold_alt(&t, alt, 20000u); /* EMA settle */
  hold_alt(&t, alt, MISSION_FLOAT_SUSTAIN_MS + 2000u);
  assert_state(mission_get_state(), MISSION_STATE_FLOAT, "ASCENT→FLOAT");

  /* FLOAT→BURST */
  alt += MISSION_BURST_RATE_MPS * 1.1f;
  feed_alt(t, alt, true);
  assert_state(mission_get_state(), MISSION_STATE_BURST, "FLOAT→BURST");
  assert_true(mission_burst_latched(), "burst latched on entry");

  /* BURST→DESCENT */
  t += 1000u;
  alt -= 20.0f;
  feed_alt(t, alt, true);
  assert_state(mission_get_state(), MISSION_STATE_DESCENT, "BURST→DESCENT");

  /* Settle EMA, then advance until DESCENT→LANDED (stop before next tick). */
  t += 1000u;
  hold_alt(&t, alt, 25000u);
  assert_state(mission_get_state(), MISSION_STATE_DESCENT, "still DESCENT after settle");
  {
    const uint32_t t0 = t;
    while (mission_get_state() == MISSION_STATE_DESCENT &&
           (t - t0) < (MISSION_LANDED_STABLE_MS + 30000u))
    {
      feed_alt(t, alt, true);
      t += 1000u;
    }
  }
  assert_state(mission_get_state(), MISSION_STATE_LANDED, "DESCENT→LANDED");

  feed_alt(t, alt, true);
  assert_state(mission_get_state(), MISSION_STATE_BEACON, "LANDED→BEACON");
}

static void test_burst_latch_no_return(void)
{
  uint32_t t = 0u;
  float alt = 200.0f;
  uint32_t end;

  mission_init();

  for (; t <= MISSION_ARM_HOLD_MS; t += 1000u)
  {
    feed_alt(t, alt, true);
  }
  climb_to_ascent(&t, &alt);

  while (alt < MISSION_FLOAT_ALT_M + 100.0f)
  {
    alt += 2.0f;
    feed_alt(t, alt, true);
    t += 1000u;
  }
  hold_alt(&t, alt, 20000u);
  hold_alt(&t, alt, MISSION_FLOAT_SUSTAIN_MS + 2000u);
  assert_state(mission_get_state(), MISSION_STATE_FLOAT, "latch setup FLOAT");

  alt -= 60.0f;
  feed_alt(t, alt, true);
  assert_state(mission_get_state(), MISSION_STATE_BURST, "latch BURST");
  t += 1000u;
  feed_alt(t, alt - 10.0f, true);
  alt -= 10.0f;
  assert_state(mission_get_state(), MISSION_STATE_DESCENT, "latch DESCENT");

  end = t + 60000u;
  for (t += 1000u; t < end; t += 1000u)
  {
    alt += 0.2f; /* mild climb after thick-air slowdown */
    feed_alt(t, alt, true);
    assert_true(mission_get_state() != MISSION_STATE_ASCENT, "no return ASCENT");
    assert_true(mission_get_state() != MISSION_STATE_FLOAT, "no return FLOAT");
    assert_true(mission_burst_latched(), "latch remains true");
  }
}

static void test_burst_via_freefall(void)
{
  uint32_t t = 0u;
  float alt = 1000.0f;

  mission_init();
  for (; t <= MISSION_ARM_HOLD_MS; t += 1000u)
  {
    feed_alt(t, alt, true);
  }
  climb_to_ascent(&t, &alt);
  assert_state(mission_get_state(), MISSION_STATE_ASCENT, "freefall setup ASCENT");

  {
    const uint32_t end = t + MISSION_FREEFALL_SUSTAIN_MS + 200u;
    for (; t <= end; t += 100u)
    {
      feed(t, alt, true, true, 0.05f, 0.05f, 0.05f, true);
      if (mission_get_state() == MISSION_STATE_BURST)
      {
        break;
      }
    }
  }
  assert_state(mission_get_state(), MISSION_STATE_BURST, "ASCENT→BURST freefall");
  assert_true(mission_burst_latched(), "freefall latched");
}

static void test_ascent_skips_float_to_burst(void)
{
  uint32_t t = 0u;
  float alt = 500.0f;

  mission_init();
  for (; t <= MISSION_ARM_HOLD_MS; t += 1000u)
  {
    feed_alt(t, alt, true);
  }
  climb_to_ascent(&t, &alt);
  assert_state(mission_get_state(), MISSION_STATE_ASCENT, "skip-float ASCENT");

  alt -= 55.0f;
  feed_alt(t, alt, true);
  assert_state(mission_get_state(), MISSION_STATE_BURST, "ASCENT→BURST skip FLOAT");
}

/** F8.4: no healthy altitude source → stay PAD; interrupted OK resets arm timer. */
static void test_pad_no_source(void)
{
  uint32_t t = 0u;
  const float alt = 200.0f;

  mission_init();
  for (; t <= MISSION_ARM_HOLD_MS + 5000u; t += 1000u)
  {
    feed_alt(t, alt, false);
  }
  assert_state(mission_get_state(), MISSION_STATE_PAD, "no source stays PAD");

  mission_init();
  t = 0u;
  for (; t < (MISSION_ARM_HOLD_MS / 2u); t += 1000u)
  {
    feed_alt(t, alt, true);
  }
  assert_state(mission_get_state(), MISSION_STATE_PAD, "partial arm still PAD");
  feed_alt(t, alt, false); /* interrupt → reset arm timer */
  t += 1000u;
  {
    const uint32_t end = t + MISSION_ARM_HOLD_MS - 1000u;
    for (; t <= end; t += 1000u)
    {
      feed_alt(t, alt, true);
    }
  }
  assert_state(mission_get_state(), MISSION_STATE_PAD, "interrupted arm not ARMED yet");
  {
    const uint32_t end = t + MISSION_ARM_HOLD_MS + 1000u;
    for (; t <= end; t += 1000u)
    {
      feed_alt(t, alt, true);
    }
  }
  assert_state(mission_get_state(), MISSION_STATE_ARMED, "full hold after reset → ARMED");
}

/** F8.4: flat altitude while ARMED → no false ASCENT. */
static void test_armed_no_false_ascent(void)
{
  uint32_t t = 0u;
  const float alt = 200.0f;

  mission_init();
  for (; t <= MISSION_ARM_HOLD_MS; t += 1000u)
  {
    feed_alt(t, alt, true);
  }
  assert_state(mission_get_state(), MISSION_STATE_ARMED, "setup ARMED");

  hold_alt(&t, alt, MISSION_ASCENT_SUSTAIN_MS + 5000u);
  assert_state(mission_get_state(), MISSION_STATE_ARMED, "flat stays ARMED");
}

/** F8.4: above float alt but still climbing → stay ASCENT (rate gate). */
static void test_float_requires_low_rate(void)
{
  uint32_t t = 0u;
  float alt = 200.0f;

  mission_init();
  for (; t <= MISSION_ARM_HOLD_MS; t += 1000u)
  {
    feed_alt(t, alt, true);
  }
  climb_to_ascent(&t, &alt);
  assert_state(mission_get_state(), MISSION_STATE_ASCENT, "rate-gate setup ASCENT");

  /* Climb past float altitude at ~+2 m/s, then keep climbing through sustain window. */
  while (alt < MISSION_FLOAT_ALT_M + 100.0f)
  {
    alt += 2.0f;
    feed_alt(t, alt, true);
    t += 1000u;
  }
  {
    const uint32_t end = t + MISSION_FLOAT_SUSTAIN_MS + 2000u;
    for (; t <= end; t += 1000u)
    {
      alt += 2.0f;
      feed_alt(t, alt, true);
    }
  }
  assert_true(alt >= MISSION_FLOAT_ALT_M, "above float alt");
  assert_state(mission_get_state(), MISSION_STATE_ASCENT, "climbing stays ASCENT");
}

/** F8.4: freefall shorter than sustain → no BURST. */
static void test_freefall_too_short(void)
{
  uint32_t t = 0u;
  float alt = 1000.0f;

  mission_init();
  for (; t <= MISSION_ARM_HOLD_MS; t += 1000u)
  {
    feed_alt(t, alt, true);
  }
  climb_to_ascent(&t, &alt);
  assert_state(mission_get_state(), MISSION_STATE_ASCENT, "short-ff setup ASCENT");

  {
    const uint32_t end = t + (MISSION_FREEFALL_SUSTAIN_MS / 2u);
    for (; t <= end; t += 100u)
    {
      feed(t, alt, true, true, 0.05f, 0.05f, 0.05f, true);
    }
  }
  assert_state(mission_get_state(), MISSION_STATE_ASCENT, "short freefall still ASCENT");

  feed_alt(t, alt, true); /* 1 g restores */
  assert_state(mission_get_state(), MISSION_STATE_ASCENT, "after 1g still ASCENT");
  assert_true(!mission_burst_latched(), "short freefall not latched");
}

/** F8.4: DESCENT while still falling → no early LANDED. */
static void test_descent_not_landed_while_falling(void)
{
  uint32_t t = 0u;
  float alt = 500.0f;
  uint32_t end;

  mission_init();
  for (; t <= MISSION_ARM_HOLD_MS; t += 1000u)
  {
    feed_alt(t, alt, true);
  }
  climb_to_ascent(&t, &alt);
  assert_state(mission_get_state(), MISSION_STATE_ASCENT, "falling setup ASCENT");

  alt -= 55.0f;
  feed_alt(t, alt, true);
  assert_state(mission_get_state(), MISSION_STATE_BURST, "falling BURST");
  t += 1000u;
  alt -= 20.0f;
  feed_alt(t, alt, true);
  assert_state(mission_get_state(), MISSION_STATE_DESCENT, "falling DESCENT");

  end = t + MISSION_LANDED_STABLE_MS + 10000u;
  for (t += 1000u; t <= end; t += 1000u)
  {
    alt -= 2.0f; /* ~−2 m/s, above LANDED rate gate */
    feed_alt(t, alt, true);
  }
  assert_state(mission_get_state(), MISSION_STATE_DESCENT, "falling stays DESCENT");
}

int main(void)
{
  test_full_profile();
  test_burst_latch_no_return();
  test_burst_via_freefall();
  test_ascent_skips_float_to_burst();
  test_pad_no_source();
  test_armed_no_false_ascent();
  test_float_requires_low_rate();
  test_freefall_too_short();
  test_descent_not_landed_while_falling();

  if (failures == 0)
  {
    printf("PASS test_mission_sm\n");
    return 0;
  }
  printf("%d failure(s)\n", failures);
  return 1;
}
