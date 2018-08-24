#ifndef __TPS65132_I2C_H
#define __TPS65132_I2C_H

/* TPS65132 Registers */
#define TPS65132_VPOS_REG  0x00
#define TPS65132_VNEG_REG  0x01

int tps65132_write_bytes(unsigned char addr, unsigned char value);

#endif