
obj-$(CONFIG_MTK_FINGERPRINT) +=  fp_irq.o

ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),y)
	subdir-ccflags-y += -I$(srctree)/drivers/spi/trustzone/spi/Tlspi/inc
endif

ifeq ($(CONFIG_TRUSTONIC_TEE_SUPPORT),y)
	include $(srctree)/drivers/misc/mediatek/gud/Makefile.include
endif



MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
subdir-ccflags-y += -Werror
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/mach
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
subdir-ccflags-y += -I$(srctree)/drivers/spi/mediatek/$(MTK_PLATFORM)
#obj-y := fpc1020_main.o fpc1020_common.o fpc1020_input.o fpc1020_capture.o

ifeq ($(CONFIG_ARCH_MTK_PROJECT), "htc_alpine")
  ccflags-y+= -DHTC_ALE
endif
