
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include "LC898123AF.h"
#include "Ois.h"
#include "ois_otp_def.h"
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#define LENS_I2C_BUSNUM 0


#define LC898123AF_DRVNAME "LC898123AF_OPENLOOP"
#define LC898123AF_VCM_WRITE_ID           0x7C

#define OV16880_SECTOR_UNIT 64*4
#define OV16880_PD_DATA_SECTOR_NUM 5

#ifdef LC898123AF_DEBUG
#define LC898123AFDB printk
#else
#define LC898123AFDB(x,...)
#endif

static spinlock_t *g_LC898123AF_SpinLock;

static struct i2c_client * g_pstLC898123AF_I2Cclient = NULL;

#define OIS_DMA_BUF_SIZE 64
static u8 *ois_i2c_dma_va = NULL;
static dma_addr_t ois_i2c_dma_pa = 0;


static int  *g_s4LC898123AF_Opened = 0;
static int g_i4MotorStatus = 0;
static int g_i4Dir = 0;
static unsigned int g_u4LC898123AF_INF = 0;
static unsigned int g_u4LC898123AF_MACRO = 1023;
static unsigned int g_u4TargetPosition = 0;
static unsigned int g_u4CurrPosition   = 0;
static unsigned int g_u4InitPosition   = 100;

static int g_sr = 3;

void RegWriteA(unsigned short RegAddr, unsigned char RegData)
{
    int  i4RetValue = 0;
    char puSendCmd[3] = {(char)((RegAddr>>16)&0xFFFF),(char)(RegAddr&0xFFFF),RegData};
    

    g_pstLC898123AF_I2Cclient->ext_flag |= I2C_A_FILTER_MSG;
    g_pstLC898123AF_I2Cclient->addr = (LC898123AF_VCM_WRITE_ID >> 1);
    i4RetValue = i2c_master_send(g_pstLC898123AF_I2Cclient, puSendCmd, 3);
    if (i4RetValue < 0) 
    {
        LC898123AFDB("[LC898123AF]   %s   I2C send failed!! \n", __func__);
        return;
    }
}
void RegReadA(unsigned short RegAddr, unsigned char *RegData)
{
    int  i4RetValue = 0;
    char pBuff[2] = {(char)(RegAddr >> 16) , (char)(RegAddr & 0xFFFF)};

    g_pstLC898123AF_I2Cclient->addr = (LC898123AF_VCM_WRITE_ID >> 1);

    i4RetValue = i2c_master_send(g_pstLC898123AF_I2Cclient, pBuff, 2);
    if (i4RetValue < 0 ) 
    {
        LC898123AFDB("[CAMERA SENSOR]   %s   read I2C send failed!!\n", __func__);
        return;
    }

    i4RetValue = i2c_master_recv(g_pstLC898123AF_I2Cclient, (u8*)RegData, 1);

    
    if (i4RetValue != 1) 
    {
        LC898123AFDB("[CAMERA SENSOR]   %s   I2C read failed!! \n", __func__);
        return;
    }
}
void RamWriteA( unsigned short RamAddr, unsigned short RamData )

