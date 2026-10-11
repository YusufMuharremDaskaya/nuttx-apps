/****************************************************************************
 * apps/system/uart_test/uart_test_main.c
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
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <sys/stat.h>
#include <poll.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define UART_TEST_BUF_SIZE 512

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct uart_info_s
{
  const char *devpath;
  const char *hw_name;
  uint32_t    base_addr;
  int         irq;
  const char *pins;
  const char *notes;
};

/****************************************************************************
 * Private Data
 ****************************************************************************/

static const struct uart_info_s g_uart_table[] =
{
  {
    .devpath   = "/dev/ttyS0",
    .hw_name   = "UART-MAIN1",
    .base_addr = 0x02810000,
    .irq       = 211,
    .pins      = "Hat Pin 8 (TX) / Hat Pin 10 (RX)",
    .notes     = "Primary 40-pin Hat UART (Linux ttyS3 / PX4 TTL Console)"
  },
  {
    .devpath   = "/dev/ttyS1",
    .hw_name   = "UART-MAIN6",
    .base_addr = 0x02860000,
    .irq       = 216,
    .pins      = "Hat Pin 11 (TX) / Hat Pin 7 (RX)",
    .notes     = "Secondary Hat UART (Linux ttyS6 / BT shared)"
  },
  {
    .devpath   = "/dev/ttyS2",
    .hw_name   = "UART-WKUP0",
    .base_addr = 0x2B300000,
    .irq       = 219,
    .pins      = "Hat Pin 18 (TX) / Hat Pin 26 (RX)",
    .notes     = "Wakeup domain Hat UART (Linux ttyS0)"
  },
  {
    .devpath   = "/dev/ttyS3",
    .hw_name   = "MCU_UART0",
    .base_addr = 0x04A00000,
    .irq       = 217,
    .pins      = "MCU Domain Dedicated Pins",
    .notes     = "Dedicated Cortex-R5F MCU Domain UART"
  },
};

#define NUM_UARTS (sizeof(g_uart_table) / sizeof(g_uart_table[0]))

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static speed_t baud_to_speed(uint32_t baud)
{
  switch (baud)
    {
      case 9600:    return B9600;
      case 19200:   return B19200;
      case 38400:   return B38400;
      case 57600:   return B57600;
      case 115200:  return B115200;
      case 230400:  return B230400;
#ifdef B460800
      case 460800:  return B460800;
#endif
#ifdef B921600
      case 921600:  return B921600;
#endif
      default:      return B115200;
    }
}

static uint32_t speed_to_baud(speed_t speed)
{
  switch (speed)
    {
      case B9600:   return 9600;
      case B19200:  return 19200;
      case B38400:  return 38400;
      case B57600:  return 57600;
      case B115200: return 115200;
      case B230400: return 230400;
#ifdef B460800
      case B460800: return 460800;
#endif
#ifdef B921600
      case B921600: return 921600;
#endif
      default:      return (uint32_t)speed;
    }
}

static void show_usage(const char *progname)
{
  printf("\nUsage: %s <command> [arguments]\n\n", progname);
  printf("Commands:\n");
  printf("  list                          Scan and list all UART interfaces\n");
  printf("  write <dev> <text>            Transmit text message over UART\n");
  printf("  read  <dev> [timeout_sec]     Listen for incoming UART data\n");
  printf("  loop  <dev> [interval_ms]     Transmit continuous periodic test frames\n");
  printf("  baud  <dev> <rate>            Configure port baud rate (e.g. 115200)\n");
  printf("  loopback <tx_dev> <rx_dev> [internal] Run loopback test (pin jumper or internal)\n\n");
  printf("Examples:\n");
  printf("  %s list\n", progname);
  printf("  %s write /dev/ttyS0 \"Teknofest Gemstone Test\"\n", progname);
  printf("  %s read /dev/ttyS0 5\n", progname);
  printf("  %s loop /dev/ttyS0 1000\n", progname);
  printf("  %s baud /dev/ttyS0 115200\n", progname);
  printf("  %s loopback /dev/ttyS0 /dev/ttyS0\n", progname);
  printf("  %s loopback /dev/ttyS0 /dev/ttyS0 internal\n\n", progname);
}

/****************************************************************************
 * Command: list
 ****************************************************************************/

