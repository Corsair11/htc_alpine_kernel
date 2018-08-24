include $(srctree)/drivers/misc/mediatek/Makefile.custom

# Linux driver folder

ccflags-y += -I$(srctree)/drivers/base
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/ft8716/include/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/ft8716/include/$(CONFIG_FT8716_FIRMWARE)/

#ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/
#ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
#ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
#ccflags-y += -I$(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/$(ARCH_MTK_PROJECT)/include/
#ccflags-y += -I$(srctree)/drivers/misc/mediatek/mach/$(MTK_PLATFORM)/$(ARCH_MTK_PROJECT)/dct/dct/

# ccflags-y += -I$(srctree)/drivers/misc/mediatek/lcm/ft8716_tianma_1080p_vdo/


obj-y	+=  focaltech_esd_protection.o
obj-y	+=  focaltech_ctl.o
obj-y	+=  focaltech_ex_fun.o
obj-y	+=  focaltech_flash.o
obj-y	+=  focaltech_Gesture.o
obj-y	+=  focaltech_core.o