{
    int  i4RetValue = 0;
    char puSendCmd[4] = {(char)((RamAddr >>  16)&0xFFFF), 
                         (char)( RamAddr       &0xFFFF),
                         (char)((RamData >>  8)&0xFFFF), 
                         (char)( RamData       &0xFFFF)};
    

    g_pstLC898123AF_I2Cclient->ext_flag |= I2C_A_FILTER_MSG;
    g_pstLC898123AF_I2Cclient->addr = (LC898123AF_VCM_WRITE_ID >> 1);
    i4RetValue = i2c_master_send(g_pstLC898123AF_I2Cclient, puSendCmd, 4);
    if (i4RetValue < 0) 
    {
        LC898123AFDB("[LC898123AF]   %s   I2C send failed!! \n", __func__);
        return;
    }
}
void RamReadA( unsigned short RamAddr, void * ReadData )
{
    int  i4RetValue = 0;
    char pBuff[2] = {(char)(RamAddr >> 16) , (char)(RamAddr & 0xFFFF)};
    unsigned short  vRcvBuff=0;
	unsigned int *pRcvBuff;
    pRcvBuff =(unsigned int *)ReadData;

    g_pstLC898123AF_I2Cclient->addr = (LC898123AF_VCM_WRITE_ID >> 1);

    i4RetValue = i2c_master_send(g_pstLC898123AF_I2Cclient, pBuff, 2);
    if (i4RetValue < 0 ) 
    {
        LC898123AFDB("[CAMERA SENSOR]   %s   read I2C send failed!!\n", __func__);
        return;
    }

    i4RetValue = i2c_master_recv(g_pstLC898123AF_I2Cclient, (u8*)&vRcvBuff, 2);
    if (i4RetValue != 2) 
    {
        LC898123AFDB("[CAMERA SENSOR]   %s   I2C read failed!! \n", __func__);
        return;
    }
    *pRcvBuff=    ((vRcvBuff&0xFFFF) <<16) + ((vRcvBuff>> 16)&0xFFFF) ;
    
    

}
void RamWrite32A(unsigned short RamAddr, unsigned int RamData )
{
    int  i4RetValue = 0;
    char puSendCmd[6] = {(char)((RamAddr >>  8)&0xFF), 
                         (char)( RamAddr       &0xFF),
                         (char)((RamData >>  24)&0xFF), 
                         (char)((RamData >>  16)&0xFF), 
                         (char)((RamData >>  8)&0xFF), 
                         (char)( RamData       &0xFF)};
    
    LC898123AFDB("[LC898123AF]   %s   I2C w4 (%x %x) \n", __func__, RamAddr,(unsigned int)RamData);
    
    g_pstLC898123AF_I2Cclient->ext_flag |= I2C_A_FILTER_MSG;
    g_pstLC898123AF_I2Cclient->addr = (LC898123AF_VCM_WRITE_ID >> 1);
    i4RetValue = i2c_master_send(g_pstLC898123AF_I2Cclient, puSendCmd, 6);
    if (i4RetValue < 0) 
    {
        LC898123AFDB("[LC898123AF]   %s   I2C send failed!! \n", __func__);
        return;
    }
    else
	LC898123AFDB("[LC898123AF]   %s   I2C send OK!! \n", __func__);
   
}
void RamRead32A(unsigned short RamAddr, void * ReadData )
{
    int  i4RetValue = 0;
    char pBuff[2] = {(char)(RamAddr >> 8) , (char)(RamAddr & 0xFF)};
    unsigned int *pRcvBuff, vRcvBuff=0;
    pRcvBuff =(unsigned int *)ReadData;

    g_pstLC898123AF_I2Cclient->addr = (LC898123AF_VCM_WRITE_ID >> 1);

    i4RetValue = i2c_master_send(g_pstLC898123AF_I2Cclient, pBuff, 2);
    if (i4RetValue < 0 ) 
    {
        LC898123AFDB("[CAMERA SENSOR]   %s   read I2C send failed!!\n", __func__);
        return;
    }

    i4RetValue = i2c_master_recv(g_pstLC898123AF_I2Cclient, (u8*)&vRcvBuff, 4);
    if (i4RetValue != 4) 
    {
        LC898123AFDB("[CAMERA SENSOR]   %s   I2C read failed!! \n", __func__);
        return;
    }
    *pRcvBuff=   ((vRcvBuff     &0xFF) <<24) 
               +(((vRcvBuff>> 8)&0xFF) <<16) 
               +(((vRcvBuff>>16)&0xFF) << 8) 
               +(((vRcvBuff>>24)&0xFF)     );

        LC898123AFDB("[LC898123AF]   %s   I2C r4 (%x %x) \n", __func__, RamAddr,(unsigned int)*pRcvBuff);
}
void WitTim(unsigned short  UsWitTim )
{
    msleep(UsWitTim);
}

int CntWrt(UINT8 * PcSetDat, UINT16 UsDatNum)
{
	int rc = 0;
	int  i4RetValue = 0;
	__u32 ext_flag_backup;
	char puSendCmd[UsDatNum];

	LC898123AFDB("[LC898123AF]   %s   UsDatNum=%u\n", __func__, UsDatNum);

	if (ois_i2c_dma_va == NULL) {
		LC898123AFDB("[LC898123AF]   %s   ois_i2c_dma_va is NULL\n", __func__);
		return -1;
	}

	if (UsDatNum > OIS_DMA_BUF_SIZE) {
		LC898123AFDB("[LC898123AF]   %s   data size(0x%x) > OIS_DMA_BUF_SIZE(0x%x)\n",__func__, UsDatNum ,OIS_DMA_BUF_SIZE);
		return -1;
	}

	
	memset(puSendCmd, 0, UsDatNum);
	memcpy(&puSendCmd[0], &PcSetDat[0], UsDatNum);

	
	memset(ois_i2c_dma_va, 0, OIS_DMA_BUF_SIZE);
	memcpy(&ois_i2c_dma_va[0], &puSendCmd[0], UsDatNum);

	
	g_pstLC898123AF_I2Cclient->ext_flag |= I2C_A_FILTER_MSG;
	g_pstLC898123AF_I2Cclient->addr = (LC898123AF_VCM_WRITE_ID >> 1);

	if (UsDatNum <= 8) { 
		i4RetValue = i2c_master_send(g_pstLC898123AF_I2Cclient, puSendCmd, UsDatNum);
	} else { 
		ext_flag_backup = g_pstLC898123AF_I2Cclient->ext_flag;

		
		g_pstLC898123AF_I2Cclient->ext_flag |= I2C_DMA_FLAG | I2C_ENEXT_FLAG;
		i4RetValue = i2c_master_send(g_pstLC898123AF_I2Cclient, (const char *)ois_i2c_dma_pa, UsDatNum);

		g_pstLC898123AF_I2Cclient->ext_flag = ext_flag_backup;
	}

	if (i4RetValue < 0)
	{
		LC898123AFDB("[LC898123AF]   %s   I2C send failed !! \n", __func__);
		return i4RetValue;
	}
	else
		LC898123AFDB("[LC898123AF]   %s   I2C send OK !! \n", __func__);

	return rc;
}

