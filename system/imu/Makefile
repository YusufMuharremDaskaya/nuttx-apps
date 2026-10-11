############################################################################
# apps/system/imu/Makefile
#
# SPDX-License-Identifier: Apache-2.0
#
############################################################################

include $(APPDIR)/Make.defs

# ICM-20948 IMU Test Application

PROGNAME  = $(CONFIG_SYSTEM_IMU_PROGNAME)
PRIORITY  = $(CONFIG_SYSTEM_IMU_PRIORITY)
STACKSIZE = $(CONFIG_SYSTEM_IMU_STACKSIZE)
MODULE    = $(CONFIG_SYSTEM_IMU)

# Sources

MAINSRC = imu_main.c

include $(APPDIR)/Application.mk
