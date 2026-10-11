/****************************************************************************
 * apps/system/eeprom_test/eeprom_test_main.c
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
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>
#include <nuttx/i2c/i2c_master.h>
#include <nuttx/mtd/mtd.h>
#include <nuttx/fs/fs.h>
#include <sys/mount.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define EEPROM_DEV_PATH       "/dev/eeprom"
#define MTD_DEV_PATH          "/fs/mtd_params"
#define I2C1_DEV_PATH         "/dev/i2c1"
#define I2C0_DEV_PATH         "/dev/i2c0"

#define AT24C512_TOTAL_SIZE   65536  /* 64 KiB */
#define AT24C512_PAGE_SIZE    128    /* 128 Bytes */
#define AT24C512_I2C_ADDR     0x51   /* 7-bit slave address on WKUP_I2C0 */

#define MAGIC_SIGNATURE       "T3-GEM-O1:AT24C512C:PERSIST_VERIFIED"
#define MAGIC_OFFSET          0

/* AM67A Hardware Register Bases for Direct Diagnostics */

#define WKUP_I2C0_BASE        0x2b200000ul
#define MCU_I2C0_BASE         0x04900000ul
#define MCU_PADCFG_PMUX_BASE  0x04084000ul

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void print_hexdump(off_t offset, const uint8_t *data, size_t len)
{
  size_t i;
  size_t j;

  for (i = 0; i < len; i += 16)
    {
      printf("  0x%04lx: ", (unsigned long)(offset + i));

      /* Hex bytes */

      for (j = 0; j < 16; j++)
        {
          if (i + j < len)
            {
              printf("%02x ", data[i + j]);
            }
          else
            {
              printf("   ");
            }
          if (j == 7)
            {
              printf(" ");
            }
        }

      printf(" |");

      /* ASCII characters */

      for (j = 0; j < 16 && (i + j) < len; j++)
        {
          uint8_t c = data[i + j];
          printf("%c", isprint(c) ? c : '.');
        }

      printf("|\n");
    }
}

static void print_usage(void)
{
  printf("===================================================================\n");
  printf("  T3-GEM-O1 / AM67A MCU Domain AT24C512C EEPROM & MTD Test Utility \n");
  printf("===================================================================\n");
  printf("Usage: eeprom_test <command> [arguments...]\n\n");
  printf("Commands:\n");
  printf("  info                    Show hardware, bus, and driver layout\n");
  printf("  diag                    Dump low-level I2C controller & padconf MMRs\n");
  printf("  scan  [bus 0|1]         Scan I2C bus addresses 0x03..0x77 (like i2cdetect)\n");
  printf("  probe [bus] [addr]      Verify ACK from device (default bus 1, addr 0x51)\n");
  printf("  read  [offset] [len]    Read and hex-dump bytes from /dev/eeprom\n");
  printf("  write <offset> <text>   Write string to /dev/eeprom and verify\n");
  printf("  dump  [offset] [lines]  Dump memory (16 bytes/line, default 4 lines)\n");
  printf("  benchmark [bytes]       Measure read & write throughput across pages\n");
  printf("  verify                  Write magic signature for reboot test\n");
  printf("  check                   Check if magic signature persisted across reboot\n");
  printf("  mtd                     Inspect /fs/mtd_params geometry & status\n");
  printf("===================================================================\n");
}