int CntRd3( UINT32 addr, void * PcSetDat, UINT16 UsDatNum )
{
	
	return 0;
}

void WPBCtrl( UINT8 UcCtrl )
{
	
}



void LC898prtvalue(unsigned short  prtvalue )
{
    LC898123AFDB("[LC898123AF]printvalue ======%x   \n",prtvalue);
}

inline static int getLC898123AFInfo(__user stLC898123AF_MotorInfo * pstMotorInfo)
{
    stLC898123AF_MotorInfo stMotorInfo;
    stMotorInfo.u4MacroPosition   = g_u4LC898123AF_MACRO;
    stMotorInfo.u4InfPosition     = g_u4LC898123AF_INF;
    stMotorInfo.u4CurrentPosition = g_u4CurrPosition / 2; 
    stMotorInfo.bIsSupportSR      = 1;

    if (g_i4MotorStatus == 1)    {stMotorInfo.bIsMotorMoving = 1;}
    else                        {stMotorInfo.bIsMotorMoving = 0;}

    if (*g_s4LC898123AF_Opened >= 1)    {stMotorInfo.bIsMotorOpen = 1;}
    else                        {stMotorInfo.bIsMotorOpen = 0;}

    if(copy_to_user(pstMotorInfo , &stMotorInfo , sizeof(stLC898123AF_MotorInfo)))
    {
        LC898123AFDB("[LC898123AF] copy to user failed when getting motor information \n");
    }
    return 0;
}

void LC898123AF_init_drv(void)
{
	
	unsigned int	UlReadVal ;
	RamWrite32A( 0xC000, 0xD000AC) ;			
	RamRead32A(  0xD000, &UlReadVal ) ;
	printk("[LC898123AFMAPFLAG] 0x%x\n",(unsigned int) UlReadVal);
	RamWrite32A( 0xC000, 0xD00100) ;			
	RamRead32A(  0xD000, &UlReadVal ) ;

	RamWrite32A( 0xF012, 0x01) ;	
	printk("[LC898123AFICFLAG] 0x%x\n", (unsigned int)UlReadVal);
	return;
}

#if 0
#define AF_CENTER_POS   512
static unsigned short SetVCMPos(unsigned short _wData)
{
    
    
    

    LC898123AFDB("%s: Before cal, _wData = %d", __func__, _wData);
    if(_wData >= AF_CENTER_POS) {
        _wData -= AF_CENTER_POS;
        _wData *= 44;
        _wData += 0x300;
    }
    else {
        _wData = AF_CENTER_POS -_wData;
        _wData *= 44;
        _wData = 0xffff - _wData;
        _wData -= 0x300;
    }

    LC898123AFDB("After cal ,  _wData = 0x%x . \n", _wData );

    return _wData;
}
#endif

inline static int moveLC898123AF(unsigned int a_u4Position)
{
    if((a_u4Position > g_u4LC898123AF_MACRO) || (a_u4Position < g_u4LC898123AF_INF))
    {
        LC898123AFDB("[LC898123AF] out of range \n");
        return -EINVAL;
    }

    printk("%s: a_u4Position = %d \n", __func__, a_u4Position);

    
    a_u4Position = a_u4Position * 2;

    if (*g_s4LC898123AF_Opened == 1)
    {
        LC898123AF_init_drv();
        spin_lock(g_LC898123AF_SpinLock);
        g_u4CurrPosition = g_u4InitPosition;
        *g_s4LC898123AF_Opened = 2;
        spin_unlock(g_LC898123AF_SpinLock);
    }

    if (g_u4CurrPosition < a_u4Position)
    {
        spin_lock(g_LC898123AF_SpinLock);    
        g_i4Dir = 1;
        spin_unlock(g_LC898123AF_SpinLock);    
    }
    else if (g_u4CurrPosition > a_u4Position)
    {
        spin_lock(g_LC898123AF_SpinLock);    
        g_i4Dir = -1;
        spin_unlock(g_LC898123AF_SpinLock);            
    }
    else   return 0; 

    spin_lock(g_LC898123AF_SpinLock);    
    g_u4TargetPosition = a_u4Position;
    g_sr = 3;
    g_i4MotorStatus = 0;
    spin_unlock(g_LC898123AF_SpinLock);    
    
	
    
    
    RamWrite32A(0xF01A, g_u4TargetPosition | 1 << 16);

    spin_lock(g_LC898123AF_SpinLock);        
    g_u4CurrPosition = (unsigned int)g_u4TargetPosition;
    spin_unlock(g_LC898123AF_SpinLock);                

    return 0;
}

