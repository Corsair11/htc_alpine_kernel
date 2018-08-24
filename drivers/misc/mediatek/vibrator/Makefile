ifeq ($(CONFIG_MTK_VIBRATOR),y)
obj-$(CONFIG_MTK_VIBRATOR) := vibrator_drv.o
obj-$(CONFIG_INPUT_DRV260X_HAPTICS) += vib_drv2604.o
obj-$(CONFIG_VIB_TRIGGERS)      += vib-triggers.o
obj-y += $(subst ",,$(CONFIG_MTK_PLATFORM))/

ccflags-y += -I$(srctree)/drivers/misc/mediatek/vibrator/$(MTK_PLATFORM)
endif

ifeq ($(CONFIG_VIB_DRV2625),y)
	obj-$(CONFIG_VIB_DRV2625) += drv2625/
	obj-$(CONFIG_VIB_TRIGGERS)      += vib-triggers.o
endif


ifeq ($(CONFIG_VIB_DRV2624),y)
obj-$(CONFIG_VIB_DRV2624) += drv2624/
obj-$(CONFIG_VIB_TRIGGERS)      += vib-triggers.o
endif