static int cmd_info(void)
{
  struct stat st;

  printf("-------------------------------------------------------------------\n");
  printf(" AT24C512C EEPROM & MTD Storage Specifications:\n");
  printf("-------------------------------------------------------------------\n");
  printf("  Part Name:      Microchip / Atmel AT24C512C\n");
  printf("  Capacity:       512 Kbits = 65,536 Bytes (64 KiB)\n");
  printf("  Page Size:      128 Bytes (supports 128-byte page write)\n");
  printf("  Bus Interface:  WKUP_I2C0 (/dev/i2c1) at 400 kHz Fast Mode\n");
  printf("  I2C Address:    0x51 (7-bit)\n");
  printf("  Internal Addr:  16-bit (2 address bytes per transfer)\n");
  printf("  Endurance:      1,000,000 write cycles / 100-year retention\n");
  printf("-------------------------------------------------------------------\n");
  printf(" Driver Node Status:\n");

  if (stat(I2C0_DEV_PATH, &st) == 0)
    {
      printf("  [OK] %-16s : Present (MCU_I2C0 Bus Master 0)\n", I2C0_DEV_PATH);
    }
  else
    {
      printf("  [--] %-16s : Not found\n", I2C0_DEV_PATH);
    }

  if (stat(I2C1_DEV_PATH, &st) == 0)
    {
      printf("  [OK] %-16s : Present (WKUP_I2C0 Bus Master 1)\n", I2C1_DEV_PATH);
    }
  else
    {
      printf("  [--] %-16s : Not found\n", I2C1_DEV_PATH);
    }

  if (stat(EEPROM_DEV_PATH, &st) == 0)
    {
      printf("  [OK] %-16s : Present (POSIX Character Driver)\n", EEPROM_DEV_PATH);
    }
  else
    {
      printf("  [--] %-16s : Not found\n", EEPROM_DEV_PATH);
    }

  if (stat(MTD_DEV_PATH, &st) == 0)
    {
      printf("  [OK] %-16s : Present (MTD Parameter Storage)\n", MTD_DEV_PATH);
    }
  else
    {
      printf("  [--] %-16s : Not found\n", MTD_DEV_PATH);
    }

  printf("-------------------------------------------------------------------\n");
  return OK;
}

