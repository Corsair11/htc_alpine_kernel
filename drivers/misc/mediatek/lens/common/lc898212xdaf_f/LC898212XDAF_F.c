
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/fs.h>

#include "lens_info.h"
#include "LC898212XDAF_F.h"

#define AF_DRVNAME "LC898212XDAF_F_DRV"
#define AF_I2C_SLAVE_ADDR         0xE4
#define EEPROM_I2C_SLAVE_ADDR     0xA0

#define I2C_SLAVE_ADDRESS AF_I2C_SLAVE_ADDR

#define AF_DEBUG
#ifdef AF_DEBUG
#else
#define LOG_INF(format, args...)
#endif

#define LOG_INF printk

extern int t4ka7_vcm_driver_vendor_flag ;                                                                                                                       
extern char t4ka7_otp_data[23];

static struct i2c_client *g_pstAF_I2Cclient;
static int *g_pAF_Opened;
static spinlock_t *g_pAF_SpinLock;

static unsigned long g_u4AF_INF;
static unsigned long g_u4AF_MACRO = 1023;
static unsigned long g_u4CurrPosition;

#define Min_Pos		0
#define Max_Pos		1023

static int g_sr = 3;


int s4AF_ReadReg_LC898212XDAF_F(u8 *a_pSendData, u16 a_sizeSendData, u8 *a_pRecvData,
			  u16 a_sizeRecvData, u16 i2cId)
{
	int i4RetValue = 0;

	g_pstAF_I2Cclient->addr = i2cId >> 1;

	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, a_pSendData, a_sizeSendData);

	if (i4RetValue != a_sizeSendData) {
		LOG_INF("I2C send failed!!, Addr = 0x%x\n", a_pSendData[0]);
		return -1;
	}

	i4RetValue = i2c_master_recv(g_pstAF_I2Cclient, (u8 *) a_pRecvData, a_sizeRecvData);

	if (i4RetValue != a_sizeRecvData) {
		LOG_INF("I2C read failed!!\n");
		return -1;
	}

	return 0;
}

int s4AF_WriteReg_LC898212XDAF_F(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId)
{
	int i4RetValue = 0;

	g_pstAF_I2Cclient->addr = i2cId >> 1;

	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, a_pSendData, a_sizeSendData);

	if (i4RetValue < 0) {
		LOG_INF("I2C send failed!!, Addr = 0x%x, Data = 0x%x\n", a_pSendData[0], a_pSendData[1]);
		return -1;
	}

	return 0;
}

#if 0

