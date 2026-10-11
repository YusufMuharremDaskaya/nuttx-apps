/****************************************************************************
 * apps/system/pwm_test/pwm_test_main.c
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
#include <string.h>
#include <unistd.h>
#include <arch/chip/am67a_mcu_pwm.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define PWM_DEFAULT_FREQ 400

static const char *g_chan_pin_names[AM67A_MCU_PWM_NCHANNELS] =
{
  "Hat Pin 29 (EPWM0_A)",
  "Hat Pin  8 (EPWM0_B)",
  "Hat Pin 31 (EPWM1_A)",
  "Hat Pin 33 (EPWM1_B)",
  "Hat Pin 32 (ECAP0_APWM)",
  "Hat Pin 36 (ECAP1_APWM)",
  "Hat Pin 12 (ECAP2_APWM)",
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void print_usage(void)
{
  printf("====================================================\n");
  printf("  AM67A MCU Domain 7-Channel PWM Utility            \n");
  printf("====================================================\n");
  printf("Channels / Physical Pins:\n");
  printf("  CH0 (Motor 1): Hat Pin 29 -> EPWM0_A\n");
  printf("  CH1 (Motor 2): Hat Pin  8 -> EPWM0_B\n");
  printf("  CH2 (Motor 3): Hat Pin 31 -> EPWM1_A\n");
  printf("  CH3 (Motor 4): Hat Pin 33 -> EPWM1_B\n");
  printf("  CH4 (Aux 1)  : Hat Pin 32 -> ECAP0_APWM\n");
  printf("  CH5 (Aux 2)  : Hat Pin 36 -> ECAP1_APWM\n");
  printf("  CH6 (Aux 3)  : Hat Pin 12 -> ECAP2_APWM\n\n");
  printf("Usage:\n");
  printf("  pwm_test <ch 0-6> <freq_hz> <pulse_us>\n");
  printf("      Set single channel (e.g. pwm_test 0 400 1500)\n\n");
  printf("  pwm_test all <freq_hz> <pulse_us>\n");
  printf("      Set all 7 channels (e.g. pwm_test all 400 1500)\n\n");
  printf("  pwm_test ramp <ch 0-6> [freq_hz]\n");
  printf("      Sweep pulse from 1000 us -> 2000 us -> 1000 us\n\n");
  printf("  pwm_test stop [ch 0-6]\n");
  printf("      Stop pulse output (force LOW) on channel or all\n\n");
  printf("Common ESC / Servo Pulse Widths (at 400 Hz / 50 Hz):\n");
  printf("  1000 us : Minimum Throttle / Position\n");
  printf("  1500 us : Neutral / Mid-Throttle\n");
  printf("  2000 us : Full Throttle / Max Position\n");
  printf("====================================================\n");
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  if (argc < 2)
    {
      print_usage();
      return 0;
    }

  /* Make sure ePWM hardware is initialized */

  am67a_mcu_pwm_init();

  /* 1. Stop command */

  if (strcmp(argv[1], "stop") == 0)
    {
      if (argc >= 3)
        {
          int ch = atoi(argv[2]);
          if (ch < 0 || ch >= AM67A_MCU_PWM_NCHANNELS)
            {
              printf("[FAIL] Invalid channel %d (valid: 0-%d)\n",
                     ch, AM67A_MCU_PWM_NCHANNELS - 1);
              return 1;
            }

          am67a_mcu_pwm_stop(ch);
          printf("[OK] Stopped CH%d [%s] -> Forced LOW (0V)\n",
                 ch, g_chan_pin_names[ch]);
        }
      else
        {
          am67a_mcu_pwm_stop_all();
          printf("[OK] Stopped all %d PWM channels -> Forced LOW (0V)\n",
                 AM67A_MCU_PWM_NCHANNELS);
        }
      return 0;
    }

  /* 2. All channels command */

  if (strcmp(argv[1], "all") == 0)
    {
      if (argc < 4)
        {
          printf("Usage: pwm_test all <freq_hz> <pulse_us>\n");
          return 1;
        }

      uint32_t freq_hz  = (uint32_t)strtoul(argv[2], NULL, 0);
      uint32_t pulse_us = (uint32_t)strtoul(argv[3], NULL, 0);

      for (int i = 0; i < AM67A_MCU_PWM_NCHANNELS; i++)
        {
          am67a_mcu_pwm_set_freq(i, freq_hz);
          am67a_mcu_pwm_set_duty_us(i, pulse_us);
        }

      float duty_pct = (pulse_us * freq_hz * 100.0f) / 1000000.0f;
      printf("[OK] All %d Channels configured:\n", AM67A_MCU_PWM_NCHANNELS);
      printf("     Frequency : %lu Hz (Period = %lu us)\n",
             freq_hz, 1000000ul / freq_hz);
      printf("     Pulse     : %lu us (%.2f %% duty)\n",
             pulse_us, duty_pct);
      for (int i = 0; i < AM67A_MCU_PWM_NCHANNELS; i++)
        {
          printf("     CH%d -> %s : ACTIVE\n", i, g_chan_pin_names[i]);
        }
      return 0;
    }

  /* 3. Ramp / sweep command */

  if (strcmp(argv[1], "ramp") == 0)
    {
      if (argc < 3)
        {
          printf("Usage: pwm_test ramp <ch 0-%d> [freq_hz]\n",
                 AM67A_MCU_PWM_NCHANNELS - 1);
          return 1;
        }

      int ch = atoi(argv[2]);
      if (ch < 0 || ch >= AM67A_MCU_PWM_NCHANNELS)
        {
          printf("[FAIL] Invalid channel %d (valid: 0-%d)\n",
                 ch, AM67A_MCU_PWM_NCHANNELS - 1);
          return 1;
        }

      uint32_t freq_hz = PWM_DEFAULT_FREQ;
      if (argc >= 4)
        {
          freq_hz = (uint32_t)strtoul(argv[3], NULL, 0);
        }

      printf("Starting throttle ramp on CH%d [%s] at %lu Hz...\n",
             ch, g_chan_pin_names[ch], freq_hz);
      am67a_mcu_pwm_set_freq(ch, freq_hz);

      /* Ramp UP: 1000 us to 2000 us */

      printf("--- Ramping UP (1000 us -> 2000 us) ---\n");
      for (uint32_t p = 1000; p <= 2000; p += 100)
        {
          am67a_mcu_pwm_set_duty_us(ch, p);
          printf("  Pulse: %4lu us\n", p);
          usleep(150000); /* 150 ms per step */
        }

      sleep(1);

      /* Ramp DOWN: 2000 us to 1000 us */

      printf("--- Ramping DOWN (2000 us -> 1000 us) ---\n");
      for (int32_t p = 2000; p >= 1000; p -= 100)
        {
          am67a_mcu_pwm_set_duty_us(ch, (uint32_t)p);
          printf("  Pulse: %4ld us\n", (long)p);
          usleep(150000);
        }

      printf("[OK] Ramp complete. Channel left at 1000 us (idle).\n");
      return 0;
    }

  /* 4. Single channel set command: pwm_test <ch> <freq> <duty_us> */

  int ch = atoi(argv[1]);
  if (ch < 0 || ch >= AM67A_MCU_PWM_NCHANNELS)
    {
      printf("[FAIL] Invalid channel '%s'. Must be 0 to 3, 'all', 'ramp', or 'stop'.\n",
             argv[1]);
      print_usage();
      return 1;
    }

  if (argc < 4)
    {
      printf("Usage: pwm_test <ch 0-3> <freq_hz> <pulse_us>\n");
      return 1;
    }

  uint32_t freq_hz  = (uint32_t)strtoul(argv[2], NULL, 0);
  uint32_t pulse_us = (uint32_t)strtoul(argv[3], NULL, 0);

  int ret = am67a_mcu_pwm_set_freq(ch, freq_hz);
  if (ret < 0)
    {
      printf("[FAIL] Failed to set frequency %lu Hz (ret = %d)\n", freq_hz, ret);
      return 1;
    }

  ret = am67a_mcu_pwm_set_duty_us(ch, pulse_us);
  if (ret < 0)
    {
      printf("[FAIL] Failed to set duty %lu us (ret = %d)\n", pulse_us, ret);
      return 1;
    }

  float duty_pct = (pulse_us * freq_hz * 100.0f) / 1000000.0f;
  printf("[OK] CH%d [%s]:\n", ch, g_chan_pin_names[ch]);
  printf("     Frequency : %lu Hz (Period = %lu us)\n",
         freq_hz, 1000000ul / freq_hz);
  printf("     Pulse     : %lu us (%.2f %% duty)\n",
         pulse_us, duty_pct);
  printf("     State     : %s\n", (pulse_us > 0) ? "ACTIVE" : "STOPPED (LOW)");

  return 0;
}