static int cmd_diag(void)
{
  volatile uint32_t *wkup_i2c = (volatile uint32_t *)WKUP_I2C0_BASE;
  volatile uint32_t *mcu_i2c  = (volatile uint32_t *)MCU_I2C0_BASE;
  volatile uint32_t *pmux     = (volatile uint32_t *)MCU_PADCFG_PMUX_BASE;

  printf("-------------------------------------------------------------------\n");
  printf(" Hardware MMR Diagnostics:\n");
  printf("-------------------------------------------------------------------\n");

  /* Pad configuration MMRs */

  printf("Padconf Registers (Base 0x%08lx):\n", (unsigned long)MCU_PADCFG_PMUX_BASE);
  printf("  WKUP_I2C0_SCL (0x04C): 0x%08lx (Mode=%lu, InEn=%lu, PullDir=%lu, PullDis=%lu)\n",
         (unsigned long)pmux[0x4c / 4],
         (unsigned long)(pmux[0x4c / 4] & 0x0f),
         (unsigned long)((pmux[0x4c / 4] >> 18) & 1),
         (unsigned long)((pmux[0x4c / 4] >> 17) & 1),
         (unsigned long)((pmux[0x4c / 4] >> 16) & 1));
  printf("  WKUP_I2C0_SDA (0x050): 0x%08lx (Mode=%lu, InEn=%lu, PullDir=%lu, PullDis=%lu)\n",
         (unsigned long)pmux[0x50 / 4],
         (unsigned long)(pmux[0x50 / 4] & 0x0f),
         (unsigned long)((pmux[0x50 / 4] >> 18) & 1),
         (unsigned long)((pmux[0x50 / 4] >> 17) & 1),
         (unsigned long)((pmux[0x50 / 4] >> 16) & 1));
  printf("  MCU_I2C0_SCL  (0x044): 0x%08lx\n", (unsigned long)pmux[0x44 / 4]);
  printf("  MCU_I2C0_SDA  (0x048): 0x%08lx\n", (unsigned long)pmux[0x48 / 4]);

  printf("\nWKUP_I2C0 Controller (Base 0x%08lx):\n", (unsigned long)WKUP_I2C0_BASE);
  uint32_t wkup_syss = wkup_i2c[0x90 / 4];
  uint32_t wkup_con  = wkup_i2c[0xa4 / 4];
  uint32_t wkup_raw  = wkup_i2c[0x24 / 4];
  uint32_t wkup_psc  = wkup_i2c[0xb0 / 4];
  uint32_t wkup_scll = wkup_i2c[0xb4 / 4];
  uint32_t wkup_sclh = wkup_i2c[0xb8 / 4];

  printf("  SYSS:    0x%08lx (ResetDone=%lu)\n", (unsigned long)wkup_syss, (unsigned long)(wkup_syss & 1));
  printf("  CON:     0x%08lx (EN=%lu, MST=%lu, TRX=%lu, STP=%lu, STT=%lu)\n",
         (unsigned long)wkup_con,
         (unsigned long)((wkup_con >> 15) & 1),
         (unsigned long)((wkup_con >> 10) & 1),
         (unsigned long)((wkup_con >> 9) & 1),
         (unsigned long)((wkup_con >> 1) & 1),
         (unsigned long)(wkup_con & 1));
  printf("  RAW_STAT:0x%08lx (BB=%lu, XRDY=%lu, RRDY=%lu, ARDY=%lu, NACK=%lu, AL=%lu)\n",
         (unsigned long)wkup_raw,
         (unsigned long)((wkup_raw >> 12) & 1),
         (unsigned long)((wkup_raw >> 4) & 1),
         (unsigned long)((wkup_raw >> 3) & 1),
         (unsigned long)((wkup_raw >> 2) & 1),
         (unsigned long)((wkup_raw >> 1) & 1),
         (unsigned long)(wkup_raw & 1));
  printf("  Timing:  PSC=%lu, SCLL=%lu, SCLH=%lu\n",
         (unsigned long)wkup_psc, (unsigned long)wkup_scll, (unsigned long)wkup_sclh);

  printf("\nMCU_I2C0 Controller (Base 0x%08lx):\n", (unsigned long)MCU_I2C0_BASE);
  uint32_t mcu_syss = mcu_i2c[0x90 / 4];
  uint32_t mcu_con  = mcu_i2c[0xa4 / 4];
  uint32_t mcu_raw  = mcu_i2c[0x24 / 4];
  printf("  SYSS:    0x%08lx, CON: 0x%08lx, RAW_STAT: 0x%08lx (BB=%lu)\n",
         (unsigned long)mcu_syss, (unsigned long)mcu_con, (unsigned long)mcu_raw,
         (unsigned long)((mcu_raw >> 12) & 1));
  printf("-------------------------------------------------------------------\n");
  return OK;
}

static int cmd_scan(int argc, char *argv[])
{
  int bus = 1;
  char devpath[32];
  int fd;
  int addr;
  int found = 0;

  if (argc > 2)
    {
      bus = atoi(argv[2]);
    }

  snprintf(devpath, sizeof(devpath), "/dev/i2c%d", bus);
  printf("Scanning I2C bus %d (%s) for devices (0x03 to 0x77)...\n\n", bus, devpath);

  fd = open(devpath, O_RDWR);
  if (fd < 0)
    {
      printf("ERROR: Cannot open %s: %s (errno %d)\n", devpath, strerror(errno), errno);
      return -errno;
    }

  printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");

  for (addr = 0; addr < 128; addr += 16)
    {
      printf("%02x: ", addr);

      for (int col = 0; col < 16; col++)
        {
          int cur_addr = addr + col;

          if (cur_addr < 0x03 || cur_addr > 0x77)
            {
              printf("   ");
              continue;
            }

          /* Perform a 1-byte read test to see if address responds with ACK */

          struct i2c_msg_s msg;
          uint8_t dummy = 0;
          struct i2c_transfer_s xfer;

          msg.frequency = 100000;
          msg.addr      = cur_addr;
          msg.flags     = I2C_M_READ;
          msg.buffer    = &dummy;
          msg.length    = 1;

          xfer.msgv     = &msg;
          xfer.msgc     = 1;

          int ret = ioctl(fd, I2CIOC_TRANSFER, (unsigned long)((uintptr_t)&xfer));
          if (ret >= 0)
            {
              printf("%02x ", cur_addr);
              found++;
            }
          else
            {
              printf("-- ");
            }
        }
      printf("\n");
    }

  close(fd);
  printf("\nFound %d device(s) on bus %d.\n", found, bus);
  return OK;
}

