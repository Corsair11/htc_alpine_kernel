 

#ifndef VL6180_I2C_H_
#define VL6180_I2C_H_

#include "vl6180x_platform.h"


#define I2C_BUFFER_CONFIG 1

int  VL6180x_I2CWrite(VL6180xDev_t dev, uint8_t  *buff, uint8_t len);

int VL6180x_I2CRead(VL6180xDev_t dev, uint8_t *buff, uint8_t len);


#define VL6180x_I2C_USER_VAR

void VL6180x_GetI2CAccess(VL6180xDev_t dev);

#define VL6180x_GetI2CAccess(dev) (void)0 

void VL6180x_DoneI2CAccess(VL6180xDev_t dev);

#define VL6180x_DoneI2CAcces(dev) (void)0  

uint8_t *VL6180x_GetI2cBuffer(VL6180xDev_t dev, int n_byte);
#if I2C_BUFFER_CONFIG == 2
#error 
#endif





#endif 
