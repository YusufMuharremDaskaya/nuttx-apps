############################################################################
# apps/system/uart_test/Makefile
#
# SPDX-License-Identifier: Apache-2.0
#
############################################################################

include $(APPDIR)/Make.defs

# T3-GEM-O1 Multi-Port UART Test Application

PROGNAME  = $(CONFIG_SYSTEM_UART_TEST_PROGNAME)
PRIORITY  = $(CONFIG_SYSTEM_UART_TEST_PRIORITY)
STACKSIZE = $(CONFIG_SYSTEM_UART_TEST_STACKSIZE)
MODULE    = $(CONFIG_SYSTEM_UART_TEST)

# Sources

MAINSRC = uart_test_main.c

include $(APPDIR)/Application.mk