inline static int setLC898123AFInf(unsigned int a_u4Position)
{
    spin_lock(g_LC898123AF_SpinLock);
    g_u4LC898123AF_INF = a_u4Position;
    spin_unlock(g_LC898123AF_SpinLock);    
    return 0;
}

inline static int setLC898123AFMacro(unsigned int a_u4Position)
{
    spin_lock(g_LC898123AF_SpinLock);
    g_u4LC898123AF_MACRO = a_u4Position;
    spin_unlock(g_LC898123AF_SpinLock);    
    return 0;    
}

inline static int setLC898123OisEnalbe(unsigned long a_enable)
{
    unsigned int ret = 0;
    printk("%s: set ois start, flag = %ld \n", __func__, a_enable);

    while(RamRead32A(0xF100, &ret), ret != 0)
        msleep(10);

    RamRead32A(0xF012, &ret);
    printk("%s: read ois enable, ret = %d, flag = %ld \n", __func__,ret,a_enable);
    if(ret == a_enable){
        printk("%s: return directly set ois success \n", __func__);
        return 0;
    }
    RamWrite32A(0xF012, a_enable);
    printk("%s: set ois success \n", __func__);

    return 0;
}

#define VCM_PREVIEW_MODE (0)
#define VCM_CAPTURE_MODE (1)
#define VCM_VIDEO_MODE      (2)

inline static int setLC898123Mode(unsigned long mode)
{
    unsigned int ret = 0;
    printk("%s: set VCM mode to %ld \n", __func__, mode);

    while(RamRead32A( 0xF100, &ret), ret != 0)
        msleep(10);

    switch(mode){
        case VCM_PREVIEW_MODE:
            RamWrite32A(0xF013, 1);
            while(RamRead32A( 0xF100, &ret), ret != 0)
                msleep(10);
            RamWrite32A(0xF012, 1);
            break;
        case VCM_CAPTURE_MODE:
            RamWrite32A(0xF013, 1);
            while(RamRead32A( 0xF100, &ret), ret != 0)
                msleep(10);
            RamWrite32A(0xF012, 1);
            break;
        case VCM_VIDEO_MODE:
            RamWrite32A(0xF012, 1);
            while(RamRead32A( 0xF100, &ret), ret != 0)
                msleep(10);
            RamWrite32A(0xF013, 0);
            break;
    }
    printk("%s: set vcm mode success \n", __func__);
    return 0;
}



int htc_ois_calibration(unsigned int cmd_flag );


long LC898123AF_Ioctl(
struct file * a_pstFile,
unsigned int a_u4Command,
unsigned long a_u4Param)
{
    int i4RetValue = 0;

    switch(a_u4Command)
    {
        case LC898123AFIOC_G_MOTORINFO :
            i4RetValue = getLC898123AFInfo((__user stLC898123AF_MotorInfo *)(a_u4Param));
        break;

        case LC898123AFIOC_T_MOVETO :
            i4RetValue = moveLC898123AF(a_u4Param);
        break;
 
        case LC898123AFIOC_T_SETINFPOS :
            i4RetValue = setLC898123AFInf(a_u4Param);
        break;

        case LC898123AFIOC_T_SETMACROPOS :
            i4RetValue = setLC898123AFMacro(a_u4Param);
        break;

        
        case LC898123AFIOC_T_OIS_ENABLE:
            printk("%s: LC898123AFIOC_T_OIS_ENABLE   OIS command = %ld", __func__, a_u4Param);
            i4RetValue = setLC898123OisEnalbe(a_u4Param);
        break;

        case LC898123AFIOC_T_SET_MODE:
            printk("%s: set vcm mode to %ld \n", __func__, a_u4Param);
            i4RetValue = setLC898123Mode(a_u4Param);
        break;

        case LC898123AFIOC_T_OIS_GYRO_CALI:
            printk("%s: LC898123AFIOC_T_OIS_GYRO_CALI   OIS command = %ld", __func__, a_u4Param);
            i4RetValue = htc_ois_calibration(a_u4Param);
        break;
        
        default :
              LC898123AFDB("[LC898123AF] No CMD \n");
            i4RetValue = -EPERM;
        break;
    }

    return i4RetValue;
}

static ssize_t ois_gyro_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    unsigned int ret_gyro_x = 0;
    unsigned int ret_gyro_y = 0;
    ssize_t ret = 0;

    RamRead32A(0x00EC, &ret_gyro_x);
    RamRead32A(0x00F0, &ret_gyro_y);

    printk("[LC898123AF] gyro:%u:%u\n", ret_gyro_x, ret_gyro_y);
	sprintf(buf, "gyro:%u:%u\n", ret_gyro_x, ret_gyro_y);
	ret = strlen(buf) + 1;
	return ret;
}

static DEVICE_ATTR(ois, 0444, ois_gyro_show, NULL);
static struct kobject *android_lc898123af_openloop;