static int s4EEPROM_ReadReg_LC898212XDAF_F(u16 addr, u8 *data)
{
	int i4RetValue = 0;

	u8 puSendCmd[2] = { (u8) (addr >> 8), (u8) (addr & 0xFF) };

	i4RetValue = s4AF_ReadReg_LC898212XDAF_F(puSendCmd, sizeof(puSendCmd), data, 1, EEPROM_I2C_SLAVE_ADDR);
	if (i4RetValue < 0)
		LOG_INF("I2C read e2prom failed!!\n");

	return i4RetValue;
}
static void LC898212XD_init(void)
{

	u8 val1 = 0, val2 = 0;

	int Hall_Off = 0x80;	
	int Hall_Bias = 0x80;	

	unsigned short HallMinCheck = 0;
	unsigned short HallMaxCheck = 0;
	unsigned short HallCheck = 0;

	s4EEPROM_ReadReg_LC898212XDAF_F(0x0003, &val1);
	LOG_INF("Addr = 0x0003 , Data = %x\n", val1);

	s4EEPROM_ReadReg_LC898212XDAF_F(0x0004, &val2);
	LOG_INF("Addr = 0x0004 , Data = %x\n", val2);

	if (val1 == 0xb && val2 == 0x2) { 

		
		s4EEPROM_ReadReg_LC898212XDAF_F(0x0F33, &val2);
		s4EEPROM_ReadReg_LC898212XDAF_F(0x0F34, &val1);
		Hall_Min = ((val1 << 8) | (val2 & 0x00FF)) & 0xFFFF;

		s4EEPROM_ReadReg_LC898212XDAF_F(0x0F35, &val2);
		s4EEPROM_ReadReg_LC898212XDAF_F(0x0F36, &val1);
		Hall_Max = ((val1 << 8) | (val2 & 0x00FF)) & 0xFFFF;

		s4EEPROM_ReadReg_LC898212XDAF_F(0x0F37, &val1);
		Hall_Off = val1;
		s4EEPROM_ReadReg_LC898212XDAF_F(0x0F38, &val2);
		Hall_Bias = val2;

	} else { 

		
		s4EEPROM_ReadReg_LC898212XDAF_F(0x0F67, &val1);
		s4EEPROM_ReadReg_LC898212XDAF_F(0x0F68, &val2);
		HallMinCheck = ((val1 << 8) | (val2 & 0x00FF)) & 0xFFFF;

		s4EEPROM_ReadReg_LC898212XDAF_F(0x0F69, &val1);
		s4EEPROM_ReadReg_LC898212XDAF_F(0x0F70, &val2);
		HallMaxCheck = ((val1 << 8) | (val2 & 0x00FF)) & 0xFFFF;

		s4EEPROM_ReadReg_LC898212XDAF_F(0x0F63, &val1);
		HallCheck = val1;

		if ((HallCheck == 0) && (0x1FFF <= HallMaxCheck && HallMaxCheck <= 0x7FFF) &&
			(0x8001 <= HallMinCheck && HallMinCheck <= 0xEFFF)) {

			s4EEPROM_ReadReg_LC898212XDAF_F(0x0F63, &val1);
			s4EEPROM_ReadReg_LC898212XDAF_F(0x0F64, &val2);
			Hall_Bias = ((val1 << 8) | (val2 & 0x00FF)) & 0xFFFF;

			s4EEPROM_ReadReg_LC898212XDAF_F(0x0F65, &val1);
			s4EEPROM_ReadReg_LC898212XDAF_F(0x0F66, &val2);
			Hall_Off = ((val1 << 8) | (val2 & 0x00FF)) & 0xFFFF;

			Hall_Min = HallMinCheck;

			Hall_Max = HallMaxCheck;
			
		} else {

			
			s4EEPROM_ReadReg_LC898212XDAF_F(0x0F63, &val2);
			s4EEPROM_ReadReg_LC898212XDAF_F(0x0F64, &val1);
			HallMinCheck = ((val1 << 8) | (val2 & 0x00FF)) & 0xFFFF;

			s4EEPROM_ReadReg_LC898212XDAF_F(0x0F65, &val2);
			s4EEPROM_ReadReg_LC898212XDAF_F(0x0F66, &val1);
			HallMaxCheck = ((val1 << 8) | (val2 & 0x00FF)) & 0xFFFF;

			s4EEPROM_ReadReg_LC898212XDAF_F(0x0F67, &val1);
			s4EEPROM_ReadReg_LC898212XDAF_F(0x0F68, &val2);

			if ((val1 != 0) && (val2 != 0) && (0x1FFF <= HallMaxCheck && HallMaxCheck <= 0x7FFF) &&
				(0x8001 <= HallMinCheck && HallMinCheck <= 0xEFFF)) {

				Hall_Min = HallMinCheck;
				Hall_Max = HallMaxCheck;

				
				Hall_Off = val1;
				
				Hall_Bias = val2;
				

			} else {

				
				s4EEPROM_ReadReg_LC898212XDAF_F(0x0F33, &val2);
				s4EEPROM_ReadReg_LC898212XDAF_F(0x0F34, &val1);
				HallMinCheck = ((val1 << 8) | (val2 & 0x00FF)) & 0xFFFF;

				s4EEPROM_ReadReg_LC898212XDAF_F(0x0F35, &val2);
				s4EEPROM_ReadReg_LC898212XDAF_F(0x0F36, &val1);
				HallMaxCheck = ((val1 << 8) | (val2 & 0x00FF)) & 0xFFFF;

				s4EEPROM_ReadReg_LC898212XDAF_F(0x0F37, &val1);
				s4EEPROM_ReadReg_LC898212XDAF_F(0x0F38, &val2);

				if ((val1 != 0) && (val2 != 0) && (0x1FFF <= HallMaxCheck && HallMaxCheck <= 0x7FFF) &&
					(0x8001 <= HallMinCheck && HallMinCheck <= 0xEFFF)) {

					Hall_Min = HallMinCheck;
					Hall_Max = HallMaxCheck;

					
					Hall_Off = val1;
					
					Hall_Bias = val2;
					
				} else {
					
					s4EEPROM_ReadReg_LC898212XDAF_F(0x0016, &val1);
					s4EEPROM_ReadReg_LC898212XDAF_F(0x0015, &val2);
					HallMinCheck = ((val1 << 8) | (val2 & 0x00FF)) & 0xFFFF;

					s4EEPROM_ReadReg_LC898212XDAF_F(0x0018, &val1);
					s4EEPROM_ReadReg_LC898212XDAF_F(0x0017, &val2);
					HallMaxCheck = ((val1 << 8) | (val2 & 0x00FF)) & 0xFFFF;

					if ((0x1FFF <= HallMaxCheck && HallMaxCheck <= 0x7FFF) &&
						(0x8001 <= HallMinCheck && HallMinCheck <= 0xEFFF)) {

						Hall_Min = HallMinCheck;
						Hall_Max = HallMaxCheck;

						s4EEPROM_ReadReg_LC898212XDAF_F(0x001A, &val1);
						s4EEPROM_ReadReg_LC898212XDAF_F(0x0019, &val2);
						Hall_Off = val2;
						Hall_Bias = val1;
					}
				}
			}
		}
	}


	if (!(0 <= Hall_Max && Hall_Max <= 0x7FFF)) {
		signed short Temp;

		Temp = Hall_Min;
		Hall_Min = Hall_Max;
		Hall_Max = Temp;
	}

	LOG_INF("=====LC898212XD:=init=hall_max:0x%x==hall_min:0x%x====halloff:0x%x, hallbias:0x%x===\n",
	     Hall_Max, Hall_Min, Hall_Off, Hall_Bias);

	LC898212XDAF_F_MONO_init(Hall_Off, Hall_Bias);
}

