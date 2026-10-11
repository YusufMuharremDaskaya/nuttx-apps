############################################################################
# apps/system/eeprom_test/Makefile
#
# SPDX-License-Identifier: Apache-2.0
#
############################################################################

include $(APPDIR)/Make.defs

# AM67A AT24C512C EEPROM Test Application

PROGNAME  = $(CONFIG_SYSTEM_EEPROM_TEST_PROGNAME)
PRIORITY  = $(CONFIG_SYSTEM_EEPROM_TEST_PRIORITY)
STACKSIZE = $(CONFIG_SYSTEM_EEPROM_TEST_STACKSIZE)
MODULE    = $(CONFIG_SYSTEM_EEPROM_TEST)

# Sources

MAINSRC = eeprom_test_main.c

include $(APPDIR)/Application.mk
