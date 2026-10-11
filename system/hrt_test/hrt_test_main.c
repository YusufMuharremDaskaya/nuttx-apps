/****************************************************************************
 * apps/system/hrt_test/hrt_test_main.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>

#include <arch/chip/am67a_mcu_hrt.h>

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct latency_test_s
{
  volatile bool     fired;
  hrt_abstime       target_deadline;
  hrt_abstime       actual_time;
};

struct periodic_test_s
{
  volatile uint32_t count;
  hrt_abstime       last_time;
  int64_t           min_delta_us;
  int64_t           max_delta_us;
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void latency_callback(void *arg)
{
  struct latency_test_s *test = (struct latency_test_s *)arg;
  test->actual_time = hrt_absolute_time();
  test->fired = true;
}

static void periodic_callback(void *arg)
{
  struct periodic_test_s *test = (struct periodic_test_s *)arg;
  hrt_abstime now = hrt_absolute_time();

  if (test->count > 0)
    {
      int64_t delta = (int64_t)(now - test->last_time);
      if (delta < test->min_delta_us)
        {
          test->min_delta_us = delta;
        }
      if (delta > test->max_delta_us)
        {
          test->max_delta_us = delta;
        }
    }

  test->last_time = now;
  test->count++;
}

static int test_clock_accuracy(void)
{
  hrt_abstime t0;
  hrt_abstime t1;
  hrt_abstime elapsed;
  hrt_abstime prev;
  hrt_abstime curr;
  bool monotonic = true;
  uint32_t min_step = 0xffffffff;
  int i;

  printf("\n=== Test 1: Monotonicity & Resolution (100,000 iterations) ===\n");

  prev = hrt_absolute_time();
  for (i = 0; i < 100000; i++)
    {
      curr = hrt_absolute_time();
      if (curr < prev)
        {
          monotonic = false;
          printf("ERROR: Time jumped backwards! prev=%" PRIu64 ", curr=%" PRIu64 "\n",
                 prev, curr);
          break;
        }

      if (curr > prev)
        {
          uint32_t step = (uint32_t)(curr - prev);
          if (step < min_step)
            {
              min_step = step;
            }
        }

      prev = curr;
    }

  if (monotonic)
    {
      printf("[PASS] Monotonic check OK (Zero backwards jumps).\n");
      printf("       Smallest observed non-zero step: %" PRIu32 " us\n", min_step);
    }
  else
    {
      printf("[FAIL] Monotonicity test failed!\n");
      return -1;
    }

  printf("\n=== Test 2: Wall-Clock 1-Second Accuracy Test ===\n");
  printf("Measuring 1-second elapsed time via sleep(1)...\n");

  t0 = hrt_absolute_time();
  sleep(1);
  t1 = hrt_absolute_time();
  elapsed = t1 - t0;

  printf("t0:      %" PRIu64 " us\n", t0);
  printf("t1:      %" PRIu64 " us\n", t1);
  printf("Elapsed: %" PRIu64 " us (Expected: ~1000000 us)\n", elapsed);

  if (elapsed >= 995000ULL && elapsed <= 1005000ULL)
    {
      double error_pct = ((double)((int64_t)elapsed - 1000000) / 1000000.0) * 100.0;
      printf("[PASS] Frequency accuracy within +/-0.5%% (Error: %+.3f%%)\n", error_pct);
    }
  else
    {
      printf("[FAIL] Frequency mismatch! Measured %" PRIu64 " us.\n", elapsed);
      return -1;
    }

  printf("\n=== Test 3: Short 100ms Accuracy via usleep(100000) ===\n");

  t0 = hrt_absolute_time();
  usleep(100000);
  t1 = hrt_absolute_time();
  elapsed = t1 - t0;

  printf("Elapsed: %" PRIu64 " us (Expected: ~100000 us)\n", elapsed);
  if (elapsed >= 98000ULL && elapsed <= 105000ULL)
    {
      printf("[PASS] 100ms sleep check OK.\n");
    }
  else
    {
      printf("[WARN] 100ms sleep drift (Measured: %" PRIu64 " us)\n", elapsed);
    }

  return 0;
}

static int test_callout_latency(void)
{
  struct hrt_call call;
  struct latency_test_s test;
  hrt_abstime now;
  hrt_abstime delay = 50000; /* 50 ms */
  int timeout_ms = 100;

  printf("\n=== Test 4: Hardware Match Callout Latency (50 ms one-shot) ===\n");

  memset(&test, 0, sizeof(test));
  now = hrt_absolute_time();
  test.target_deadline = now + delay;

  hrt_call_after(&call, delay, latency_callback, &test);

  while (!test.fired && timeout_ms-- > 0)
    {
      usleep(1000);
    }

  if (!test.fired)
    {
      printf("[FAIL] Callout did not fire within timeout!\n");
      hrt_cancel(&call);
      return -1;
    }

  int64_t error = (int64_t)(test.actual_time - test.target_deadline);
  printf("Scheduled deadline: %" PRIu64 " us\n", test.target_deadline);
  printf("Actual fire time:   %" PRIu64 " us\n", test.actual_time);
  printf("Latency error:      %+" PRId64 " us\n", error);

  if (error >= 0 && error < 50)
    {
      printf("[PASS] Callout latency is under 50 us (Extremely low jitter!)\n");
      return 0;
    }
  else
    {
      printf("[WARN] High callout latency: %+" PRId64 " us\n", error);
      return 0;
    }
}