static int cmd_probe(int argc, char *argv[])
{
  int bus = 1;
  uint8_t addr = AT24C512_I2C_ADDR;
  char devpath[32];
  int fd;

  if (argc > 2)
    {
      bus = atoi(argv[2]);
    }
  if (argc > 3)
    {
      addr = (uint8_t)strtoul(argv[3], NULL, 0);
    }

  snprintf(devpath, sizeof(devpath), "/dev/i2c%d", bus);
  printf("Probing address 0x%02x on %s...\n", addr, devpath);

  fd = open(devpath, O_RDWR);
  if (fd < 0)
    {
      printf("ERROR: Failed to open %s: %s (errno %d)\n", devpath, strerror(errno), errno);
      return -errno;
    }

  struct i2c_msg_s msg;
  uint8_t dummy = 0;
  struct i2c_transfer_s xfer;

  msg.frequency = 100000;
  msg.addr      = addr;
  msg.flags     = I2C_M_READ;
  msg.buffer    = &dummy;
  msg.length    = 1;

  xfer.msgv     = &msg;
  xfer.msgc     = 1;

  int ret = ioctl(fd, I2CIOC_TRANSFER, (unsigned long)((uintptr_t)&xfer));
  close(fd);

  if (ret >= 0)
    {
      printf("SUCCESS: Device at 0x%02x on %s ACKed! Read byte: 0x%02x\n",
             addr, devpath, dummy);
      return OK;
    }
  else
    {
      printf("ERROR: Probe failed (errno %d: %s)\n", errno, strerror(errno));
      printf("Checking hardware register status...\n");
      cmd_diag();
      return -errno;
    }
}

static int cmd_read(int argc, char *argv[])
{
  off_t offset = 0;
  size_t len = 64;
  int fd;
  uint8_t *buf;
  ssize_t nread;

  if (argc > 2)
    {
      offset = (off_t)strtoul(argv[2], NULL, 0);
    }
  if (argc > 3)
    {
      len = (size_t)strtoul(argv[3], NULL, 0);
    }

  if (offset >= AT24C512_TOTAL_SIZE)
    {
      printf("ERROR: Offset 0x%lx exceeds EEPROM capacity (64 KiB)\n",
             (unsigned long)offset);
      return -EINVAL;
    }

  if (offset + len > AT24C512_TOTAL_SIZE)
    {
      len = AT24C512_TOTAL_SIZE - offset;
    }

  buf = (uint8_t *)malloc(len);
  if (!buf)
    {
      printf("ERROR: Failed to allocate %zu bytes buffer\n", len);
      return -ENOMEM;
    }

  fd = open(EEPROM_DEV_PATH, O_RDONLY);
  if (fd < 0)
    {
      printf("ERROR: Failed to open %s: %s\n", EEPROM_DEV_PATH, strerror(errno));
      free(buf);
      return -errno;
    }

  if (lseek(fd, offset, SEEK_SET) < 0)
    {
      printf("ERROR: Seek failed: %s\n", strerror(errno));
      close(fd);
      free(buf);
      return -errno;
    }

  nread = read(fd, buf, len);
  close(fd);

  if (nread < 0)
    {
      printf("ERROR: Read failed: %s\n", strerror(errno));
      free(buf);
      return -errno;
    }

  printf("Read %zd bytes from offset 0x%04lx:\n", nread, (unsigned long)offset);
  print_hexdump(offset, buf, nread);

  free(buf);
  return OK;
}