static int cmd_list(void)
{
  printf("\n===================================================================\n");
  printf(" T3-GEM-O1 Multi-Port UART Diagnostics (Cortex-R5F NuttX)\n");
  printf("===================================================================\n");

  size_t i;
  for (i = 0; i < NUM_UARTS; i++)
    {
      const struct uart_info_s *u = &g_uart_table[i];
      struct stat st;
      bool present = (stat(u->devpath, &st) == 0);

      printf("\n[%s] %s\n", u->devpath, u->hw_name);
      printf("  Physical Pins : %s\n", u->pins);
      printf("  MMR Base      : 0x%08" PRIx32 " (IRQ %d)\n", u->base_addr, u->irq);
      printf("  Description   : %s\n", u->notes);

      if (present)
        {
          int fd = open(u->devpath, O_RDWR | O_NONBLOCK);
          if (fd >= 0)
            {
              struct termios tio;
              if (tcgetattr(fd, &tio) == 0)
                {
                  uint32_t baud = speed_to_baud(cfgetospeed(&tio));
                  printf("  Status        : [PRESENT & READY] (Current Baud: %" PRIu32 ")\n",
                         baud);
                }
              else
                {
                  printf("  Status        : [PRESENT & OPENED]\n");
                }
              close(fd);
            }
          else
            {
              printf("  Status        : [PRESENT, Busy/Error: errno=%d (%s)]\n",
                     errno, strerror(errno));
            }
        }
      else
        {
          printf("  Status        : [NOT PRESENT] (Device node not registered)\n");
        }
    }

  /* Also check RPMsg virtual NSH console */

  struct stat st;
  printf("\n[/dev/ttyRPMSG_NSH] RPMsg Virtual UART Console\n");
  printf("  Status        : %s\n",
         (stat("/dev/ttyRPMSG_NSH", &st) == 0) ? "[ACTIVE CONSOLE]" : "[NOT MOUNTED]");

  printf("\n===================================================================\n\n");
  return OK;
}

/****************************************************************************
 * Command: write
 ****************************************************************************/

static int cmd_write(const char *devpath, const char *msg)
{
  int fd = open(devpath, O_WRONLY);
  if (fd < 0)
    {
      printf("ERROR: Cannot open %s for writing: %s (errno %d)\n",
             devpath, strerror(errno), errno);
      return ERROR;
    }

  size_t len = strlen(msg);
  ssize_t nwritten = write(fd, msg, len);
  if (nwritten < 0)
    {
      printf("ERROR: Failed to write to %s: %s (errno %d)\n",
             devpath, strerror(errno), errno);
      close(fd);
      return ERROR;
    }

  /* Also send \r\n for line termination */

  write(fd, "\r\n", 2);

  /* Wait for transmit holding/shift register to empty */

  tcdrain(fd);
  close(fd);

  printf("SUCCESS: Sent %zd bytes to %s: \"%s\"\n", nwritten + 2, devpath, msg);
  return OK;
}

/****************************************************************************
 * Command: read
 ****************************************************************************/

static int cmd_read(const char *devpath, int timeout_sec)
{
  int fd = open(devpath, O_RDONLY | O_NONBLOCK);
  if (fd < 0)
    {
      printf("ERROR: Cannot open %s for reading: %s (errno %d)\n",
             devpath, strerror(errno), errno);
      return ERROR;
    }

  printf("Listening on %s (Timeout: %d seconds)...\n", devpath, timeout_sec);

  struct pollfd pfd;
  pfd.fd = fd;
  pfd.events = POLLIN;

  int timeout_ms = timeout_sec * 1000;
  int ret = poll(&pfd, 1, timeout_ms);

  if (ret < 0)
    {
      printf("ERROR: poll failed: %s (errno %d)\n", strerror(errno), errno);
      close(fd);
      return ERROR;
    }
  else if (ret == 0)
    {
      printf("TIMEOUT: No data received within %d seconds.\n", timeout_sec);
      close(fd);
      return OK;
    }

  uint8_t buffer[UART_TEST_BUF_SIZE];
  ssize_t nread = read(fd, buffer, sizeof(buffer) - 1);
  close(fd);

  if (nread <= 0)
    {
      printf("No data read (errno %d: %s)\n", errno, strerror(errno));
      return ERROR;
    }

  buffer[nread] = '\0';
  printf("\n[SUCCESS] Received %zd bytes:\n", nread);
  printf("  Raw ASCII : %s\n", (char *)buffer);
  printf("  Hex Dump  : ");
  ssize_t i;
  for (i = 0; i < nread; i++)
    {
      printf("%02X ", buffer[i]);
    }
  printf("\n\n");

  return OK;
}

