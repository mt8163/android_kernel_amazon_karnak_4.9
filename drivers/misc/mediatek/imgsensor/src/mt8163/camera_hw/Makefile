#
# Copyright (C) 2015 MediaTek Inc.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#

#
# Makefile for misc devices that really don't fit anywhere else.
#

#include $(srctree)/drivers/misc/mediatek/Makefile

MTK_CUSTOM_PATH  := $(srctree)/drivers/misc/mediatek

MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
ARCH_MTK_PROJECT := $(subst ",,$(CONFIG_ARCH_MTK_PROJECT))

ccflags-y += -I$(MTK_CUSTOM_PATH)/imgsensor/inc \

MTK_PLATFORM := $(strip $(MTK_PLATFORM))
# remove the _l318 postfix, get the pure project name
ARCH_MTK_PROJECT := $(subst _l318,,$(ARCH_MTK_PROJECT))
# strip
ARCH_MTK_PROJECT := $(strip $(ARCH_MTK_PROJECT))

ifeq ($(wildcard $(MTK_CUSTOM_PATH)/imgsensor/src/$(MTK_PLATFORM)/camera_hw/camera_project/$(ARCH_MTK_PROJECT)),)
obj-y				+= kd_camera_hw.o
else
obj-y 				+= camera_project/$(ARCH_MTK_PROJECT)/
endif