static int ois_gyro_sysfs_init(void)
{
	int ret ;
	printk("[LC898123AF] kobject create and add\n");
    if(android_lc898123af_openloop == NULL){
        android_lc898123af_openloop = kobject_create_and_add("android_camera_lc898123af", NULL);
        if (android_lc898123af_openloop == NULL) {
            printk("[LC898123AF] subsystem_register failed\n");
            ret = -ENOMEM;
            return ret ;
        }
        printk("[LC898123AF] sysfs_create_file:android_lc898123af_openloop\n");
        ret = sysfs_create_file(android_lc898123af_openloop, &dev_attr_ois.attr);
        if (ret) {
            printk("[LC898123AF] sysfs_create_file failed.\n");
            kobject_del(android_lc898123af_openloop);
            android_lc898123af_openloop = NULL;
        }
    }
	return 0 ;
}
#if 0
static char nvr_mem[NVR_BLOCK_SIZE * 2];
static void mem_print(char*data_p, int size)
{
	int i = 0;
	printk("\n--ois_nvr dump--");
	while(size--){
		if(!(i & 0x0f))
			printk("\nois_nvr 0x%03x: ", i);
		printk("%02x ", data_p[i]);
		i++;
	}
	printk("\n----------------\n");
}

static int mem_equal(char * p1, char * p2, int size)
{
	while(size--){
		if(*p1++ != *p2++)
			return 0;
	}
	return 1;
}

static int verify_ois_otp(char *p)
{
	int i = 8;
	while(i--){
		LC898123AFDB("[LC898123AF] [%d]=0x%02x.\n", i, p[i]);
		if((i) != p[i])
			return 0;
	}
	return 1;
}

static int verify_nvr1(char *p)
{
	if(p[0] == 0x02 && p[1] == 0x3f)
		return 1;
	else
		return 0;
}

#if 0
int len_sysboot_check(char * ois_otp_buf_p)
{
	unsigned int ret = 0, flag1 = 0, flag2 = 0;
	unsigned int remap, UlReadVal;

	LC898123AFDB("[LC898123AF] sysboot check\n");
	RamWrite32A( CMD_IO_ADR_ACCESS, CVER_123) ;
	RamRead32A(  CMD_IO_DAT_ACCESS, &UlReadVal) ;
	LC898123AFDB("[LC898123AF] read chipver %x\n", UlReadVal);
	if(UlReadVal != 0xB4){
		OscStb();
		return 0;
	}

	if(ois_otp_buf_p){
		LC898123AFDB("[LC898123AF] nvr read\n");
		FlashNVR_ReadData_Byte(NVR0_ADDR, nvr_mem, NVR_BLOCK_SIZE * 2);
		mem_print(nvr_mem + NVR0_OFFSET, NVR_BLOCK_SIZE * 2);

		if(verify_ois_otp(nvr_mem)){
			LC898123AFDB("[LC898123AF] nvr0 data seems right.\n");
		}
		else{
			LC898123AFDB("[LC898123AF] !!!nvr data seems not right.\n");
		}
		if(verify_nvr1(nvr_mem + NVR_BLOCK_SIZE)){
			LC898123AFDB("[LC898123AF] nvr1 data seems right\n");
		}
		else{
			LC898123AFDB("[LC898123AF] !!!nvr1 data seems error\n");
		}

		if(verify_ois_otp(ois_otp_buf_p)){
			LC898123AFDB("[LC898123AF] ois_otp data seems right.\n");
			if(!mem_equal(ois_otp_buf_p + NVR0_OFFSET, nvr_mem + NVR0_OFFSET, NVR0_SIZE)){
				LC898123AFDB("[LC898123AF] nvr0 not match\n");
				flag1 = 1;
				memcpy(nvr_mem + NVR0_OFFSET, ois_otp_buf_p + NVR0_OFFSET, NVR0_SIZE);
			}
			else
				LC898123AFDB("[LC898123AF] nvr0 match!\n");

			if(!mem_equal(ois_otp_buf_p + NVR11_OFFSET, nvr_mem + NVR_BLOCK_SIZE, NVR11_SIZE)){
				LC898123AFDB("[LC898123AF] nvr11 not match\n");
				flag2 = 1;
				memcpy(nvr_mem + NVR_BLOCK_SIZE, ois_otp_buf_p + NVR11_OFFSET, NVR11_SIZE);
			}
			else
				LC898123AFDB("[LC898123AF] nvr11 match!\n");

			if(!mem_equal(ois_otp_buf_p + NVR1_COORDINATE_ETC_DATA_OFFSET, nvr_mem + NVR12_ADDR, NVR12_SIZE)){
				LC898123AFDB("[LC898123AF] nvr12 not match\n");
				flag2 = 1;
				memcpy(nvr_mem + NVR12_ADDR, ois_otp_buf_p + NVR1_COORDINATE_ETC_DATA_OFFSET, NVR12_SIZE);
			}
			else
				LC898123AFDB("[LC898123AF] nvr12 match!\n");

			if(flag1){
				LC898123AFDB("[LC898123AF] nvr0 writing\n");
				FlashNVRSectorErase_Byte(NVR0_ADDR);
				FlashNVR_WriteData_Byte(NVR0_ADDR, nvr_mem + NVR0_OFFSET, NVR_BLOCK_SIZE);
			}
			if(flag2){
				if(verify_nvr1(nvr_mem + NVR_BLOCK_SIZE)){
					LC898123AFDB("[LC898123AF] nvr1 writing\n");
					FlashNVRSectorErase_Byte(NVR11_ADDR);
					FlashNVR_WriteData_Byte(NVR11_ADDR, nvr_mem + NVR_BLOCK_SIZE, NVR_BLOCK_SIZE);
				}
				else{
					LC898123AFDB("[LC898123AF] otp nvr1 data error, will not write nvr1\n");
				}
			}
			if(flag1 || flag2){
				LC898123AFDB("[LC898123AF] nvr read after flash\n");
				FlashNVR_ReadData_Byte(NVR0_ADDR, nvr_mem, NVR_BLOCK_SIZE * 2);

				if(!mem_equal(ois_otp_buf_p + NVR0_OFFSET, nvr_mem + NVR0_OFFSET, NVR0_SIZE)){
					LC898123AFDB("[LC898123AF] nvr0 not match after flash!!!!!\n");
				}
				else{
					LC898123AFDB("[LC898123AF] nvr0 match\n");
				}
				if(!mem_equal(ois_otp_buf_p + NVR11_OFFSET, nvr_mem + NVR_BLOCK_SIZE, NVR11_SIZE)){
					LC898123AFDB("[LC898123AF] nvr11 not match after flash!!!!\n");
				}
				else{
					LC898123AFDB("[LC898123AF] nvr11 match\n");
				}
				if(!mem_equal(ois_otp_buf_p + NVR1_COORDINATE_ETC_DATA_OFFSET, nvr_mem + NVR12_ADDR, NVR12_SIZE)){
					LC898123AFDB("[LC898123AF] nvr12 not match after flash!!!!\n");
				}
				else{
					LC898123AFDB("[LC898123AF] nvr12 match\n");
				}
			}
		}
		else
			LC898123AFDB("[LC898123AF] !!!ois_otp data seems not right, will not write to nvram.\n");

	}
	
	RamWrite32A( CMD_IO_ADR_ACCESS, SYS_DSP_REMAP) ;
	RamRead32A(  CMD_IO_DAT_ACCESS, &remap ) ;
	LC898123AFDB("[LC898123AF] remap %d\n", remap);
	if(remap != 1) {
		printk("%s: remap = %d, start flash update \n", __func__, remap);
		ret = FlashUpdate();
		printk("%s: end flash update ret = %d\n", __func__, ret);
		RamWrite32A( CMD_IO_ADR_ACCESS, SYS_DSP_REMAP) ;
		RamRead32A(  CMD_IO_DAT_ACCESS, &remap ) ;
		LC898123AFDB("[LC898123AF] remap %d\n", remap);
		if(remap != 1) {
			printk("remap is still not 1 after update flash\n");
		}
	}
	LC898123AFDB("[LC898123AF] sysboot check end\n");
	return 0;
}
#endif
#endif