static int test_periodic_imu_loop(void)
{
  struct hrt_call call;
  struct periodic_test_s test;

  printf("\n=== Test 5: 1000 Hz Periodic Callout Simulation (1000 us period) ===\n");
  printf("Running for 1 second...\n");

  test.count        = 0;
  test.last_time    = hrt_absolute_time();
  test.min_delta_us = 999999;
  test.max_delta_us = 0;

  hrt_call_every(&call, 1000, 1000, periodic_callback, &test);

  sleep(1);

  hrt_cancel(&call);

  printf("Total callbacks fired in 1 sec: %" PRIu32 " (Expected: ~1000)\n", test.count);
  printf("Min interval: %" PRId64 " us\n", test.min_delta_us);
  printf("Max interval: %" PRId64 " us\n", test.max_delta_us);

  if (test.count >= 950 && test.count <= 1050)
    {
      printf("[PASS] 1000 Hz loop rate verified!\n");
      return 0;
    }
  else
    {
      printf("[FAIL] Callout count mismatch: %" PRIu32 "\n", test.count);
      return -1;
    }
}

static int test_cancel(void)
{
  struct hrt_call call;
  struct latency_test_s test;

  printf("\n=== Test 6: Callout Cancellation Test ===\n");

  memset(&test, 0, sizeof(test));
  hrt_call_after(&call, 100000, latency_callback, &test); /* 100 ms */

  /* Immediately cancel */

  hrt_cancel(&call);

  /* Wait 150 ms to ensure it never fires */

  usleep(150000);

  if (test.fired)
    {
      printf("[FAIL] Cancelled callout still executed!\n");
      return -1;
    }
  else
    {
      printf("[PASS] Callout successfully cancelled (Did not fire).\n");
      return 0;
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  printf("========================================\n");
  printf(" AM67A MCU High Resolution Timer (HRT)  \n");
  printf(" Hardware: MCU_TIMER1 (0x04810000)      \n");
  printf(" Clock: 25 MHz (HFOSC0), 40 ns tick     \n");
  printf("========================================\n");

  if (argc > 1)
    {
      if (strcmp(argv[1], "clock") == 0)
        {
          return test_clock_accuracy();
        }
      else if (strcmp(argv[1], "latency") == 0)
        {
          return test_callout_latency();
        }
      else if (strcmp(argv[1], "periodic") == 0)
        {
          return test_periodic_imu_loop();
        }
      else if (strcmp(argv[1], "cancel") == 0)
        {
          return test_cancel();
        }
      else
        {
          printf("Usage: hrt_test [clock|latency|periodic|cancel|all]\n");
          return -1;
        }
    }

  /* Default: Run entire suite */

  if (test_clock_accuracy() < 0)
    {
      return -1;
    }

  if (test_callout_latency() < 0)
    {
      return -1;
    }

  if (test_periodic_imu_loop() < 0)
    {
      return -1;
    }

  if (test_cancel() < 0)
    {
      return -1;
    }

  printf("\n>>> ALL HRT TESTS PASSED SUCCESSFULLY! <<<\n");
  return 0;
}
