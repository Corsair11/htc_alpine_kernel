#ifndef _LC898123AF_H
#define _LC898123AF_H

#include <linux/ioctl.h>

#define LC898123AF_MAGIC 'A'


typedef struct {
u32 u4CurrentPosition;
u32 u4MacroPosition;
u32 u4InfPosition;
bool          bIsMotorMoving;
bool          bIsMotorOpen;
bool          bIsSupportSR;
} stLC898123AF_MotorInfo;

#define LC898123AFIOC_G_MOTORINFO _IOR(LC898123AF_MAGIC,0,stLC898123AF_MotorInfo)

#define LC898123AFIOC_T_MOVETO _IOW(LC898123AF_MAGIC,1, u32)

#define LC898123AFIOC_T_SETINFPOS _IOW(LC898123AF_MAGIC,2, u32)

#define LC898123AFIOC_T_SETMACROPOS _IOW(LC898123AF_MAGIC,3, u32)

#define LC898123AFIOC_T_OIS_ENABLE _IOW(LC898123AF_MAGIC,4, u32)
#define LC898123AFIOC_T_SET_MODE    _IOW(LC898123AF_MAGIC,5, u32)
#define LC898123AFIOC_T_OIS_GYRO_CALI _IOW(LC898123AF_MAGIC,6, u32)

#ifdef HiauRml_CAMERA
extern void kdSetI2CSpeed(u16 i2cSpeed);
#endif

#else
#endif