static int cmd_write(int argc, char *argv[])
{
  off_t offset;
  const char *text;
  size_t len;
  int fd;
  ssize_t nwritten;
  uint8_t *verify_buf;
  ssize_t nread;

  if (argc < 4)
    {
      printf("Usage: eeprom_test write <offset> <text>\n");
      return -EINVAL;
    }

  offset = (off_t)strtoul(argv[2], NULL, 0);
  text = argv[3];
  len = strlen(text);

  if (offset >= AT24C512_TOTAL_SIZE || (offset + len) > AT24C512_TOTAL_SIZE)
    {
      printf("ERROR: Offset 0x%lx + len %zu exceeds 64 KiB\n",
             (unsigned long)offset, len);
      return -EINVAL;
    }

  fd = open(EEPROM_DEV_PATH, O_RDWR);
  if (fd < 0)
    {
      printf("ERROR: Failed to open %s for write: %s\n",
             EEPROM_DEV_PATH, strerror(errno));
      return -errno;
    }

  if (lseek(fd, offset, SEEK_SET) < 0)
    {
      printf("ERROR: Seek failed: %s\n", strerror(errno));
      close(fd);
      return -errno;
    }

  printf("Writing %zu bytes to offset 0x%04lx: \"%s\"...\n",
         len, (unsigned long)offset, text);

  nwritten = write(fd, text, len);
  if (nwritten != (ssize_t)len)
    {
      printf("ERROR: Write failed: returned %zd (expected %zu), errno: %s\n",
             nwritten, len, strerror(errno));
      close(fd);
      return -EIO;
    }

  /* Wait for EEPROM internal write cycle (tWR max 5ms per page) */

  usleep(10000);

  /* Verify written contents */

  verify_buf = (uint8_t *)malloc(len);
  if (!verify_buf)
    {
      close(fd);
      return -ENOMEM;
    }

  if (lseek(fd, offset, SEEK_SET) < 0)
    {
      printf("ERROR: Seek for verify failed: %s\n", strerror(errno));
      close(fd);
      free(verify_buf);
      return -errno;
    }

  nread = read(fd, verify_buf, len);
  close(fd);

  if (nread != (ssize_t)len)
    {
      if (nread < 0)
        {
          printf("ERROR: Readback for verify failed: %s (errno %d)\n",
                 strerror(errno), errno);
        }
      else
        {
          printf("ERROR: Readback for verify failed: returned %zd (expected %zu)\n",
                 nread, len);
        }
      free(verify_buf);
      return -EIO;
    }

  if (memcmp(text, verify_buf, len) == 0)
    {
      printf("SUCCESS: Verified byte-for-byte readback match!\n");
      print_hexdump(offset, verify_buf, len);
      free(verify_buf);
      return OK;
    }
  else
    {
      printf("ERROR: Readback mismatch!\n");
      printf("Expected:\n");
      print_hexdump(offset, (const uint8_t *)text, len);
      printf("Actual:\n");
      print_hexdump(offset, verify_buf, len);
      free(verify_buf);
      return -EFAULT;
    }
}

static int cmd_dump(int argc, char *argv[])
{
  off_t offset = 0;
  size_t lines = 4;

  if (argc > 2)
    {
      offset = (off_t)strtoul(argv[2], NULL, 0);
    }
  if (argc > 3)
    {
      lines = (size_t)strtoul(argv[3], NULL, 0);
    }

  char len_str[32];
  char off_str[32];
  snprintf(off_str, sizeof(off_str), "0x%lx", (unsigned long)offset);
  snprintf(len_str, sizeof(len_str), "%zu", lines * 16);

  char *read_argv[] = { "eeprom_test", "read", off_str, len_str, NULL };
  return cmd_read(4, read_argv);
}