static unsigned short AF_convert(int position)
{
#if 0	 
	return (((position - Min_Pos) * (unsigned short)(Hall_Max - Hall_Min) / (Max_Pos -
										 Min_Pos)) +
		Hall_Min) & 0xFFFF;
#else	 
	return (((Max_Pos - position) * (unsigned short)(Hall_Max - Hall_Min) / (Max_Pos -
										 Min_Pos)) +
		Hall_Min) & 0xFFFF;
#endif
}
#endif


static inline int getAFInfo(__user stAF_MotorInfo * pstMotorInfo)
{
	stAF_MotorInfo stMotorInfo;

	stMotorInfo.u4MacroPosition = g_u4AF_MACRO;
	stMotorInfo.u4InfPosition = g_u4AF_INF;
	stMotorInfo.u4CurrentPosition = g_u4CurrPosition;
	stMotorInfo.bIsSupportSR = 1;

	stMotorInfo.bIsMotorMoving = 1;

	if (*g_pAF_Opened >= 1)
		stMotorInfo.bIsMotorOpen = 1;
	else
		stMotorInfo.bIsMotorOpen = 0;

	if (copy_to_user(pstMotorInfo, &stMotorInfo, sizeof(stAF_MotorInfo)))
		LOG_INF("copy to user failed when getting motor information\n");

       printk("%s: g_u4CurrPosition = %ld \n", __func__, g_u4CurrPosition);
	return 0;
}


#if 0
static inline int moveAF(unsigned long a_u4Position)
{
	if ((a_u4Position > g_u4AF_MACRO) || (a_u4Position < g_u4AF_INF)) {
		LOG_INF("out of range\n");
		return -EINVAL;
	}

	if (*g_pAF_Opened == 1) {
		
		LC898212XD_init();

		spin_lock(g_pAF_SpinLock);
		g_u4CurrPosition = a_u4Position;
		*g_pAF_Opened = 2;
		spin_unlock(g_pAF_SpinLock);

		LC898212XDAF_F_MONO_moveAF(AF_convert((int)a_u4Position));
	}

	if (g_u4CurrPosition == a_u4Position)
		return 0;

	if ((LC898212XDAF_F_MONO_moveAF(AF_convert((int)a_u4Position))&0x1) == 0) {
		spin_lock(g_pAF_SpinLock);
		g_u4CurrPosition = (unsigned long)a_u4Position;
		spin_unlock(g_pAF_SpinLock);
	} else {
		LOG_INF("set I2C failed when moving the motor\n");
	}

	return 0;
}
#else

static long g_i4Dir = 0;
static long g_i4MotorStatus = 0;
static unsigned long g_u4TargetPosition;



static int ReadI2C(u8 length, u8 addr, u16 *data)
{
    u8 pBuff[2];
    u8 u8data=0;

    g_pstAF_I2Cclient->addr = (I2C_SLAVE_ADDRESS) >> 1;
    if (i2c_master_send(g_pstAF_I2Cclient, &addr, 1) < 0 )
    {
        LOG_INF("[CAMERA SENSOR] read I2C send failed!!\n");
        return -1;
    }

    if(length==0)
    {
        if (i2c_master_recv(g_pstAF_I2Cclient, &u8data, 1) < 0)
        {
            LOG_INF("ReadI2C failed!! \n");
            return -1;
        }
        *data = u8data;
    }
    else if(length==1)
    {
        if (i2c_master_recv(g_pstAF_I2Cclient, pBuff, 2) < 0)
        {
            LOG_INF("ReadI2C 2 failed!! \n");
            return -1;
        }

        *data = (((u16)pBuff[0]) << 8) + ((u16)pBuff[1]);
    }
    LOG_INF("ReadI2C 0x%x, 0x%x, 0x%x \n", length, addr, *data);

    return 0;
}