#if 0
static int LC898123AF_Open(struct inode * a_pstInode, struct file * a_pstFile)
{
    unsigned int  ret;
    unsigned int remap;
    LC898123AFDB("[LC898123AF] LC898123AF_Open - Start\n");
    if(g_s4LC898123AF_Opened)
    {    
        LC898123AFDB("[LC898123AF] the device is opened \n");
        return -EBUSY;
    }
	
	spin_lock(&g_LC898123AF_SpinLock);
    g_s4LC898123AF_Opened = 1;
    spin_unlock(&g_LC898123AF_SpinLock);
    
	
	
 
    #if 0
    if(remap != 1) {
        printk("%s: remap = %d, start flash update \n", __func__, remap);
        ret = FlashUpdate();
        printk("%s: end flash update ret = %d \n", __func__, ret);
        msleep(100);
    }
    #endif
    while(RamRead32A( 0xF100, &ret), ret != 0){
        msleep(10);
        LC898123AFDB("[LC898123AF] status not ready, wait \n");
    }
    RamWrite32A( 0xF012, 0x03) ;
    LC898123AFDB("[LC898123AF] LC898123AF_Open - End\n");
    return 0;
}
#endif

int LC898123AF_Release(struct inode * a_pstInode, struct file * a_pstFile)
{
    LC898123AFDB("[LC898123AF] LC898123AF_Release - Start\n");
	
    if (*g_s4LC898123AF_Opened == 2)
    {
        g_sr = 5;
 
#if 0
		
		RamWrite32A(0xF01A,100);
        msleep(10);
		
		RamWrite32A(0xF01A,50);
        msleep(10);    
#endif

	RamWrite32A(0xF012,0x0);
	
        
        
        
    }

    if (*g_s4LC898123AF_Opened)
    {
        LC898123AFDB("[LC898123AF] feee \n");
                                            
        spin_lock(g_LC898123AF_SpinLock);
        *g_s4LC898123AF_Opened = 0;
        spin_unlock(g_LC898123AF_SpinLock);

    }
    LC898123AFDB("[LC898123AF] LC898123AF_Release - End\n");

	if (ois_i2c_dma_va) {
		printk("[LC898123AF]   dma_free_coherent() try to free memory for DMA\n");
		dma_free_coherent(&g_pstLC898123AF_I2Cclient->dev, OIS_DMA_BUF_SIZE, ois_i2c_dma_va, ois_i2c_dma_pa);
		ois_i2c_dma_va = NULL;
		ois_i2c_dma_pa = 0;
	}
    return 0;
}


