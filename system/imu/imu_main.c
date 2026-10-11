/****************************************************************************
 * apps/system/imu/imu_main.c
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
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <nuttx/spi/spi.h>

#ifndef getreg32
#  define getreg32(a) (*(volatile uint32_t *)(a))
#endif

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define DEFAULT_BUS                 0
#define DEFAULT_CHANNEL             3
#define DEFAULT_SPI_FREQ            10000000u
#define DEFAULT_SAMPLE_COUNT        200

/* ICM-20948 Register Definitions */

/* User Bank 0 */

#define ICM20948_WHO_AM_I           0x00
#define ICM20948_USER_CTRL          0x03
#define ICM20948_PWR_MGMT_1         0x06
#define ICM20948_PWR_MGMT_2         0x07
#define ICM20948_ACCEL_XOUT_H       0x2d
#define ICM20948_GYRO_XOUT_H        0x33
#define ICM20948_TEMP_OUT_H         0x39
#define ICM20948_REG_BANK_SEL       0x7f

#define ICM20948_RESET              0x80
#define ICM20948_SLEEP              0x40
#define ICM20948_I2C_IF_DIS         0x10

/* User Bank 2 */

#define ICM20948_GYRO_SMPLRT_DIV    0x00
#define ICM20948_GYRO_CONFIG_1      0x01
#define ICM20948_ODR_ALIGN_EN       0x09
#define ICM20948_ACCEL_SMPLRT_DIV_1 0x10
#define ICM20948_ACCEL_SMPLRT_DIV_2 0x11
#define ICM20948_ACCEL_CONFIG       0x14

#define ICM20948_WHO_AM_I_VAL       0xea

/****************************************************************************
 * External Functions
 ****************************************************************************/

extern FAR struct spi_dev_s *am67a_mcu_spibus_initialize(int port);

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static uint8_t icm_read_reg(FAR struct spi_dev_s *spi, uint32_t devid,
                            uint8_t reg)
{
  uint8_t tx[2] = { (uint8_t)(reg | 0x80), 0x00 };
  uint8_t rx[2] = { 0x00, 0x00 };

  SPI_LOCK(spi, true);
  SPI_SELECT(spi, devid, true);
  SPI_EXCHANGE(spi, tx, rx, 2);
  SPI_SELECT(spi, devid, false);
  SPI_LOCK(spi, false);

  return rx[1];
}

static void icm_write_reg(FAR struct spi_dev_s *spi, uint32_t devid,
                          uint8_t reg, uint8_t val)
{
  uint8_t tx[2] = { (uint8_t)(reg & 0x7f), val };
  uint8_t rx[2] = { 0, 0 };

  SPI_LOCK(spi, true);
  SPI_SELECT(spi, devid, true);
  SPI_EXCHANGE(spi, tx, rx, 2);
  SPI_SELECT(spi, devid, false);
  SPI_LOCK(spi, false);
}

static void icm_read_bytes(FAR struct spi_dev_s *spi, uint32_t devid,
                           uint8_t reg, FAR uint8_t *buf, size_t len)
{
  uint8_t tx[32];
  uint8_t rx[32];

  if (len > 30)
    {
      len = 30;
    }

  memset(tx, 0x00, len + 1);
  memset(rx, 0x00, len + 1);
  tx[0] = (uint8_t)(reg | 0x80);

  SPI_LOCK(spi, true);
  SPI_SELECT(spi, devid, true);
  SPI_EXCHANGE(spi, tx, rx, len + 1);
  SPI_SELECT(spi, devid, false);
  SPI_LOCK(spi, false);

  memcpy(buf, &rx[1], len);
}

static void fmt_fixed(FAR char *buf, size_t len, int32_t milli)
{
  int32_t integer = milli / 1000;
  int32_t frac = abs(milli % 1000);

  if (milli < 0 && integer == 0)
    {
      snprintf(buf, len, "-0.%02" PRId32, frac / 10);
    }
  else
    {
      snprintf(buf, len, "%+" PRId32 ".%02" PRId32, integer, frac / 10);
    }
}