static int WriteI2C(u8 length, u8 addr, u16 data)
{
    u8 puSendCmd[2] = {addr, (u8)(data&0xFF)};
    u8 puSendCmd2[3] = {addr, (u8)((data>>8)&0xFF), (u8)(data&0xFF)};
    LOG_INF("WriteI2C 0x%x, 0x%x, 0x%x\n", length, addr, data);
	
    g_pstAF_I2Cclient->addr = (I2C_SLAVE_ADDRESS) >> 1;
    
    if(length==0)
    {
        if (i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2) < 0)
        {
            LOG_INF("WriteI2C failed!! \n");
            return -1;
        }
    }
    else if(length==1)
    {
        if (i2c_master_send(g_pstAF_I2Cclient, puSendCmd2, 3) < 0)
        {
            LOG_INF("WriteI2C 2 failed!! \n");
            return -1;
        }
    }
    
    return 0;
}

extern char t4ka7_otp_data[23];

void LC898212XDAF_init_drv(void)
{
     u16 Reg_MTM_0x3C;
     u16 Reg_MTM_0x3C_2;

     LOG_INF("Load  LC898212AF_MTM_init_drv\n");
     WriteI2C(0,0x80, 0x34);
     WriteI2C(0,0x81, 0x20);
     WriteI2C(0,0x84, 0xE0);
     WriteI2C(0,0x87, 0x05);
     WriteI2C(0,0xA4, 0x24);
     WriteI2C(0,0x8B, 0x80);
     WriteI2C(0,0x3A, 0x00);
     WriteI2C(0,0x3B, 0x00);
     WriteI2C(0,0x04, 0x00);
     WriteI2C(0,0x05, 0x00);
     WriteI2C(0,0x02, 0x00);
     WriteI2C(0,0x03, 0x00);
     WriteI2C(0,0x18, 0x00);
     WriteI2C(0,0x19, 0x00);
     WriteI2C(0,0x28, 0x80);
     WriteI2C(0,0x29, 0x80);
     WriteI2C(0,0x83, 0x2C);
     WriteI2C(0,0x84, 0xE3);
     WriteI2C(0,0x97, 0x00);
     WriteI2C(0,0x98, 0x42);
     WriteI2C(0,0x99, 0x00);
     WriteI2C(0,0x9A, 0x00);
     
     WriteI2C(0,0x88, 0x68);
     WriteI2C(0,0x92, 0x00);
     WriteI2C(0,0xA0, 0x01);
     WriteI2C(0,0x7A, 0x68);
     WriteI2C(0,0x7B, 0x00);
     WriteI2C(0,0x7E, 0x78);
     WriteI2C(0,0x7F, 0x00);
     WriteI2C(0,0x7C, 0x01);
     WriteI2C(0,0x7D, 0x00);
     WriteI2C(0,0x93, 0xC0);
     WriteI2C(0,0x86, 0x60);
     WriteI2C(0,0x40, 0x80);
     WriteI2C(0,0x41, 0x10);
     WriteI2C(0,0x42, 0x75);
     WriteI2C(0,0x43, 0x70);
     WriteI2C(0,0x44, 0x8B);
     WriteI2C(0,0x45, 0x50);
     WriteI2C(0,0x46, 0x6A);
     WriteI2C(0,0x47, 0x10);
     WriteI2C(0,0x48, 0x5A);
     WriteI2C(0,0x49, 0x90);
     WriteI2C(0,0x76, 0x0C);
     WriteI2C(0,0x77, 0x50);
     WriteI2C(0,0x4A, 0x20);
     WriteI2C(0,0x4B, 0x30);
     WriteI2C(0,0x50, 0x04);
     WriteI2C(0,0x51, 0xF0);
     WriteI2C(0,0x52, 0x76);
     WriteI2C(0,0x53, 0x10);
     WriteI2C(0,0x54, 0x14);
     WriteI2C(0,0x55, 0x50);
     WriteI2C(0,0x56, 0x00);
     WriteI2C(0,0x57, 0x00);
     WriteI2C(0,0x58, 0x7F);
     WriteI2C(0,0x59, 0xF0);
     WriteI2C(0,0x4C, 0x32);
     WriteI2C(0,0x4D, 0xF0);
     WriteI2C(0,0x78, 0x20);
     WriteI2C(0,0x79, 0x00);
     WriteI2C(0,0x4E, 0x7F);
     WriteI2C(0,0x4F, 0xF0);
     WriteI2C(0,0x6E, 0x00);
     WriteI2C(0,0x6F, 0x00);
     WriteI2C(0,0x72, 0x18);
     WriteI2C(0,0x73, 0xE0);
     WriteI2C(0,0x74, 0x4E);
     WriteI2C(0,0x75, 0x30);
     WriteI2C(0,0x30, 0x00);
     WriteI2C(0,0x31, 0x00);
     WriteI2C(0,0x5A, 0x06);
     WriteI2C(0,0x5B, 0x80);
     WriteI2C(0,0x5C, 0x72);
     WriteI2C(0,0x5D, 0xF0);
     WriteI2C(0,0x5E, 0x7F);
     WriteI2C(0,0x5F, 0x70);
     WriteI2C(0,0x60, 0x7E);
     WriteI2C(0,0x61, 0xD0);
     WriteI2C(0,0x62, 0x7F);
     WriteI2C(0,0x63, 0xF0);
     WriteI2C(0,0x64, 0x00);
     WriteI2C(0,0x65, 0x00);
     WriteI2C(0,0x66, 0x00);
     WriteI2C(0,0x67, 0x00);
     WriteI2C(0,0x68, 0x51);
     WriteI2C(0,0x69, 0x30);
     WriteI2C(0,0x6A, 0x72);
     WriteI2C(0,0x6B, 0xF0);
     WriteI2C(0,0x70, 0x00);
     WriteI2C(0,0x71, 0x00);
     WriteI2C(0,0x6C, 0x80);
     WriteI2C(0,0x6D, 0x10);
     WriteI2C(0,0x76, 0x0C);
     WriteI2C(0,0x77, 0x50);
     WriteI2C(0,0x78, 0x20);
     WriteI2C(0,0x79, 0x00);
     WriteI2C(0,0x30, 0x00);
     WriteI2C(0,0x31, 0x00);
     
     WriteI2C(0,0x28, t4ka7_otp_data[14]);  
     WriteI2C(0,0x29, t4ka7_otp_data[13]);  
     WriteI2C(0,0x3A, 0x00);
     WriteI2C(0,0x3B, 0x00);
     WriteI2C(0,0x04, 0x00);
     WriteI2C(0,0x05, 0x00);
     WriteI2C(0,0x02, 0x00);
     WriteI2C(0,0x03, 0x00);
     WriteI2C(0,0x85, 0xC0);
     msleep(5);
     
     ReadI2C( 1, 0x3C, &Reg_MTM_0x3C);
     WriteI2C(1, 0x04, Reg_MTM_0x3C);
     WriteI2C(1, 0x18, Reg_MTM_0x3C);
     WriteI2C(0,0x87, 0x85);
     
     WriteI2C(0,0x5A, 0x08);
     WriteI2C(0,0x5B, 0x00);
     WriteI2C(0,0x83, 0xac);
     WriteI2C(0,0xA0, 0x01);
     
     ReadI2C( 1, 0x3C, &Reg_MTM_0x3C_2);
     WriteI2C(1, 0x18, Reg_MTM_0x3C_2);
     
     
     if ((t4ka7_otp_data[18] << 8 | t4ka7_otp_data[17]) > ReadI2C( 1, 0x3C, &Reg_MTM_0x3C_2))
     {
          WriteI2C(1, 0xA1, (t4ka7_otp_data[18] << 8 | t4ka7_otp_data[17])&0xfff0);
          WriteI2C(1, 0x16, 0x0180);
          WriteI2C(0, 0x8A, 0x8D);
     }
     else if ((t4ka7_otp_data[18] << 8 | t4ka7_otp_data[17]) < ReadI2C( 1, 0x3C, &Reg_MTM_0x3C_2))
     {
          WriteI2C(1, 0xA1, (t4ka7_otp_data[18] << 8 | t4ka7_otp_data[17])&0xfff0);
          WriteI2C(1, 0x16, 0xFE80);
          WriteI2C(0, 0x8A, 0x8D);
     }
     msleep(30);
}