static int cmd_benchmark(int argc, char *argv[])
{
  size_t test_size = 512; /* Default: 4 pages (4 * 128B) */
  uint8_t *wbuf;
  uint8_t *rbuf;
  int fd;
  size_t i;
  struct timespec t0;
  struct timespec t1;
  double write_ms;
  double read_ms;
  ssize_t ret;

  if (argc > 2)
    {
      test_size = (size_t)strtoul(argv[2], NULL, 0);
    }

  if (test_size < 128)
    {
      test_size = 128;
    }
  if (test_size > 4096)
    {
      test_size = 4096;
    }

  wbuf = (uint8_t *)malloc(test_size);
  rbuf = (uint8_t *)malloc(test_size);
  if (!wbuf || !rbuf)
    {
      printf("ERROR: Memory allocation failed for benchmark\n");
      if (wbuf) free(wbuf);
      if (rbuf) free(rbuf);
      return -ENOMEM;
    }

  /* Fill with alternating pattern */

  for (i = 0; i < test_size; i++)
    {
      wbuf[i] = (uint8_t)(i ^ 0xaa);
    }

  printf("Running EEPROM benchmark with %zu bytes (at offset 0x1000)...\n", test_size);

  fd = open(EEPROM_DEV_PATH, O_RDWR);
  if (fd < 0)
    {
      printf("ERROR: Failed to open %s: %s\n", EEPROM_DEV_PATH, strerror(errno));
      free(wbuf);
      free(rbuf);
      return -errno;
    }

  /* 1. Measure Write Time */

  lseek(fd, 0x1000, SEEK_SET);
  clock_gettime(CLOCK_MONOTONIC, &t0);

  ret = write(fd, wbuf, test_size);

  clock_gettime(CLOCK_MONOTONIC, &t1);

  if (ret != (ssize_t)test_size)
    {
      printf("ERROR: Benchmark write failed: ret %zd (%s)\n", ret, strerror(errno));
      close(fd);
      free(wbuf);
      free(rbuf);
      return -EIO;
    }

  write_ms = (t1.tv_sec - t0.tv_sec) * 1000.0 + (t1.tv_nsec - t0.tv_nsec) / 1000000.0;

  /* 2. Measure Read Time */

  lseek(fd, 0x1000, SEEK_SET);
  clock_gettime(CLOCK_MONOTONIC, &t0);

  ret = read(fd, rbuf, test_size);

  clock_gettime(CLOCK_MONOTONIC, &t1);

  if (ret != (ssize_t)test_size)
    {
      printf("ERROR: Benchmark read failed: ret %zd (%s)\n", ret, strerror(errno));
      close(fd);
      free(wbuf);
      free(rbuf);
      return -EIO;
    }

  read_ms = (t1.tv_sec - t0.tv_sec) * 1000.0 + (t1.tv_nsec - t0.tv_nsec) / 1000000.0;
  close(fd);

  /* 3. Check data integrity */

  if (memcmp(wbuf, rbuf, test_size) != 0)
    {
      printf("ERROR: Benchmark data corruption detected!\n");
      free(wbuf);
      free(rbuf);
      return -EFAULT;
    }

  printf("-------------------------------------------------------------------\n");
  printf(" Benchmark Results (%zu bytes across %zu pages):\n",
         test_size, test_size / AT24C512_PAGE_SIZE);
  printf("  Write Time:       %.2f ms (%.2f KB/s, ~%.2f ms/page)\n",
         write_ms, ((double)test_size / 1024.0) / (write_ms / 1000.0),
         write_ms / (test_size / (double)AT24C512_PAGE_SIZE));
  printf("  Read Time:        %.2f ms (%.2f KB/s)\n",
         read_ms, ((double)test_size / 1024.0) / (read_ms / 1000.0));
  printf("  Integrity Check:  PASS (100%% bit exact match)\n");
  printf("-------------------------------------------------------------------\n");

  free(wbuf);
  free(rbuf);
  return OK;
}

