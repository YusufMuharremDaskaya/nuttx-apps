############################################################################
# apps/system/pwm_test/Makefile
#
# SPDX-License-Identifier: Apache-2.0
#
############################################################################

include $(APPDIR)/Make.defs

# AM67A ePWM Motor Test Application

PROGNAME  = $(CONFIG_SYSTEM_PWM_TEST_PROGNAME)
PRIORITY  = $(CONFIG_SYSTEM_PWM_TEST_PRIORITY)
STACKSIZE = $(CONFIG_SYSTEM_PWM_TEST_STACKSIZE)
MODULE    = $(CONFIG_SYSTEM_PWM_TEST)

# Sources

MAINSRC = pwm_test_main.c

include $(APPDIR)/Application.mk