bool LC898123AF_readFlash(unsigned char* data){
	int i, index = 0;
	uint16_t addr = 0x1A00; 
	uint16_t offset = 0; 
	for(i = 0; i < 32; i++){
		offset = addr + i;
              if(offset > 0x1A0E && offset < 0x1A1A) continue; 
		
		FlashByteRead(offset, &data[index], 0);
		printk("%s read 0x%x to data[%d] = 0x%x  ", __func__, offset, index, data[index]);
		index++;
	}
       return true;
}

bool LC898123AF_readFlash_PDdata(unsigned char* data){
	int i;
	uint16_t addr = 0x1A40; 
	for (i = 0; i <= OV16880_PD_DATA_SECTOR_NUM; i++)  
	{
		FlashSectorRead_htc(addr, data+ (OV16880_SECTOR_UNIT * i));
		addr += 64;
	}
	
       return true;
}

void LC898123AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient, spinlock_t *pAF_SpinLock, int *pAF_Opened)
{

	g_pstLC898123AF_I2Cclient = pstAF_I2Cclient;
	g_LC898123AF_SpinLock = pAF_SpinLock;
	g_s4LC898123AF_Opened = pAF_Opened;

#if 0
       RamRead32A(  0x8000, &UlReadVal ) ;
       printk("[LC898123AF_SetI2Cclient] Read version flag 0x%x\n",(unsigned int) UlReadVal);
#endif

	if (ois_i2c_dma_va == NULL) {
		printk("[LC898123AF]   dma_alloc_coherent() try to alloc memory for DMA\n");
		ois_i2c_dma_va = (u8 *)dma_alloc_coherent(&g_pstLC898123AF_I2Cclient->dev, OIS_DMA_BUF_SIZE, &ois_i2c_dma_pa, GFP_KERNEL);
		if (ois_i2c_dma_va == NULL) {
			printk("[LC898123AF]   dma_alloc_coherent failed !! \n");
		}
	}

       ois_gyro_sysfs_init();
}


struct file *FILE_Open(const char *path, int flags, int mode)
{
	struct file *filp = NULL;
	mm_segment_t oldfs;
	int err = 0;

	oldfs = get_fs();
	set_fs(get_ds());
	filp = filp_open(path, flags, mode);
	set_fs(oldfs);
	if (IS_ERR(filp)) {
		err = PTR_ERR(filp);
		pr_err("File Open Error:%s",path);
		return NULL;
	}
	if(!filp->f_op){
		pr_err("File Operation Method Error!!");
		return NULL;
	}

	return filp;
}

void FILE_Close(struct file* file) {
	filp_close(file, NULL);
}

int FILE_Write(struct file *file, unsigned long long offset, unsigned char *data, unsigned int size)
{
	mm_segment_t oldfs;
	int ret;

	oldfs = get_fs();
	set_fs(get_ds());

	ret = vfs_write(file, data, size, &offset);

	set_fs(oldfs);
	return ret;
}