/****************************************************************************
 * Command: loop (Continuous transmission for oscilloscope/TTL test)
 ****************************************************************************/

static int cmd_loop(const char *devpath, int interval_ms)
{
  int fd = open(devpath, O_WRONLY);
  if (fd < 0)
    {
      printf("ERROR: Cannot open %s: %s (errno %d)\n",
             devpath, strerror(errno), errno);
      return ERROR;
    }

  printf("Starting periodic loop test on %s every %d ms (Press Ctrl+C to stop)...\n",
         devpath, interval_ms);

  uint32_t seq = 1;
  char msg[128];

  while (1)
    {
      snprintf(msg, sizeof(msg), "Teknofest Gemstone Frame #%" PRIu32 "\r\n", seq++);
      ssize_t ret = write(fd, msg, strlen(msg));
      if (ret < 0)
        {
          printf("ERROR during write: %s (errno %d)\n", strerror(errno), errno);
          break;
        }

      tcdrain(fd);
      printf("-> Transmitted Frame #%" PRIu32 "\n", seq - 1);
      usleep(interval_ms * 1000);
    }

  close(fd);
  return OK;
}

/****************************************************************************
 * Command: baud
 ****************************************************************************/

static int cmd_baud(const char *devpath, uint32_t baud)
{
  int fd = open(devpath, O_RDWR);
  if (fd < 0)
    {
      printf("ERROR: Cannot open %s: %s (errno %d)\n",
             devpath, strerror(errno), errno);
      return ERROR;
    }

  struct termios tio;
  if (tcgetattr(fd, &tio) < 0)
    {
      printf("ERROR: tcgetattr failed: %s (errno %d)\n",
             strerror(errno), errno);
      close(fd);
      return ERROR;
    }

  speed_t speed = baud_to_speed(baud);
  cfsetspeed(&tio, speed);

  if (tcsetattr(fd, TCSANOW, &tio) < 0)
    {
      printf("ERROR: tcsetattr failed: %s (errno %d)\n",
             strerror(errno), errno);
      close(fd);
      return ERROR;
    }

  close(fd);
  printf("SUCCESS: Configured %s to %" PRIu32 " baud\n", devpath, baud);
  return OK;
}

/****************************************************************************
 * Command: loopback
 ****************************************************************************/