void LC898212XDAF_TDK_init_drv(void)
{
     u16 Reg_TDK_0x3C;
     u16 Reg_TDK_0x3C_2;
     LOG_INF("Load  LC898212AF_TDK_init_drv\n");
      
     WriteI2C(0,0x80, 0x34);
     WriteI2C(0,0x81, 0x20);
     WriteI2C(0,0x84, 0xE0);
     WriteI2C(0,0x87, 0x05);
     WriteI2C(0,0xA4, 0x24);
     WriteI2C(0,0x8B, 0x80);
     WriteI2C(0,0x3A, 0x00);
     WriteI2C(0,0x3B, 0x00);
     WriteI2C(0,0x04, 0x00);
     WriteI2C(0,0x05, 0x00);
     WriteI2C(0,0x02, 0x00);
     WriteI2C(0,0x03, 0x00);
     WriteI2C(0,0x18, 0x00);
     WriteI2C(0,0x19, 0x00);
     WriteI2C(0,0x28, 0x80);
     WriteI2C(0,0x29, 0x80);
     WriteI2C(0,0x83, 0x2C);
     WriteI2C(0,0x84, 0xE3);
     WriteI2C(0,0x97, 0x00);
     WriteI2C(0,0x98, 0x42);
     WriteI2C(0,0x99, 0x00);
     WriteI2C(0,0x9A, 0x00);
     
     WriteI2C(0,0x88, 0x70);
     WriteI2C(0,0x92, 0x00);
     WriteI2C(0,0xA0, 0x02);
     WriteI2C(0,0x7A, 0x68);
     WriteI2C(0,0x7B, 0x00);
     WriteI2C(0,0x7E, 0x78);
     WriteI2C(0,0x7F, 0x00);
     WriteI2C(0,0x7C, 0x01);
     WriteI2C(0,0x7D, 0x00);
     WriteI2C(0,0x93, 0xC0);
     WriteI2C(0,0x86, 0x60);
     WriteI2C(0,0x40, 0x40);
     WriteI2C(0,0x41, 0x30);
     WriteI2C(0,0x42, 0x71);
     WriteI2C(0,0x43, 0x50);
     WriteI2C(0,0x44, 0x8F);
     WriteI2C(0,0x45, 0x90);
     WriteI2C(0,0x46, 0x61);
     WriteI2C(0,0x47, 0xB0);
     WriteI2C(0,0x48, 0x7F);
     WriteI2C(0,0x49, 0xF0);
     WriteI2C(0,0x76, 0x0C);
     WriteI2C(0,0x77, 0x50);
     WriteI2C(0,0x4A, 0x39);
     WriteI2C(0,0x4B, 0x30);
     WriteI2C(0,0x50, 0x04);
     WriteI2C(0,0x51, 0xF0);
     WriteI2C(0,0x52, 0x76);
     WriteI2C(0,0x53, 0x10);
     WriteI2C(0,0x54, 0x20);
     WriteI2C(0,0x55, 0x30);
     WriteI2C(0,0x56, 0x00);
     WriteI2C(0,0x57, 0x00);
     WriteI2C(0,0x58, 0x7F);
     WriteI2C(0,0x59, 0xF0);
     WriteI2C(0,0x4C, 0x40);
     WriteI2C(0,0x4D, 0x30);
     WriteI2C(0,0x78, 0x40);
     WriteI2C(0,0x79, 0x00);
     WriteI2C(0,0x4E, 0x80);
     WriteI2C(0,0x4F, 0x10);
     WriteI2C(0,0x6E, 0x00);
     WriteI2C(0,0x6F, 0x00);
     WriteI2C(0,0x72, 0x18);
     WriteI2C(0,0x73, 0xE0);
     WriteI2C(0,0x74, 0x4E);
     WriteI2C(0,0x75, 0x30);
     WriteI2C(0,0x30, 0x00);
     WriteI2C(0,0x31, 0x00);
     WriteI2C(0,0x5A, 0x06);
     WriteI2C(0,0x5B, 0x80);
     WriteI2C(0,0x5C, 0x72);
     WriteI2C(0,0x5D, 0xF0);
     WriteI2C(0,0x5E, 0x7F);
     WriteI2C(0,0x5F, 0x70);
     WriteI2C(0,0x60, 0x7E);
     WriteI2C(0,0x61, 0xD0);
     WriteI2C(0,0x62, 0x7F);
     WriteI2C(0,0x63, 0xF0);
     WriteI2C(0,0x64, 0x00);
     WriteI2C(0,0x65, 0x00);
     WriteI2C(0,0x66, 0x00);
     WriteI2C(0,0x67, 0x00);
     WriteI2C(0,0x68, 0x51);
     WriteI2C(0,0x69, 0x30);
     WriteI2C(0,0x6A, 0x72);
     WriteI2C(0,0x6B, 0xF0);
     WriteI2C(0,0x70, 0x00);
     WriteI2C(0,0x71, 0x00);
     WriteI2C(0,0x6C, 0x80);
     WriteI2C(0,0x6D, 0x10);
     WriteI2C(0,0x76, 0x0c);
     WriteI2C(0,0x77, 0x50);
     WriteI2C(0,0x78, 0x40);
     WriteI2C(0,0x79, 0x00);
     WriteI2C(0,0x30, 0x00);
     WriteI2C(0,0x31, 0x00);
     
     WriteI2C(0,0x28, t4ka7_otp_data[14]);  
     WriteI2C(0,0x29, t4ka7_otp_data[13]);  
     WriteI2C(0,0x3A, 0x00);
     WriteI2C(0,0x3B, 0x00);
     WriteI2C(0,0x04, 0x00);
     WriteI2C(0,0x05, 0x00);
     WriteI2C(0,0x02, 0x00);
     WriteI2C(0,0x03, 0x00);
     WriteI2C(0,0x85, 0xC0);
     msleep(5);
     
     ReadI2C( 1, 0x3C, &Reg_TDK_0x3C);
     WriteI2C(1, 0x04, Reg_TDK_0x3C);
     WriteI2C(1, 0x18, Reg_TDK_0x3C);
     WriteI2C(0,0x87, 0x85);
     
     WriteI2C(0,0x5A, 0x08);
     WriteI2C(0,0x5B, 0x00);
     WriteI2C(0,0x83, 0xac);
     WriteI2C(0,0xA0, 0x02);
     
     ReadI2C( 1, 0x3C, &Reg_TDK_0x3C_2);
     WriteI2C(1, 0x18, Reg_TDK_0x3C_2);
     
     

     if((t4ka7_otp_data[18] << 8 | t4ka7_otp_data[17]) > ReadI2C( 1, 0x3C, &Reg_TDK_0x3C_2))
     {
          WriteI2C(1, 0xA1, (t4ka7_otp_data[18] << 8 | t4ka7_otp_data[17])&0xfff0);
          WriteI2C(1, 0x16, 0x0180);
          WriteI2C(0, 0x8A, 0x8D);
     }
     else if((t4ka7_otp_data[18] << 8 | t4ka7_otp_data[17]) < ReadI2C( 1, 0x3C, &Reg_TDK_0x3C_2))
     {
          WriteI2C(1, 0xA1, (t4ka7_otp_data[18] << 8 | t4ka7_otp_data[17])&0xfff0);
          WriteI2C(1, 0x16, 0xFE80);
          WriteI2C(0, 0x8A, 0x8D);
     }

     msleep(30);
}

