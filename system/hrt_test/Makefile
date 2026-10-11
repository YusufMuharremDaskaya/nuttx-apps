############################################################################
# apps/system/hrt_test/Makefile
#
# SPDX-License-Identifier: Apache-2.0
#
############################################################################

include $(APPDIR)/Make.defs

# High Resolution Timer (HRT) Test Application

PROGNAME  = $(CONFIG_SYSTEM_HRT_TEST_PROGNAME)
PRIORITY  = $(CONFIG_SYSTEM_HRT_TEST_PRIORITY)
STACKSIZE = $(CONFIG_SYSTEM_HRT_TEST_STACKSIZE)
MODULE    = $(CONFIG_SYSTEM_HRT_TEST)

# Sources

MAINSRC = hrt_test_main.c

include $(APPDIR)/Application.mk
