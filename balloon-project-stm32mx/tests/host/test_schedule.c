/**
 * @file test_schedule.c
 * @brief Host unit tests for F8.2 LoRa/camera period table and due logic.
 */

#include <stdio.h>

#include "schedule.h"

static int failures = 0;

static void assert_true(bool got, const char *msg)
{
  if (!got)
  {
    printf("FAIL %s\n", msg);
    failures++;
  }
}

static void assert_u32(uint32_t got, uint32_t expected, const char *msg)
{
  if (got != expected)
  {
    printf("FAIL %s: got %lu expected %lu\n", msg,
           (unsigned long)got, (unsigned long)expected);
    failures++;
  }
}

static void test_period_table(void)
{
  assert_u32(schedule_lora_period_ms(MISSION_STATE_ASCENT), SCHEDULE_LORA_ASCENT_MS,
             "ASCENT LoRa");
  assert_u32(schedule_lora_period_ms(MISSION_STATE_DESCENT), SCHEDULE_LORA_DESCENT_MS,
             "DESCENT LoRa");
  assert_u32(schedule_lora_period_ms(MISSION_STATE_FLOAT), SCHEDULE_LORA_FLOAT_MS,
             "FLOAT LoRa");
  assert_u32(schedule_lora_period_ms(MISSION_STATE_BEACON), SCHEDULE_LORA_BEACON_MS,
             "BEACON LoRa");
  assert_u32(schedule_lora_period_ms(MISSION_STATE_PAD), SCHEDULE_LORA_PAD_MS, "PAD LoRa");
  assert_u32(schedule_lora_period_ms(MISSION_STATE_ARMED), SCHEDULE_LORA_ARMED_MS,
             "ARMED LoRa");
  assert_u32(schedule_lora_period_ms(MISSION_STATE_BURST), SCHEDULE_LORA_BURST_MS,
             "BURST LoRa");
  assert_u32(schedule_lora_period_ms(MISSION_STATE_LANDED), SCHEDULE_LORA_LANDED_MS,
             "LANDED LoRa");

  assert_u32(schedule_camera_period_ms(MISSION_STATE_ASCENT), SCHEDULE_CAM_ASCENT_MS,
             "ASCENT cam");
  assert_u32(schedule_camera_period_ms(MISSION_STATE_FLOAT), SCHEDULE_CAM_FLOAT_MS,
             "FLOAT cam");
  assert_u32(schedule_camera_period_ms(MISSION_STATE_DESCENT), 0u, "DESCENT cam off");
  assert_u32(schedule_camera_period_ms(MISSION_STATE_BEACON), 0u, "BEACON cam off");
  assert_u32(schedule_camera_period_ms(MISSION_STATE_PAD), 0u, "PAD cam off");
}

static void test_lora_due_timing(void)
{
  bool lora = false;
  bool cam = false;

  schedule_init();
  schedule_poll(0u, MISSION_STATE_ASCENT, &lora, &cam);
  assert_true(!lora, "first poll not due");
  assert_true(!cam, "first poll cam not due (ASCENT arms)");

  schedule_poll(SCHEDULE_LORA_ASCENT_MS - 1u, MISSION_STATE_ASCENT, &lora, &cam);
  assert_true(!lora, "not due early");

  schedule_poll(SCHEDULE_LORA_ASCENT_MS, MISSION_STATE_ASCENT, &lora, &cam);
  assert_true(lora, "due at period");

  schedule_poll(SCHEDULE_LORA_ASCENT_MS, MISSION_STATE_ASCENT, &lora, &cam);
  assert_true(!lora, "no double-due same time");

  schedule_poll(SCHEDULE_LORA_ASCENT_MS + SCHEDULE_LORA_ASCENT_MS, MISSION_STATE_ASCENT,
                &lora, &cam);
  assert_true(lora, "due again after next period");
}

static void test_state_change_no_immediate(void)
{
  bool lora = false;
  bool cam = false;
  uint32_t t = 0u;

  schedule_init();
  schedule_poll(t, MISSION_STATE_FLOAT, &lora, &cam); /* arm */
  t = SCHEDULE_LORA_FLOAT_MS;
  schedule_poll(t, MISSION_STATE_FLOAT, &lora, &cam);
  assert_true(lora, "FLOAT fire");

  /* Switch to BEACON — must wait BEACON period from last fire, not immediate. */
  schedule_poll(t + 1u, MISSION_STATE_BEACON, &lora, &cam);
  assert_true(!lora, "BEACON not immediate after FLOAT fire");

  schedule_poll(t + SCHEDULE_LORA_BEACON_MS - 1u, MISSION_STATE_BEACON, &lora, &cam);
  assert_true(!lora, "BEACON not early");

  schedule_poll(t + SCHEDULE_LORA_BEACON_MS, MISSION_STATE_BEACON, &lora, &cam);
  assert_true(lora, "BEACON due after its period from last fire");
}

static void test_camera_due_and_off(void)
{
  bool lora = false;
  bool cam = false;
  uint32_t t = 0u;

  schedule_init();
  schedule_poll(t, MISSION_STATE_ASCENT, &lora, &cam);

  schedule_poll(SCHEDULE_CAM_ASCENT_MS - 1u, MISSION_STATE_ASCENT, &lora, &cam);
  assert_true(!cam, "cam not early");

  schedule_poll(SCHEDULE_CAM_ASCENT_MS, MISSION_STATE_ASCENT, &lora, &cam);
  assert_true(cam, "cam due at 30 s");

  /* DESCENT: camera off — never due. */
  schedule_init();
  for (t = 0u; t <= 120000u; t += 1000u)
  {
    schedule_poll(t, MISSION_STATE_DESCENT, &lora, &cam);
    assert_true(!cam, "DESCENT cam never due");
  }
}

int main(void)
{
  test_period_table();
  test_lora_due_timing();
  test_state_change_no_immediate();
  test_camera_due_and_off();

  if (failures == 0)
  {
    printf("PASS test_schedule\n");
    return 0;
  }
  printf("%d failure(s)\n", failures);
  return 1;
}
