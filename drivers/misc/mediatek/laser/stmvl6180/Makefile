#
# Makefile for misc devices that really don't fit anywhere else.
#
include $(srctree)/drivers/misc/mediatek/Makefile.custom

ccflags-y += -I$(MTK_CUSTOM_PATH)/laser/stmvl6180/inc

obj-y += stmvl6180.o vl6180x_api.o vl6180x_i2c.o vl6180x_port_i2c.o
