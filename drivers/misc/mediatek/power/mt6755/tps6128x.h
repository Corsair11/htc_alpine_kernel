#ifndef _tps6128x_SW_H_
#define _tps6128x_SW_H_

#define DEVICE_NAME "TPS6128x"

#define TPS_ERR(fmt, arg...) \
	printk(KERN_ERR "["DEVICE_NAME"] %s: " fmt, __func__, ## arg)
#define TPS_LOG(fmt, arg...) \
	printk(KERN_INFO "["DEVICE_NAME"] %s: " fmt, __func__, ## arg)
#define TPS_DBG(fmt, arg...) \
	printk(KERN_DEBUG"["DEVICE_NAME"] %s: " fmt, __func__, ## arg)

/*
 * ----------------------------------------------------------------------------
 * Registers, all 8 bits
 * ----------------------------------------------------------------------------
 */

/* Register definitions */
#define TPS6128x_CHIPVERSION  0x0
#define TPS6128x_CONFIG       0x1
#define TPS6128x_VOUTFLOORSET 0x2
#define TPS6128x_VOUTROOFSET  0x3
#define TPS6128x_ILIMSET      0x4
#define TPS6128x_STATUS       0x5

/* Chip vesion */
#define TPS6128x_CHIPVERSION_ADDR                  TPS6128x_CHIPVERSION
#define TPS6128x_CHIPVERSION_MASK                  0xFF
#define TPS6128x_CHIPVERSION_SHIFT                 0x0

/* Config */
#define TPS6128x_CONFIG_MODE_CTRL_ADDR             TPS6128x_CONFIG
#define TPS6128x_CONFIG_MODE_CTRL_MASK             0x3
#define TPS6128x_CONFIG_MODE_CTRL_SHIFT            0
#define TPS6128x_CONFIG_CONFIG_SSFM_ADDR           TPS6128x_CONFIG
#define TPS6128x_CONFIG_SSFM_MASK                  0x1
#define TPS6128x_CONFIG_SSFM_SHIFT                 2
#define TPS6128x_CONFIG_CONFIG_GPIOCFG_ADDR        TPS6128x_CONFIG
#define TPS6128x_CONFIG_GPIOCFG_MASK               0x1
#define TPS6128x_CONFIG_GPIOCFG_SHIFT              3
#define TPS6128x_CONFIG_ENABLE_ADDR                TPS6128x_CONFIG
#define TPS6128x_CONFIG_ENABLE_MASK                0x3
#define TPS6128x_CONFIG_ENABLE_SHIFT               5
#define TPS6128x_CONFIG_RESET_ADDR                 TPS6128x_CONFIG
#define TPS6128x_CONFIG_RESET_MASK                 0x1
#define TPS6128x_CONFIG_RESET_SHIFT                7

/* Output voltage */
#define TPS6128x_VOUTFLOORSET_VOUTFLOOR_TH_ADDR    TPS6128x_VOUTFLOORSET
#define TPS6128x_VOUTFLOORSET_VOUTFLOOR_TH_MASK    0x1F
#define TPS6128x_VOUTFLOORSET_VOUTFLOOR_TH_SHIFT   0

/* Output voltage */
#define TPS6128x_VOUTROOFSET_VOUTROOF_TH_ADDR      TPS6128x_VOUTROOFSET
#define TPS6128x_VOUTROOFSET_VOUTROOF_TH_MASK      0x1F
#define TPS6128x_VOUTROOFSET_VOUTROOF_TH_SHIFT     0

/* Current Limit */
#define TPS6128x_ILIMSET_ILIM_ADDR                 TPS6128x_ILIMSET
#define TPS6128x_ILIMSET_ILIM_MASK                 0xF
#define TPS6128x_ILIMSET_ILIM_SHIFT                0
#define TPS6128x_ILIMSET_SOFT_START_ADDR           TPS6128x_ILIMSET
#define TPS6128x_ILIMSET_SOFT_START_MASK           0x1
#define TPS6128x_ILIMSET_SOFT_START_SHIFT          4
#define TPS6128x_ILIMSET_ILIM_OFF_ADDR             TPS6128x_ILIMSET
#define TPS6128x_ILIMSET_ILIM_OFF_MASK             0x1
#define TPS6128x_ILIMSET_ILIM_OFF_SHIFT            5

uint32_t tps6128x_read_interface (uint8_t addr, uint8_t *val, uint8_t mask, uint8_t shift);
uint32_t tps6128x_write_interface (uint8_t addr, uint8_t val, uint8_t mask, uint8_t shift);

#endif // _tps6128x_SW_H_