static int SetVCMPos(u16 _wData)
{
    u16 TargetPos;
    
    u16 ExistentPos = 0;
    int i2cret=0;

    LOG_INF("t4ka7_vcm_driver_vendor_flag = %d (0=TDK , 1=MTM) , Set  _wData = %d . \n", t4ka7_vcm_driver_vendor_flag , _wData );

    _wData = 1023 - _wData;

    LOG_INF("After cal ,  _wData = %d . \n", _wData );

    _wData = _wData<<2 ;

    
    if( _wData < 0x800 ) TargetPos =  0x800  - _wData; 
    else                 TargetPos = (0x1800 - _wData)&0xFFF; 
    TargetPos = TargetPos<<4;

    ReadI2C(1, 0x3C, &ExistentPos);
    printk("SetVCMPos 0x%x 0x%x  \n", TargetPos, ExistentPos);
    if ((signed short)TargetPos > (signed short)ExistentPos)
    {
        WriteI2C(1, 0xA1, TargetPos&0xfff0);
        
         if(t4ka7_vcm_driver_vendor_flag == 0)
          WriteI2C(1, 0x16, 0xFE80);
          else
        WriteI2C(1, 0x16, 0x0180);
        
        i2cret = WriteI2C(0, 0x8A, 0x0D);
    }
	else if ((signed short)TargetPos < (signed short)ExistentPos)
    {
        WriteI2C(1, 0xA1, TargetPos&0xfff0);
       
        if(t4ka7_vcm_driver_vendor_flag == 0)
          WriteI2C(1, 0x16, 0x0180);
          else
        WriteI2C(1, 0x16, 0xFE80);
        
        i2cret = WriteI2C(0, 0x8A, 0x0D);
    }
    return i2cret;
}