static void show_usage(FAR const char *progname)
{
  printf("Usage: %s [options]\n", progname);
  printf("Options:\n");
  printf("  -b <bus>      SPI bus number (default: %d)\n", DEFAULT_BUS);
  printf("  -c <channel>  SPI CS channel number 0..3 (default: %d)\n",
         DEFAULT_CHANNEL);
  printf("  -f <freq>     SPI frequency in Hz (default: %u)\n",
         DEFAULT_SPI_FREQ);
  printf("  -m <mode>     SPI mode 0..3 (default: 3)\n");
  printf("  -n <count>    Number of samples to read (default: %d)\n",
         DEFAULT_SAMPLE_COUNT);
  printf("  -h            Show this help message\n");
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  FAR struct spi_dev_s *spi;
  int bus = DEFAULT_BUS;
  int channel = DEFAULT_CHANNEL;
  uint32_t freq = DEFAULT_SPI_FREQ;
  int spimode = SPIDEV_MODE3;
  int count = DEFAULT_SAMPLE_COUNT;
  uint32_t devid;
  uint8_t whoami;
  uint8_t user_ctrl;
  int opt;
  int i;

  while ((opt = getopt(argc, argv, "b:c:f:m:n:h")) != -1)
    {
      switch (opt)
        {
          case 'b':
            bus = atoi(optarg);
            break;
          case 'c':
            channel = atoi(optarg);
            break;
          case 'f':
            freq = (uint32_t)strtoul(optarg, NULL, 0);
            break;
          case 'm':
            spimode = atoi(optarg);
            break;
          case 'n':
            count = atoi(optarg);
            break;
          case 'h':
          default:
            show_usage(argv[0]);
            return (opt == 'h') ? 0 : 1;
        }
    }

  devid = SPIDEV_IMU((uint32_t)channel);

  printf("\n========================================\n");
  printf(" ICM-20948 IMU Test Application\n");
  printf(" SPI Bus: %d, Channel (CS): %d, Mode: %d, Speed: %" PRIu32 " Hz\n",
         bus, channel, spimode, freq);
  printf("========================================\n\n");

  spi = am67a_mcu_spibus_initialize(bus);
  if (spi == NULL)
    {
      fprintf(stderr, "ERROR: Failed to initialize SPI bus %d\n", bus);
      return -ENODEV;
    }

  /* Configure SPI bus parameters: Mode 3, 8 bits, 10 MHz */

  SPI_SETMODE(spi, (enum spi_mode_e)spimode);
  SPI_SETBITS(spi, 8);
  SPI_SETFREQUENCY(spi, freq);

  /* Ensure CS starts deasserted */

  SPI_SELECT(spi, devid, false);
  usleep(10000);

  /* Step 1: Select Bank 0 */

  icm_write_reg(spi, devid, ICM20948_REG_BANK_SEL, 0x00);
  usleep(2000);

  /* Step 2: Soft Reset */

  icm_write_reg(spi, devid, ICM20948_PWR_MGMT_1, ICM20948_RESET);

  /* Wait 100 ms for full Power-On Reset / Analog settle */

  usleep(100000);

  /* Step 3: Wake Up: clear sleep & select auto PLL clock */

  icm_write_reg(spi, devid, ICM20948_PWR_MGMT_1, 0x01);
  usleep(20000);

  /* Disable duty-cycling / low-power cycling modes: LP_CONFIG = 0x00 */

  icm_write_reg(spi, devid, 0x05, 0x00);
  usleep(2000);

  /* Step 4: Force Bank 0 */

  icm_write_reg(spi, devid, ICM20948_REG_BANK_SEL, 0x00);
  usleep(2000);

  /* Step 5: Pin interface to SPI by setting I2C_IF_DIS in USER_CTRL */

  user_ctrl = icm_read_reg(spi, devid, ICM20948_USER_CTRL);
  icm_write_reg(spi, devid, ICM20948_USER_CTRL, (uint8_t)(user_ctrl | ICM20948_I2C_IF_DIS));
  usleep(5000);

  /* Step 6: Verify Device ID (WHO_AM_I) */

  whoami = icm_read_reg(spi, devid, ICM20948_WHO_AM_I);
  printf("WHO_AM_I: 0x%02x (Expected: 0x%02x) -> %s\n",
         whoami, ICM20948_WHO_AM_I_VAL,
         (whoami == ICM20948_WHO_AM_I_VAL) ? "MATCH OK!" : "MISMATCH!");

  /* Step 7: Enable all accelerometer and gyroscope axes */

  icm_write_reg(spi, devid, ICM20948_PWR_MGMT_2, 0x00);
  usleep(5000);

  /* Step 8: Select User Bank 2 for sensor configuration */

  icm_write_reg(spi, devid, ICM20948_REG_BANK_SEL, 0x20);
  usleep(2000);

  /* Gyro: +/-2000 dps (FS=3), DLPF 3 (51 Hz cutoff) -> 0x1f */

  icm_write_reg(spi, devid, ICM20948_GYRO_CONFIG_1, 0x1f);
  usleep(2000);

  /* Accel: +/-8g (FS=2), DLPF 3 (50 Hz cutoff) -> 0x1d */

  icm_write_reg(spi, devid, ICM20948_ACCEL_CONFIG, 0x1d);
  usleep(2000);

  /* Synchronize sample clocks */

  icm_write_reg(spi, devid, ICM20948_ODR_ALIGN_EN, 0x01);
  usleep(2000);

  /* Set sample rate dividers (102 Hz) */

  icm_write_reg(spi, devid, ICM20948_GYRO_SMPLRT_DIV, 10);
  icm_write_reg(spi, devid, ICM20948_ACCEL_SMPLRT_DIV_1, 0);
  icm_write_reg(spi, devid, ICM20948_ACCEL_SMPLRT_DIV_2, 10);
  usleep(2000);

  /* Return to User Bank 0 */

  icm_write_reg(spi, devid, ICM20948_REG_BANK_SEL, 0x00);
  usleep(2000);

  printf("Sensors configured (+/-8g, +/-2000 dps, DLPF on, ODR synced).\n");
  printf("Reading %d samples (14-byte burst read):\n\n", count);
  printf(" Sample |     Accel (g) [X, Y, Z]      |     Gyro (dps) [X, Y, Z]     | Temp\n");
  printf("--------+------------------------------+------------------------------+---------\n");

  /* Settle time for Gyro PLL lock, charge pump & DLPF pipeline (Datasheet: up to 100ms) */

  printf("Waiting 250 ms for Gyro PLL lock, charge pump & DLPF settling...\n");
  usleep(250000);

  uint8_t pwr1 = icm_read_reg(spi, devid, ICM20948_PWR_MGMT_1);
  printf("Pre-loop check: PWR_MGMT_1=0x%02x (expected 0x01)\n\n", pwr1);

  for (i = 0; i < count; i++)
    {
      uint8_t raw[14];
      int16_t ax;
      int16_t ay;
      int16_t az;
      int16_t gx;
      int16_t gy;
      int16_t gz;
      int16_t temp_raw;

      /* Read all 14 sensor registers in one single contiguous SPI burst */

      icm_read_bytes(spi, devid, ICM20948_ACCEL_XOUT_H, raw, 14);

      ax = (int16_t)(((uint16_t)raw[0] << 8) | raw[1]);
      ay = (int16_t)(((uint16_t)raw[2] << 8) | raw[3]);
      az = (int16_t)(((uint16_t)raw[4] << 8) | raw[5]);
      gx = (int16_t)(((uint16_t)raw[6] << 8) | raw[7]);
      gy = (int16_t)(((uint16_t)raw[8] << 8) | raw[9]);
      gz = (int16_t)(((uint16_t)raw[10] << 8) | raw[11]);
      temp_raw = (int16_t)(((uint16_t)raw[12] << 8) | raw[13]);

      int32_t ax_mg = (int32_t)ax * 1000 / 4096;
      int32_t ay_mg = (int32_t)ay * 1000 / 4096;
      int32_t az_mg = (int32_t)az * 1000 / 4096;

      int32_t gx_mdps = (int32_t)gx * 10000 / 164;
      int32_t gy_mdps = (int32_t)gy * 10000 / 164;
      int32_t gz_mdps = (int32_t)gz * 10000 / 164;

      int32_t temp_mc = ((int32_t)temp_raw * 1000 / 334) + 21000;

      char sax[16];
      char say[16];
      char saz[16];
      char sgx[16];
      char sgy[16];
      char sgz[16];
      char stemp[16];

      fmt_fixed(sax, sizeof(sax), ax_mg);
      fmt_fixed(say, sizeof(say), ay_mg);
      fmt_fixed(saz, sizeof(saz), az_mg);

      fmt_fixed(sgx, sizeof(sgx), gx_mdps / 10);
      fmt_fixed(sgy, sizeof(sgy), gy_mdps / 10);
      fmt_fixed(sgz, sizeof(sgz), gz_mdps / 10);

      fmt_fixed(stemp, sizeof(stemp), temp_mc);

      /* Read real-time hardware status of IMU_EN pin and PADCONFIG */

      uint32_t gpio_dir = getreg32(0x04201010ul);
      uint32_t gpio_out = getreg32(0x04201014ul);
      uint32_t gpio_in  = getreg32(0x04201020ul);
      uint32_t padcfg   = getreg32(0x04084030ul);

      int en_dir_out = ((gpio_dir & (1u << 12)) == 0) ? 1 : 0;
      int en_out_val = ((gpio_out & (1u << 12)) != 0) ? 1 : 0;
      int en_in_phys = ((gpio_in & (1u << 12)) != 0) ? 1 : 0;
      int pmux_mode  = (int)(padcfg & 0x0fu);

      printf(" #%5d | %6s, %6s, %6s | %6s, %6s, %6s | %5s C | [IMU_EN: phys=%d out=%d dir_out=%d mux=%d]\n",
             i + 1, sax, say, saz, sgx, sgy, sgz, stemp,
             en_in_phys, en_out_val, en_dir_out, pmux_mode);

      usleep(10000); /* 10 ms (100 Hz) */
    }

  printf("\nIMU test completed.\n");
  return 0;
}
