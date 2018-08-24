/*
 * Copyright (C) 2010 NXP Semiconductors
 */

#define PN548_I2C_NAME "pn548"
 
#define NFC_DEBUG 0

#define PN548_MAGIC	0xE9
#define PN548_SET_PWR	_IOW(PN548_MAGIC, 0x01, unsigned int)
#define IO_WAKE_LOCK_TIMEOUT (2*HZ)

struct pn548_i2c_platform_data {
	void (*gpio_init) (void);
	unsigned long irq_gpio;
	uint32_t irq_gpio_flags;
	unsigned long ven_gpio;
	uint32_t ven_gpio_flags;
	unsigned long firm_gpio;
	uint32_t firm_gpio_flags;
	unsigned int ven_isinvert;
	u32 gpio_nfc_pwd;
};
