
#include "vl6180x_i2c.h"
#include <linux/i2c.h>

#if 1

#define I2C_M_WR			0x00
static struct i2c_client *pclient=NULL;

void i2c_setclient(struct i2c_client *client)
{
	pclient = client;

}
struct i2c_client* i2c_getclient(void)
{
	return pclient;
}

int VL6180x_I2CWrite(VL6180xDev_t dev, uint8_t *buff, uint8_t len)
{
	struct i2c_msg msg[1];
	int err=0;

	msg[0].addr = pclient->addr;
	msg[0].flags = I2C_M_WR;
	msg[0].buf= buff;
	msg[0].len=len;

	err = i2c_transfer(pclient->adapter,msg,1); 
	if(err != 1)
	{
		pr_err("%s: i2c_transfer err:%d, addr:0x%x, reg:0x%x\n", __func__, err, pclient->addr, 
																				(buff[0]<<8|buff[1]));
		return -1;
	}
    return 0;
}


int VL6180x_I2CRead(VL6180xDev_t dev, uint8_t *buff, uint8_t len)
{
 	struct i2c_msg msg[1];
	int err=0;

	msg[0].addr = pclient->addr;
	msg[0].flags = I2C_M_RD|pclient->flags;
	msg[0].buf= buff;
	msg[0].len=len;

	err = i2c_transfer(pclient->adapter,&msg[0],1); 
	if(err != 1)
	{
		pr_err("%s: Read i2c_transfer err:%d, addr:0x%x\n", __func__, err, pclient->addr);
		return -1;
	}
    return 0;
}

#endif