static int cmd_verify(void)
{
  int fd;
  ssize_t nw;
  ssize_t nr;
  char readback[64];
  size_t len = strlen(MAGIC_SIGNATURE);

  printf("Writing non-volatile signature to offset %d...\n", MAGIC_OFFSET);

  fd = open(EEPROM_DEV_PATH, O_RDWR);
  if (fd < 0)
    {
      printf("ERROR: Failed to open %s: %s\n", EEPROM_DEV_PATH, strerror(errno));
      return -errno;
    }

  lseek(fd, MAGIC_OFFSET, SEEK_SET);
  nw = write(fd, MAGIC_SIGNATURE, len + 1);
  if (nw != (ssize_t)(len + 1))
    {
      printf("ERROR: Signature write failed: %s\n", strerror(errno));
      close(fd);
      return -EIO;
    }

  usleep(10000);

  lseek(fd, MAGIC_OFFSET, SEEK_SET);
  memset(readback, 0, sizeof(readback));
  nr = read(fd, readback, len + 1);
  close(fd);

  if (nr != (ssize_t)(len + 1) || strcmp(readback, MAGIC_SIGNATURE) != 0)
    {
      printf("ERROR: Signature verification failed! Got: \"%s\"\n", readback);
      return -EFAULT;
    }

  printf("\n===================================================================\n");
  printf(" [OK] Non-volatile test signature successfully written to EEPROM!\n");
  printf("      Signature: \"%s\"\n", readback);
  printf("-------------------------------------------------------------------\n");
  printf(" Next Step (Persistence Test):\n");
  printf("  1. Reboot the board ('reboot' command or power cycle).\n");
  printf("  2. In NSH, run: 'eeprom_test check'\n");
  printf("  3. If verified, the EEPROM is proven non-volatile and fully operational!\n");
  printf("===================================================================\n");

  return OK;
}

static int cmd_check(void)
{
  int fd;
  ssize_t nr;
  char readback[64];
  size_t len = strlen(MAGIC_SIGNATURE);

  printf("Checking non-volatile signature at offset %d...\n", MAGIC_OFFSET);

  fd = open(EEPROM_DEV_PATH, O_RDONLY);
  if (fd < 0)
    {
      printf("ERROR: Failed to open %s: %s\n", EEPROM_DEV_PATH, strerror(errno));
      return -errno;
    }

  lseek(fd, MAGIC_OFFSET, SEEK_SET);
  memset(readback, 0, sizeof(readback));
  nr = read(fd, readback, len + 1);
  close(fd);

  if (nr < 0)
    {
      printf("ERROR: Read signature failed: %s\n", strerror(errno));
      return -errno;
    }

  if (strcmp(readback, MAGIC_SIGNATURE) == 0)
    {
      printf("\n===================================================================\n");
      printf(" [PASS] NON-VOLATILE PERSISTENCE CONFIRMED!\n");
      printf("        Found signature: \"%s\"\n", readback);
      printf("        Data survived power cycle / system reset successfully.\n");
      printf("===================================================================\n");
      return OK;
    }
  else
    {
      printf("\n===================================================================\n");
      printf(" [FAIL] Signature mismatch after reboot.\n");
      printf("        Expected: \"%s\"\n", MAGIC_SIGNATURE);
      printf("        Found:    \"%s\"\n", readback);
      printf("===================================================================\n");
      return -EFAULT;
    }
}

