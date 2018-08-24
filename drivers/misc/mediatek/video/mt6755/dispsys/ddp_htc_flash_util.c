#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/uaccess.h>
#include <primary_display.h>

#include "ddp_htc_util.h"
#include <linux/hrtimer.h>
#include <linux/leds.h>
 #include "../../../leds/mt6755/leds_sw.h"

#if defined(CONFIG_LM36274_SUPPORT)
#include "lm36274_i2c.h"
#endif


#define DEBUG_BUF 1024
#define MIN_COUNT 9
void mt65xx_flash_leds_brightness_set(unsigned  char level,int flash_time);
extern int mt65xx_leds_brightness_set(enum mt65xx_led_type type, enum led_brightness level);
extern unsigned char get_camera_blk(void);
extern int lp8557_write_bytes(unsigned char addr, unsigned char value);
unsigned int mt_get_bl_brightness(void);


void lcm_screen_off(void)
{
#if defined(CONFIG_MTK_I2C_LP8557)
            lp8557_write_bytes(0x0,0x00);
#endif
}

void lcm_screen_on(void)
{
#if defined(CONFIG_MTK_I2C_LP8557)
            lp8557_write_bytes(0x0,0x01);
#endif
}


void lcm_dimming_off(void)
{
     unsigned int cmd = 0x53;
     unsigned int count  = 1;
     unsigned int value = 0x24;
    primary_display_setlcm_cmd(&cmd, &count, &value);
}

void lcm_dimming_on(void)
{
     unsigned int cmd = 0x53;
     unsigned int count  = 1;
     unsigned int value = 0x2c;
    primary_display_setlcm_cmd(&cmd, &count, &value);
}

static ssize_t dsi_flash_cmd_write(
	struct file *file,
	const char __user *buff,
	size_t count,
	loff_t *ppos)
{
	char debug_buf[DEBUG_BUF];
	
	
	u32 brightness_value, flash_time;
	int cnt;
	

	if (count >= sizeof(debug_buf))
		return -EFAULT;

	if (copy_from_user(debug_buf, buff, count))
		return -EFAULT;

	
	debug_buf[count] = 0;

	
	cnt = (count) / 3 - 2;

	
	sscanf(debug_buf, "%d", &brightness_value);
	if(brightness_value > 255)
		return -EFAULT;
         if(brightness_value >= 100)
	       sscanf(debug_buf+4, "%d", &flash_time);
	else
	       sscanf(debug_buf+3, "%d", &flash_time);

         printk("brightness_value is %d, flash_time is %d\n",brightness_value,flash_time);
	mt65xx_flash_leds_brightness_set(brightness_value,flash_time);
       
	

	return count;
}

static const struct file_operations dsi_cmd_fops = {
	.write = dsi_flash_cmd_write,
};

static struct work_struct flashlightTimeOut;

static int lcd_backlight_value = 0;

#if defined(CONFIG_LM36274_SUPPORT)
static uint8_t brightness_lsb = 0;
static uint8_t brightness_msb = 0;
#endif

static struct hrtimer flashlight_OutTimer;


void mt65xx_flash_leds_brightness_restore(struct work_struct *work)
{
#if defined(CONFIG_LM36274_SUPPORT)
	lm36274_write_bytes(LM36274_BRIGHTNESS_LSB_REG, brightness_lsb);
	lm36274_write_bytes(LM36274_BRIGHTNESS_MSB_REG, brightness_msb);
	return;
#endif

#if defined(CONFIG_MTK_I2C_LP8557)
       lp8557_write_bytes(0x4,0x44);
#endif
       mt65xx_leds_brightness_set(MT65XX_LED_TYPE_LCD,lcd_backlight_value);
}

void mt65xx_cam_flash_leds_brightness_restore(void)
{
      printk("cam lcm brightness restore \n");
#if defined(CONFIG_MTK_I2C_LP8557)
       lp8557_write_bytes(0x4,0x44);
#endif
       mt65xx_leds_brightness_set(MT65XX_LED_TYPE_LCD,get_camera_blk());
}


void  Flashlight_OutTimer_cancel(void)
{
	hrtimer_cancel( &flashlight_OutTimer );
}

void mt65xx_flash_leds_brightness_set(unsigned  char level,int flash_time)
{
	 int s;
          int ms;
	ktime_t ktime;
         int bl_brightness;

	if(flash_time>1000)
	{
		s = flash_time/1000;
		ms = flash_time - s*1000;
	}
	else
	{
		s = 0;
		ms = flash_time;
	}

        lcd_backlight_value =  get_camera_blk();
         bl_brightness = mt_get_bl_brightness();
        if(lcd_backlight_value < bl_brightness)
             lcd_backlight_value = bl_brightness;

#if defined(CONFIG_LM36274_SUPPORT)
	lm36274_read_bytes((unsigned char)LM36274_BRIGHTNESS_LSB_REG, &brightness_lsb);
	lm36274_read_bytes((unsigned char)LM36274_BRIGHTNESS_MSB_REG, &brightness_msb);

	pr_info("[DISP] %s: original brightness lsb = 0x%x, msb = 0x%x\n", __FUNCTION__, brightness_lsb, brightness_msb);
#else
        printk("lcd_backlight_value = %d, current_level = %d\n",lcd_backlight_value,level);
        mt65xx_leds_brightness_set(MT65XX_LED_TYPE_LCD,255);  
#endif

 #if defined(CONFIG_MTK_I2C_LP8557)
        if(level == 75)
            lp8557_write_bytes(0x4,0xff);
        else if(level == 70)
            lp8557_write_bytes(0x4,0xee);
        else if(level == 65)
            lp8557_write_bytes(0x4,0xdd);
        else if(level == 60)
            lp8557_write_bytes(0x4,0xcc);
#endif

#if defined(CONFIG_LM36274_SUPPORT)
	if (level == 60) {
		
		lm36274_write_bytes(LM36274_BRIGHTNESS_LSB_REG, 0x07);
		lm36274_write_bytes(LM36274_BRIGHTNESS_MSB_REG, 0xFF);
	} else if (level == 20) {
		
		lm36274_write_bytes(LM36274_BRIGHTNESS_LSB_REG, 0x07);
		lm36274_write_bytes(LM36274_BRIGHTNESS_MSB_REG, 0x54);
	} else {
		pr_err("[DISP] %s: unsupported current level\n", __FUNCTION__);
	}
#endif

         if(flash_time != 0)
         {
		ktime = ktime_set( s, ms*1000000 );
		hrtimer_start( &flashlight_OutTimer, ktime, HRTIMER_MODE_REL );
         }
}

enum hrtimer_restart flashlightTimeOutCallback(struct hrtimer *timer)
{
    schedule_work(&flashlightTimeOut);
    return HRTIMER_NORESTART;
}


void htc_flash_debugfs_init(void)
{
	struct dentry *dent = debugfs_create_dir("htc_flash_backlight", NULL);

	if (IS_ERR(dent)) {
		pr_err(KERN_ERR "%s(%d): debugfs_create_dir fail, error %ld\n",
			__FILE__, __LINE__, PTR_ERR(dent));
		return;
	}

	if (debugfs_create_file("dsi_flash_cmd", 0644, dent, 0, &dsi_cmd_fops) == NULL) {
		pr_err(KERN_ERR "%s(%d): debugfs_create_file: index fail\n",
			__FILE__, __LINE__);
		return;
	}

         INIT_WORK(&flashlightTimeOut, mt65xx_flash_leds_brightness_restore);
	hrtimer_init( &flashlight_OutTimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL );
	flashlight_OutTimer.function=flashlightTimeOutCallback;

	return;
}