unsigned char htc_ext_GyroReCalib(int cam_id)
{
	UINT8	UcSndDat ;
	struct file*fp = NULL;
	uint8_t *m_path= "/data/GYRO_main_result.txt";
	uint8_t *f_path= "/data/GYRO_front_result.txt";
	char gyro_mem[1024];
	int count = 0;
	stReCalib pReCalib = {0};

       pr_info("[OIS_Cali]%s:E \n", __func__);

	
	UcSndDat = GyroReCalib(&pReCalib);
	pr_info("[OIS_Cali]%s: %d, pReCalib->SsDiffX = %d (%#x), pReCalib->SsDiffY = %d (%#x)\n", __func__, UcSndDat, pReCalib.SsDiffX, pReCalib.SsDiffX, pReCalib.SsDiffY, pReCalib.SsDiffY);

	
	if (cam_id == 0)
	{
		fp = FILE_Open(m_path, O_CREAT|O_RDWR|O_TRUNC, 0666);
	} else if (cam_id == 1)
	{
		fp = FILE_Open(f_path, O_CREAT|O_RDWR|O_TRUNC, 0666);
	} else
		pr_info("Can't write result.\n");

	if (fp != NULL)
	{
		count += sprintf(gyro_mem + count,"pReCalib->SsFctryOffX = %d (%#x), pReCalib->SsFctryOffY = %d (%#x) \n", pReCalib.SsFctryOffX, pReCalib.SsFctryOffX, pReCalib.SsFctryOffY, pReCalib.SsFctryOffY);
		count += sprintf(gyro_mem + count,"pReCalib->SsRecalOffX = %d (%#x), pReCalib->SsRecalOffY = %d (%#x) \n", pReCalib.SsRecalOffX, pReCalib.SsRecalOffX, pReCalib.SsRecalOffY, pReCalib.SsRecalOffY);
		count += sprintf(gyro_mem + count,"pReCalib->SsDiffX = %d (%#x), pReCalib->SsDiffY = %d (%#x) \n", pReCalib.SsDiffX, pReCalib.SsDiffX, pReCalib.SsDiffY, pReCalib.SsDiffY);
		FILE_Write (fp, 0, gyro_mem, strlen(gyro_mem)+1);
		FILE_Close (fp);
	}else
		pr_info("Can't write result.\n");

	if(pReCalib.SsDiffX >= 0x226 || pReCalib.SsDiffY >= 0x226 || pReCalib.SsRecalOffX >= 0x600 || pReCalib.SsRecalOffY >= 0x600)
	{
		pr_info("[OIS_Cali]%s:Threadhold check failed.\n", __func__);
		return -1;
	}
	else
		return UcSndDat;
}

unsigned char htc_ext_WrGyroOffsetData(void)
{
	UINT8	ans;
	pr_info("[OIS_Cali]%s: E\n", __func__);
	ans = WrGyroOffsetData();
	return ans;
}

void htc_ext_FlashSectorRead(unsigned char *data_ptr, UINT32 address, UINT32 blk_num)
{
	int i = 0;

	for (i = 0; i < blk_num; i++)
	{
		FlashSectorRead_htc(address, data_ptr+64*4*i);
		address += 64;
	}
	
	
	
}

#define VERNUM 0x0505050D 
#define CALID 0x00000005 
#define BASEVWNUM_M 0x000B

int htc_checkFWUpdate(void)
{
    UINT8 rc = 0;
    UINT32 UlFWDat;
    UINT32 resultID;

    pr_info("[OIS_Cali]%s VERNUM = %x\n", __func__, VERNUM);
    WitTim(100);

    RamRead32A(0x8000,&UlFWDat );
    pr_info("[OIS_Cali]%s FW Ver = %x\n", __func__, (unsigned int)UlFWDat);

    if(((UlFWDat&0xF) >= (BASEVWNUM_M&0xF))&&((VERNUM&0xF)>(UlFWDat&0xF)))
    {
		resultID = ReadCalibID();
		pr_info("[OIS_Cali]%s resultID = %x, CALID = 0x%x\n", __func__, (unsigned int)resultID, CALID);
		if(resultID == CALID || resultID == 0xFFFFFFFF)
		{
			pr_info("[OIS_Cali]%s:main camera FW update. %x -> %x", __func__, (unsigned int)UlFWDat, VERNUM);
			rc = FlashUpdateF40();
			if(rc!=0)
				pr_info("[OIS_Cali]%s:FlashUpdateM = %d  fail.", __func__, rc);
		}
    }
    else
        pr_info("[OIS_Cali]%s:FlashUpdate , no need to update.", __func__);

    pr_info("[OIS_Cali]%s rc = %d\n", __func__, rc);

    return rc;
}

int htc_ois_calibration(unsigned int cmd_flag )
{
    int rc = -1;
    int cam_id = 0; 

    pr_info("[OIS_Cali]%s:E \n", __func__);

    if (*g_s4LC898123AF_Opened == 1)
    {
        LC898123AF_init_drv();
        spin_lock(g_LC898123AF_SpinLock);
        *g_s4LC898123AF_Opened = 2;
        spin_unlock(g_LC898123AF_SpinLock);
    }

    pr_info("[OIS_Cali]%s cmd_flag=0x%x\n", __func__, cmd_flag);

    
    rc = htc_ext_GyroReCalib(cam_id);
    if (rc != 0)
          pr_err("[OIS_Cali]htc_GyroReCalib fail.\n");
    else
    {
        if(cmd_flag & 0x10)   
        {
            rc = htc_ext_WrGyroOffsetData();
            if (rc != 0)
                pr_err("[OIS_Cali]htc_WrGyroOffsetData fail.\n");
            else
                pr_info("[OIS_Cali]Gyro calibration success.\n");
        }
        else
            pr_info("[OIS_Cali]Skip OIS flash memory.\n");
    }

	return rc;
}

int htc_ois_FWupdate(void)
{
	int rc = -1;
	static int m_first = 0;

	if (m_first == 0)
	{
		pr_info("[OIS_Cali]   htc_checkFWUpdate()   Start\n");
		rc = htc_checkFWUpdate();
		pr_info("[OIS_Cali]   htc_checkFWUpdate()   End\n");
		m_first = 1;
	}

	return rc;
}