static int cmd_mtd(void)
{
  int fd;
  struct mtd_geometry_s geo;
  int ret = -1;

  printf("Checking MTD storage node: %s\n", MTD_DEV_PATH);

  memset(&geo, 0, sizeof(geo));

  fd = open(MTD_DEV_PATH, O_RDONLY);
  if (fd >= 0)
    {
      ret = ioctl(fd, MTDIOC_GEOMETRY, (unsigned long)((uintptr_t)&geo));
      if (ret < 0)
        {
          struct geometry bgeo;
          memset(&bgeo, 0, sizeof(bgeo));
          ret = ioctl(fd, BIOC_GEOMETRY, (unsigned long)((uintptr_t)&bgeo));
          if (ret >= 0)
            {
              geo.blocksize    = bgeo.geo_sectorsize;
              geo.erasesize    = bgeo.geo_sectorsize;
              geo.neraseblocks = bgeo.geo_nsectors;
              strlcpy(geo.model, bgeo.geo_model, sizeof(geo.model));
            }
        }
      close(fd);
    }
  else
    {
      /* If POSIX character open fails, inspect directly via kernel MTD inode */

      FAR struct inode *node = NULL;
      int r = find_mtddriver(MTD_DEV_PATH, &node);
      if (r >= 0 && node != NULL && node->u.i_mtd != NULL)
        {
          ret = MTD_IOCTL(node->u.i_mtd, MTDIOC_GEOMETRY,
                          (unsigned long)((uintptr_t)&geo));
          close_mtddriver(node);
        }
      else
        {
          printf("ERROR: Cannot open %s: %s (errno %d)\n",
                 MTD_DEV_PATH, strerror(errno), errno);
          return -errno;
        }
    }

  if (ret < 0)
    {
      printf("WARNING: Query geometry returned %d (errno %d: %s)\n",
             ret, errno, strerror(errno));
      printf("Note: Driver is registered at %s for PX4 parameter subsystem.\n",
             MTD_DEV_PATH);
      return OK;
    }

  printf("-------------------------------------------------------------------\n");
  printf(" MTD Geometry for %s:\n", MTD_DEV_PATH);
  printf("  Block Size:       %" PRIu32 " bytes\n", geo.blocksize);
  printf("  Erase Block Size: %" PRIu32 " bytes\n", geo.erasesize);
  printf("  Number of Blocks: %" PRIu32 " blocks\n", geo.neraseblocks);
  printf("  Total Capacity:   %" PRIu32 " bytes (%" PRIu32 " KiB)\n",
         geo.erasesize * geo.neraseblocks,
         (geo.erasesize * geo.neraseblocks) / 1024);
  if (geo.model[0] != '\0')
    {
      printf("  Model:            %s\n", geo.model);
    }
  printf("-------------------------------------------------------------------\n");
  printf(" [OK] MTD parameter storage is ready for PX4 Autopilot.\n");
  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  if (argc < 2)
    {
      print_usage();
      return EXIT_FAILURE;
    }

  const char *cmd = argv[1];

  if (strcmp(cmd, "info") == 0)
    {
      return cmd_info() == OK ? EXIT_SUCCESS : EXIT_FAILURE;
    }
  else if (strcmp(cmd, "diag") == 0)
    {
      return cmd_diag() == OK ? EXIT_SUCCESS : EXIT_FAILURE;
    }
  else if (strcmp(cmd, "scan") == 0)
    {
      return cmd_scan(argc, argv) == OK ? EXIT_SUCCESS : EXIT_FAILURE;
    }
  else if (strcmp(cmd, "probe") == 0)
    {
      return cmd_probe(argc, argv) == OK ? EXIT_SUCCESS : EXIT_FAILURE;
    }
  else if (strcmp(cmd, "read") == 0)
    {
      return cmd_read(argc, argv) == OK ? EXIT_SUCCESS : EXIT_FAILURE;
    }
  else if (strcmp(cmd, "write") == 0)
    {
      return cmd_write(argc, argv) == OK ? EXIT_SUCCESS : EXIT_FAILURE;
    }
  else if (strcmp(cmd, "dump") == 0)
    {
      return cmd_dump(argc, argv) == OK ? EXIT_SUCCESS : EXIT_FAILURE;
    }
  else if (strcmp(cmd, "benchmark") == 0)
    {
      return cmd_benchmark(argc, argv) == OK ? EXIT_SUCCESS : EXIT_FAILURE;
    }
  else if (strcmp(cmd, "verify") == 0)
    {
      return cmd_verify() == OK ? EXIT_SUCCESS : EXIT_FAILURE;
    }
  else if (strcmp(cmd, "check") == 0)
    {
      return cmd_check() == OK ? EXIT_SUCCESS : EXIT_FAILURE;
    }
  else if (strcmp(cmd, "mtd") == 0)
    {
      return cmd_mtd() == OK ? EXIT_SUCCESS : EXIT_FAILURE;
    }
  else
    {
      printf("Unknown command: %s\n\n", cmd);
      print_usage();
      return EXIT_FAILURE;
    }
}