static int cmd_loopback(const char *tx_dev, const char *rx_dev, bool internal_loop)
{
  if (internal_loop)
    {
      printf("Testing INTERNAL silicon loopback (MCR bit 4) on %s...\n", tx_dev);
    }
  else
    {
      printf("Testing loopback: %s (TX) -> %s (RX)...\n", tx_dev, rx_dev);
    }

  bool same_dev = (strcmp(tx_dev, rx_dev) == 0);
  int fd_tx = -1;
  int fd_rx = -1;
  uintptr_t base_addr = 0;
  volatile uint32_t *mcr_reg = NULL;
  uint32_t orig_mcr = 0;

  if (same_dev)
    {
      fd_tx = fd_rx = open(tx_dev, O_RDWR | O_NONBLOCK);
      if (fd_tx < 0)
        {
          printf("ERROR: Cannot open %s: %s (errno %d)\n",
                 tx_dev, strerror(errno), errno);
          return ERROR;
        }
    }
  else
    {
      fd_tx = open(tx_dev, O_WRONLY | O_NONBLOCK);
      if (fd_tx < 0)
        {
          printf("ERROR: Cannot open TX device %s: %s (errno %d)\n",
                 tx_dev, strerror(errno), errno);
          return ERROR;
        }

      fd_rx = open(rx_dev, O_RDONLY | O_NONBLOCK);
      if (fd_rx < 0)
        {
          printf("ERROR: Cannot open RX device %s: %s (errno %d)\n",
                 rx_dev, strerror(errno), errno);
          close(fd_tx);
          return ERROR;
        }
    }

  if (internal_loop)
    {
      for (size_t i = 0; i < NUM_UARTS; i++)
        {
          if (strcmp(g_uart_table[i].devpath, tx_dev) == 0)
            {
              base_addr = g_uart_table[i].base_addr;
              break;
            }
        }

      if (base_addr != 0)
        {
          mcr_reg = (volatile uint32_t *)(base_addr + 0x10);
          orig_mcr = *mcr_reg;
          *mcr_reg = orig_mcr | 0x10; /* Set LOOP bit */
        }
    }

  /* Flush stale RX data */

  uint8_t dummy[64];
  while (read(fd_rx, dummy, sizeof(dummy)) > 0);

  const char *test_pattern = "GEMSTONE-LOOPBACK-TEST-OK-123456789";
  size_t len = strlen(test_pattern);

  ssize_t nwritten = write(fd_tx, test_pattern, len);
  if (nwritten < 0)
    {
      printf("ERROR: Failed to write to %s: %s (errno %d)\n",
             tx_dev, strerror(errno), errno);
      if (mcr_reg != NULL)
        {
          *mcr_reg = orig_mcr;
        }
      close(fd_tx);
      if (!same_dev)
        {
          close(fd_rx);
        }
      return ERROR;
    }

  tcdrain(fd_tx);

  /* Poll with 1-second timeout */

  struct pollfd pfd;
  pfd.fd = fd_rx;
  pfd.events = POLLIN;

  int pret = poll(&pfd, 1, 1000);
  if (pret <= 0)
    {
      printf("FAIL: No data received on %s (poll=%d, errno=%d: %s)\n",
             rx_dev, pret, errno, strerror(errno));
      if (mcr_reg != NULL)
        {
          *mcr_reg = orig_mcr;
        }
      close(fd_tx);
      if (!same_dev)
        {
          close(fd_rx);
        }
      return ERROR;
    }

  uint8_t rx_buf[128];
  ssize_t total_read = 0;
  while (total_read < (ssize_t)len)
    {
      ssize_t n = read(fd_rx, rx_buf + total_read, sizeof(rx_buf) - 1 - total_read);
      if (n > 0)
        {
          total_read += n;
        }
      else
        {
          if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            {
              usleep(5000);
              continue;
            }
          break;
        }
    }

  if (mcr_reg != NULL)
    {
      *mcr_reg = orig_mcr;
    }

  close(fd_tx);
  if (!same_dev)
    {
      close(fd_rx);
    }

  if (total_read <= 0)
    {
      printf("FAIL: No data read from %s\n", rx_dev);
      return ERROR;
    }

  rx_buf[total_read] = '\0';
  if (strcmp((char *)rx_buf, test_pattern) == 0)
    {
      printf("PASS: Loopback verified! Received exact match: \"%s\"\n", (char *)rx_buf);
      return OK;
    }
  else
    {
      printf("FAIL: Loopback mismatch! Sent: \"%s\", Received: \"%s\"\n",
             test_pattern, (char *)rx_buf);
      return ERROR;
    }
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  if (argc < 2)
    {
      show_usage(argv[0]);
      return ERROR;
    }

  const char *cmd = argv[1];

  if (strcmp(cmd, "list") == 0 || strcmp(cmd, "info") == 0)
    {
      return cmd_list();
    }
  else if (strcmp(cmd, "write") == 0)
    {
      if (argc < 4)
        {
          printf("ERROR: write requires <dev> and <text>\n");
          return ERROR;
        }
      return cmd_write(argv[2], argv[3]);
    }
  else if (strcmp(cmd, "read") == 0)
    {
      if (argc < 3)
        {
          printf("ERROR: read requires <dev>\n");
          return ERROR;
        }
      int timeout = (argc >= 4) ? atoi(argv[3]) : 5;
      return cmd_read(argv[2], timeout);
    }
  else if (strcmp(cmd, "loop") == 0)
    {
      if (argc < 3)
        {
          printf("ERROR: loop requires <dev>\n");
          return ERROR;
        }
      int interval = (argc >= 4) ? atoi(argv[3]) : 1000;
      return cmd_loop(argv[2], interval);
    }
  else if (strcmp(cmd, "baud") == 0)
    {
      if (argc < 4)
        {
          printf("ERROR: baud requires <dev> and <rate>\n");
          return ERROR;
        }
      return cmd_baud(argv[2], (uint32_t)atoi(argv[3]));
    }
  else if (strcmp(cmd, "loopback") == 0)
    {
      if (argc < 4)
        {
          printf("ERROR: loopback requires <tx_dev> and <rx_dev> [internal]\n");
          return ERROR;
        }
      bool internal = (argc >= 5 && strcmp(argv[4], "internal") == 0);
      return cmd_loopback(argv[2], argv[3], internal);
    }
  else
    {
      printf("Unknown command: %s\n", cmd);
      show_usage(argv[0]);
      return ERROR;
    }

  return OK;
}
