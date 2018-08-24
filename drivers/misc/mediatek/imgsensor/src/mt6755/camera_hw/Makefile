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

obj-y				+= kd_camera_hw.o

# HTC_START, Add flag for ALPINE camera
ifeq ($(CONFIG_ARCH_MTK_PROJECT),"htc_alpine")
ccflags-y += -DALPINE_CAMERA=1
endif
# HTC_END