inline static int moveAF(unsigned long a_u4Position)
{
    int ret = 0;
    if((a_u4Position > g_u4AF_MACRO) || (a_u4Position < g_u4AF_INF))
    {
        LOG_INF("out of range \n");
        return -EINVAL;
    }

    if (*g_pAF_Opened == 1)
    {
        u16 InitPos;
	LOG_INF("t4ka7_vcm_driver_vendor_flag = %d (0=TDK , 1=MTM)\n", t4ka7_vcm_driver_vendor_flag);
        if(t4ka7_vcm_driver_vendor_flag == 0)
           LC898212XDAF_TDK_init_drv();
        else
           LC898212XDAF_init_drv();

		
        ret = ReadI2C(1, 0x3C, &InitPos);

        if(ret == 0)
        {
            LOG_INF("Init Pos %6d \n", InitPos);

            spin_lock(g_pAF_SpinLock);
            
            spin_unlock(g_pAF_SpinLock);

        }
        else
        {
            spin_lock(g_pAF_SpinLock);
            
            spin_unlock(g_pAF_SpinLock);
        }

        spin_lock(g_pAF_SpinLock);
        *g_pAF_Opened = 2;
        spin_unlock(g_pAF_SpinLock);
		return 0;
    }

    if (g_u4CurrPosition < a_u4Position)
    {
        spin_lock(g_pAF_SpinLock);
        g_i4Dir = 1;
        spin_unlock(g_pAF_SpinLock);
    }
    else if (g_u4CurrPosition > a_u4Position)
    {
        spin_lock(g_pAF_SpinLock);
        g_i4Dir = -1;
        spin_unlock(g_pAF_SpinLock);
    }
    

    spin_lock(g_pAF_SpinLock);
    g_u4TargetPosition = a_u4Position;
    spin_unlock(g_pAF_SpinLock);

    

    spin_lock(g_pAF_SpinLock);
    g_sr = 3;
    g_i4MotorStatus = 0;
    spin_unlock(g_pAF_SpinLock);

    if(SetVCMPos((u16)g_u4TargetPosition) == 0)
    {
        spin_lock(g_pAF_SpinLock);
        g_u4CurrPosition = (unsigned long)g_u4TargetPosition;
        spin_unlock(g_pAF_SpinLock);
    }
    else
    {
        LOG_INF("set I2C failed when moving the motor \n");

        spin_lock(g_pAF_SpinLock);
        g_i4MotorStatus = -1;
        spin_unlock(g_pAF_SpinLock);
    }

    return 0;
}

#endif
static inline int setAFInf(unsigned long a_u4Position)
{
	spin_lock(g_pAF_SpinLock);
	g_u4AF_INF = a_u4Position;
	spin_unlock(g_pAF_SpinLock);
	return 0;
}

static inline int setAFMacro(unsigned long a_u4Position)
{
	spin_lock(g_pAF_SpinLock);
	g_u4AF_MACRO = a_u4Position;
	spin_unlock(g_pAF_SpinLock);
	return 0;
}

static inline int getAFCalPos(__user stAF_MotorCalPos * pstMotorCalPos)
{
	return 0;
}

long LC898212XDAF_F_Ioctl(struct file *a_pstFile, unsigned int a_u4Command, unsigned long a_u4Param)
{
	long i4RetValue = 0;

	switch (a_u4Command) {
	case AFIOC_G_MOTORINFO:
		i4RetValue = getAFInfo((__user stAF_MotorInfo *) (a_u4Param));
		break;

	case AFIOC_T_MOVETO:
		i4RetValue = moveAF(a_u4Param);
		break;

	case AFIOC_T_SETINFPOS:
		i4RetValue = setAFInf(a_u4Param);
		break;

	case AFIOC_T_SETMACROPOS:
		i4RetValue = setAFMacro(a_u4Param);
		break;

	default:
		LOG_INF("No CMD\n");
		i4RetValue = -EPERM;
		break;
	}

	return i4RetValue;
}

int LC898212XDAF_F_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
	LOG_INF("Start\n");

	if (*g_pAF_Opened == 2) {
		LOG_INF("Wait\n");

		msleep(20);
	}

	if (*g_pAF_Opened) {
		LOG_INF("Free\n");

		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 0;
		spin_unlock(g_pAF_SpinLock);
	}

	LOG_INF("End\n");

	return 0;
}

void LC898212XDAF_F_SetI2Cclient(struct i2c_client *pstAF_I2Cclient, spinlock_t *pAF_SpinLock, int *pAF_Opened)
{
	g_pstAF_I2Cclient = pstAF_I2Cclient;
	g_pAF_SpinLock = pAF_SpinLock;
	g_pAF_Opened = pAF_Opened;
}
