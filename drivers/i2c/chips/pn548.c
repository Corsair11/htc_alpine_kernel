#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/wakelock.h>
#include <linux/of_gpio.h>
#include <linux/pn548.h>
#include <linux/types.h>
#include "pn548_mfg.h"
#include "pn548_htc.h"


#include <linux/dma-mapping.h>
#include <linux/of_irq.h>

static u8*		I2CDMABuf_va_read = NULL;
static u32*		I2CDMABuf_pa_read = NULL;
static u8*		I2CDMABuf_va_write = NULL;
static u32*		I2CDMABuf_pa_write = NULL;
#define DMA_SIZE					255

#define FTM_MODE 1
#if FTM_MODE
int mfc_nfc_cmd_result = 0;
int i2c_error_retry = 0;

static   unsigned long watchdog_counter;
static   unsigned int watchdogEn;
static   unsigned int watchdog_timeout;
#define WATCHDOG_FTM_TIMEOUT_SEC 20
#endif  

int is_debug = 0;
int is_debug_en = 0;
int s_wdcmd_cnt = 0;
int is_alive = 1;
int is_uicc_swp = 1;

#ifdef NFC_OFF_MODE_CHARGING_ENABLE
struct workqueue_struct *nfc_wq;
struct delayed_work nfc_work;
#endif


#define DBUF(buff,count) \
	if (is_debug) \
		for (i = 0; i < count; i++) \
			printk(KERN_DEBUG "[NFC] %s : [%d] = 0x%x\n", \
				__func__, i, buff[i]);

#define D(x...)	\
	if (is_debug)	\
		printk(KERN_DEBUG "[NFC] " x)
#define I(x...) printk(KERN_INFO "[NFC] " x)
#define E(x...) printk(KERN_ERR "[NFC] [Err] " x)

#define MAX_BUFFER_SIZE	512
#define PN548_RESET_CMD 	0
#define PN548_DOWNLOAD_CMD	1

#define NFC_I2C_RETRY 5
#define NFC_I2C_RETRY_ERROR_COUNT 5

struct pn548_dev	{
	struct class		*pn548_class;
	struct device		*pn_dev;
	struct device		*comn_dev;
	wait_queue_head_t	read_wq;
	struct mutex		read_mutex;
	struct wake_lock io_wake_lock;
	struct i2c_client	*client;
	struct miscdevice	pn548_device;
	unsigned long		irq_gpio;
	bool			irq_enabled;
	spinlock_t		irq_enabled_lock;
	unsigned long 		ven_gpio;
	unsigned int		ven_value;
	unsigned long 		firm_gpio;
	void (*gpio_init) (void);
	unsigned int 		ven_enable;
	int boot_mode;
	bool                     isReadBlock;
#ifdef NFC_IOEXT_PIN_CTRL_CONFIG
	u32 gpio_nfc_pwd;
#endif
};

struct pn548_dev *pn_info;
int ignoreI2C = 0;

#ifdef NFC_IOEXT_PIN_CTRL_CONFIG
static unsigned int NFC_I2C1V8_GPIO;
#endif

#ifdef NFC_IOEXT_PIN_CTRL_CONFIG
typedef struct nfc_gpio_config {
	struct pinctrl *pinctrl;
	struct pinctrl_state *pins_nfc_gpio_high;
	struct pinctrl_state *pins_nfc_gpio_low;
	struct pinctrl_state *pins_nfc_fw_high;
	struct pinctrl_state *pins_nfc_fw_low;
}nfc_gpio_config;

nfc_gpio_config g_nfcpinctrl_gpio;
#endif

#define RETRY_TIMES 9
static int pn548_RxData2(uint8_t *buf, int len)
{
    int ret;
    int i;
	struct pn548_dev *pni = pn_info;

    if (!pni->client) return -ENXIO;

    for (i = 0; i < RETRY_TIMES; i++) {
        D("%s(%d) len:%d\n", __func__, __LINE__, len);
        ret = i2c_master_recv(pni->client, (char *)I2CDMABuf_pa_read, (int)len);
        memcpy(buf, I2CDMABuf_va_read, len);
        if (unlikely(ret < 0)) {
            E("%s(%d) fail addr:0x%x ret:%d retry:%d\n", __func__, __LINE__, pni->client->addr, ret, i);
            msleep(1);
        }
        else {
            break;
        }
    }

    return ret;
}

static int pn548_TxData2(uint8_t *buf, int len)
{
    int ret;
    int i;
	struct pn548_dev *pni = pn_info;

    if (!pni->client) return -ENXIO;

    memcpy(I2CDMABuf_va_write, buf, len);

    for (i = 0; i < RETRY_TIMES; i++) {
        D("%s(%d) len:%d\n", __func__, __LINE__, len);
        ret = i2c_master_send(pni->client, (char *)I2CDMABuf_pa_write, (int)len);
        if (unlikely(ret < 0)) {
            E("%s(%d) fail addr:0x%x ret:%d retry:%d\n", __func__, __LINE__, pni->client->addr, ret, i);
            msleep(1);
        }
        else {
            break;
        }
    }

    return ret;
}


static void pn548_disable_irq(struct pn548_dev *pn548_dev)
{
	unsigned long flags;

	spin_lock_irqsave(&pn548_dev->irq_enabled_lock, flags);
	if (pn548_dev->irq_enabled) {
		disable_irq_nosync(pn548_dev->client->irq);
		pn548_dev->irq_enabled = false;
	}
	spin_unlock_irqrestore(&pn548_dev->irq_enabled_lock, flags);
}

static irqreturn_t pn548_dev_irq_handler(int irq, void *dev_id)
{
	struct pn548_dev *pn548_dev = dev_id;
	static unsigned long orig_jiffies = 0;

	if (gpio_get_value(pn548_dev->irq_gpio) == 0) {
		I("%s: irq_workaround\n", __func__);
		return IRQ_HANDLED;
	}

	pn548_disable_irq(pn548_dev);

	
	wake_up(&pn548_dev->read_wq);

	if (time_after(jiffies, orig_jiffies + msecs_to_jiffies(1000)))
		I("%s: irq=%d\n", __func__, irq);
	orig_jiffies = jiffies;

	return IRQ_HANDLED;
}

static void pn548_Enable(void)
{
	struct pn548_dev *pni = pn_info;
	unsigned int set_value = pni->ven_enable;
	I("%s: gpio=%lu set_value=%d\n", __func__, pni->ven_gpio, set_value);

	gpio_set_value(pni->ven_gpio, set_value);

	pni->ven_value = 1;
}

static void pn548_Disable(void)
{
	struct pn548_dev *pni = pn_info;
	unsigned int set_value = !pni->ven_enable;
	I("%s: gpio=%lu set_value=%d\n", __func__, pni->ven_gpio, set_value);

	gpio_set_value(pni->ven_gpio, set_value);

	pni->ven_value = 0;
}


static int pn548_isEn(void)
{
	struct pn548_dev *pni = pn_info;
	
	return pni->ven_value;
}

uint8_t read_buffer[MAX_BUFFER_SIZE];

static ssize_t pn548_dev_read(struct file *filp, char __user *buf,
		size_t count, loff_t *offset)
{
	struct pn548_dev *pni = pn_info;
	int ret;
	int val;
	int i;
	i = 0;

	D("%s: start count = %zu\n", __func__, count);

	if (count > MAX_BUFFER_SIZE) {
		E("%s : count =%zu> MAX_BUFFER_SIZE\n", __func__, count);
		count = MAX_BUFFER_SIZE;
	}

	val = gpio_get_value(pni->irq_gpio);

	D("%s: reading %zu bytes, irq_gpio = %d\n",
		__func__, count, val);

	mutex_lock(&pni->read_mutex);

	if (!gpio_get_value(pni->irq_gpio)) {
		if (filp->f_flags & O_NONBLOCK) {
			I("%s : f_flags & O_NONBLOCK read again\n", __func__);
			ret = -EAGAIN;
			goto fail;
		}

		pni->irq_enabled = true;
		enable_irq(pni->client->irq);
		D("%s: waiting read-event INT, because "
			"irq_gpio = 0\n", __func__);
		pni->isReadBlock = true;
		ret = wait_event_interruptible(pni->read_wq, gpio_get_value(pni->irq_gpio));

		pn548_disable_irq(pni);

		D("%s : wait_event_interruptible done\n", __func__);

		if (ret) {
			I("pn548_dev_read wait_event_interruptible breaked ret=%d\n", ret);
			goto fail;
		}

	}

	pni->isReadBlock = false;
    wake_lock_timeout(&pni ->io_wake_lock, IO_WAKE_LOCK_TIMEOUT);
	
	memset(read_buffer, 0, MAX_BUFFER_SIZE);
	ret = pn548_RxData2(read_buffer, count);
	mutex_unlock(&pni->read_mutex);

	if (ret < 0) {
		E("%s: i2c_master_recv returned %d\n", __func__, ret);
		return ret;
	}
	if (ret > count) {
		E("%s: received too many bytes from i2c (%d)\n",
			__func__, ret);
		return -EIO;
	}

	DBUF(read_buffer, count);

	if (copy_to_user(buf, read_buffer, count)) {
		E("%s : failed to copy to user space\n", __func__);
		return -EFAULT;
	}

	D("%s done count = %zu\n", __func__, count);
	return count;

fail:
	mutex_unlock(&pni->read_mutex);
	return ret;
}

static ssize_t pn548_dev_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *offset)
{
	struct pn548_dev *pni = pn_info;
	char buffer[MAX_BUFFER_SIZE];
	int ret;
	int i;
	i = 0;

	D("%s: start count = %zu\n", __func__, count);
	wake_lock_timeout(&pni ->io_wake_lock, IO_WAKE_LOCK_TIMEOUT);

	if (count > MAX_BUFFER_SIZE) {
		E("%s : count =%zu> MAX_BUFFER_SIZE\n", __func__, count);
		count = MAX_BUFFER_SIZE;
	}

	if ( is_debug && (s_wdcmd_cnt++ < 3))
		I("%s: writing %zu bytes\n",__func__, count);
	else {
		is_debug = 0;
		s_wdcmd_cnt = 4;
	}

	if (copy_from_user(buffer, buf, count)) {
		E("%s : failed to copy from user space\n", __func__);
		return -EFAULT;
	}

	DBUF(buffer, count);
	
	ret = pn548_TxData2(buffer, count);
	if (ret < 0) {
		E("%s : i2c_master_send returned %d\n", __func__, ret);
		ret = -EIO;
	} else {
		D("%s done count = %zu\n", __func__, count);
		return count;
	}

	return ret;
}

static int pn548_dev_open(struct inode *inode, struct file *filp)
{
	struct pn548_dev *pn548_dev = container_of(filp->private_data,
						struct pn548_dev,
						pn548_device);

	filp->private_data = pn548_dev;
	I("%s : major=%d, minor=%d\n", \
		__func__, imajor(inode), iminor(inode));

	return 0;
}

static long pn548_dev_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	struct pn548_dev *pni = pn_info;
	uint8_t buffer[] = {0x05, 0xF9, 0x04, 0x00, 0xC3, 0xE5};

	switch (cmd) {
	case PN548_SET_PWR:
		if (arg == 3) {
			
			I("%s Software reset\n", __func__);
			if (pn548_TxData2(buffer, 6) < 0)
				E("%s, SW-Reset TxData error!\n", __func__);
		} else if (arg == 2) {
			I("%s power on with firmware\n", __func__);
			pn548_Enable();
			msleep(20);
			gpio_set_value(pni->firm_gpio, 1);
			msleep(20);
			pn548_Disable();
			msleep(100);
			pn548_Enable();
			msleep(20);
		} else if (arg == 1) {
			
			I("%s power on delay100ms\n", __func__);
			gpio_set_value(pni->firm_gpio, 0);

			pn548_Enable();
			msleep(100);
			is_debug = 1;
			s_wdcmd_cnt = 0;
			I("%s pn548_Enable, set is_debug = %d, s_wdcmd_cnt : %d\n", __func__, is_debug, s_wdcmd_cnt);
		} else  if (arg == 0) {
			
			I("%s power off delay 100ms\n", __func__);
			gpio_set_value(pni->firm_gpio, 0);

			pn548_Disable();
			msleep(100);
			is_debug = is_debug_en;
			I("%s pn548_Disable, set is_debug = %d, s_wdcmd_cnt = %d\n", __func__, is_debug, s_wdcmd_cnt);
		} else {
			E("%s bad arg %lu\n", __func__, arg);
			goto fail;
		}
		break;
	default:
		E("%s bad ioctl for PN548%u\n", __func__, cmd);
		goto fail;
	}

	return 0;
fail:
	return -EINVAL;
}

static const struct file_operations pn548_dev_fops = {
	.owner	= THIS_MODULE,
	.llseek	= no_llseek,
	.read	= pn548_dev_read,
	.write	= pn548_dev_write,
	.open	= pn548_dev_open,
	.unlocked_ioctl = pn548_dev_ioctl,
	.compat_ioctl  = pn548_dev_ioctl,
};

static ssize_t pn_temp1_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int val = -1;
	struct pn548_dev *pni = pn_info;
	uint8_t buffer[MAX_BUFFER_SIZE];
	int i = 0;


	I("%s:\n", __func__);
	val = gpio_get_value(pni->irq_gpio);

	memset(buffer, 0, MAX_BUFFER_SIZE);
#if 1
	if (val == 1) {
		ret = pn548_RxData2(buffer, 33);
		if (ret < 0) {
			E("%s, i2c Rx error!\n", __func__);
		} else {
			for (i = 0; i < 10; i++)
			I("%s : [%d] = 0x%x\n", __func__, i, buffer[i]);
		}
	} else {
		E("%s, data not ready\n", __func__);
	}
#else
	if (val != 1)
		E("%s, ####### data not ready -> force to read!#########\n", __func__);

	ret = pn548_RxData(buffer, 33);
	if (ret < 0) {
		E("%s, i2c Rx error!\n", __func__);
	} else {
		for (i = 0; i < 10; i++)
		D("%s : [%d] = 0x%x\n", __func__, i, buffer[i]);
	}
#endif

	ret = sprintf(buf, "GPIO INT = %d "
		"Rx:ret=%d [0x%x, 0x%x, 0x%x, 0x%x]\n", val, ret, buffer[0], buffer[1],
		buffer[2], buffer[3]);

	return ret;
}


#define i2cw_size (20)
static ssize_t pn_temp1_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int code = -1;
	int ret = -1;
	struct pn548_dev *pni = pn_info;
	uint8_t buffer[] = {0x05, 0xF9, 0x04, 0x00, 0xC3, 0xE5};
	
	uint8_t i2cw[i2cw_size];
	uint32_t scan_data = 0;
	int i2cw_len = 0;
	int i = 0;
	char *ptr;

	sscanf(buf, "%d", &code);

	I("%s: irq = %d,  ven_gpio = %d,  firm_gpio = %d +\n", __func__, \
		gpio_get_value(pni->irq_gpio), pn548_isEn(), \
		gpio_get_value(pni->firm_gpio));
	I("%s: store value = %d\n", __func__, code);

	switch (code) {
	case 1:
			I("%s: case 1\n", __func__);
			ret = pn548_TxData2(buffer, 6);
			if (ret < 0)
				E("%s, i2c Tx error!\n", __func__);
			break;
	case 2:
			I("%s: case 2 %lu\n", __func__, pni->ven_gpio);
			pn548_Disable();
			break;
	case 3:
			I("%s: case 3 %lu\n", __func__, pni->ven_gpio);
			pn548_Enable();
			break;
	case 4:
			I("%s: case 4 %lu\n", __func__, pni->firm_gpio);
			gpio_set_value(pni->firm_gpio, 0);
			break;
	case 5:
			I("%s: case 5 %lu\n", __func__, pni->firm_gpio);
			gpio_set_value(pni->firm_gpio, 1);
			break;
	case 6:
			memset(i2cw, 0, i2cw_size);
			sscanf(buf, "%d %d", &code, &i2cw_len);
			I("%s: case 6 i2cw_len=%u\n", __func__, i2cw_len);

			ptr = strpbrk(buf, " ");	
			if (ptr != NULL) {
				for (i = 0 ; i <= i2cw_len ; i++) {
					sscanf(ptr, "%x", &scan_data);
					i2cw[i] = (uint8_t)scan_data;
					I("%s: i2cw[%d]=%x\n", \
						__func__, i, i2cw[i]);
					ptr = strpbrk(++ptr, " ");
					if (ptr == NULL)
						break;
				}

				ret = pn548_TxData2(i2cw, i2cw_len+1);
				

				if (ret < 0)
					E("%s, i2c Tx error!\n", __func__);
			} else {
				I("%s: skip no data found\n", __func__);
			}
			break;
	case 7:
			I("%s: case 7 disable i2c\n", __func__);
			ignoreI2C = 1;
			break;
	case 8:
			I("%s: case 8 enable i2c\n", __func__);
			ignoreI2C = 0;
			break;
	default:
			E("%s: case default\n", __func__);
			break;
	}

	I("%s: irq = %d,  ven_gpio = %d,  firm_gpio = %d -\n", __func__, \
		gpio_get_value(pni->irq_gpio), pn548_isEn(), gpio_get_value(pni->firm_gpio));
	return count;
}

static DEVICE_ATTR(pn_temp1, 0664, pn_temp1_show, pn_temp1_store);


static ssize_t debug_enable_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int ret = 0;
	I("debug_enable_show\n");

	ret = snprintf(buf, MAX_BUFFER_SIZE*2 , "is_debug=%d\n", is_debug);
	return ret;
}

static ssize_t debug_enable_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	sscanf(buf, "%d", &is_debug_en);
	if(is_debug_en == 0) {
	  is_debug = 0;
	  I("is_debug = 0 log disabled!\n");
	} else {
	  is_debug = 1;
	  is_debug_en = 1;
          I("is_debug = 1 log enabled!\n");
	}
	return count;
}

static DEVICE_ATTR(debug_enable, 0664, debug_enable_show, debug_enable_store);

static ssize_t nxp_chip_alive_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
	I("%s is %d\n", __func__, is_alive);
	ret = snprintf(buf, MAX_BUFFER_SIZE*2 , "%d\n", is_alive);
	return ret;
}

static DEVICE_ATTR(nxp_chip_alive, 0444, nxp_chip_alive_show, NULL);

static ssize_t nxp_uicc_swp_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int ret = 0;
	I("%s is %d\n", __func__, is_uicc_swp);
	ret = snprintf(buf, MAX_BUFFER_SIZE*2 , "%d\n", is_uicc_swp);
	return ret;
}

static ssize_t nxp_uicc_swp_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	sscanf(buf, "%d", &is_uicc_swp);
	return count;
}

static DEVICE_ATTR(nxp_uicc_swp, 0664, nxp_uicc_swp_show, nxp_uicc_swp_store);

#if FTM_MODE

static void pn548_hw_reset_control(int en_num)
{
	switch(en_num){
	case 0:
		pn548_Enable();
		msleep(50);
		pn548_Disable();
		msleep(50);
		pn548_Enable();
		msleep(50);
		pn548_Disable();
		msleep(50);
		break;
	case 1:
	default:
		pn548_Enable();
		msleep(50);
		pn548_Disable();
		msleep(50);
		pn548_Enable();
		msleep(50);
	}
}

void nfc_nci_dump_data(unsigned char *data, int len) {
	int i = 0, j = 0;
	char temp[len*7];
	memset(temp, 0x00, len*7);
	for (i = 0, j = 0; i < len; i++)
		j += snprintf(temp + j, MAX_BUFFER_SIZE*2 , " 0x%02X", data[i]);
	I("%s\r\n", temp);
}




int nci_Reader(control_msg_pack *script, unsigned int scriptSize) {
	static control_msg_pack *previous = 0;
	static int res_achieved = 0;
	static int ntf_achieved = 0;
	static char expect_resp_header[2] = {0};
	static char expect_ntf_header[2] = {0};
	uint8_t receiverBuffer[MAX_NFC_DATA_SIZE] ={0};
	char nci_read_header[2] = {0};
	unsigned char nci_data_len = 0;
	unsigned int GOID = 0;
	int rf_support_len = 0;


	if (previous != script) {
		I("new command, reset flags.\r\n");
		previous = script;
		res_achieved = 0;
		ntf_achieved = 0;
		if (script->exp_resp_content != 0) {
			if (0x20 == (script->cmd[1] & 0xF0))
				expect_resp_header[0] = script->cmd[1] + 0x20;
			else
				expect_resp_header[0] = script->cmd[1];
			expect_resp_header[1] = script->cmd[2];
			I(": 0x%02X, 0x%02X\r\n", expect_resp_header[0], expect_resp_header[1]);
		}

		if (*(script->exp_ntf) != 0) {
			if (0x20 == (script->cmd[1] & 0xF0))
				expect_ntf_header[0] = script->cmd[1] + 0x40;
			else if (0x00 == (script->cmd[1] & 0xF0))
				expect_ntf_header[0] = 0x60;
			I("Expected NTF Header: 0x%02X\r\n", expect_ntf_header[0]);
		}
	}


	if ( pn548_RxData2(nci_read_header, 2) < 0) {
		I("I2C error while read out the NCI header.\r\n");
		return -255;
	} else {
		I("1. [R] NCI header read: 0x%02X, 0x%02X\r\n", nci_read_header[0], nci_read_header[1]);
		mdelay(NFC_READ_DELAY);
		if ( pn548_RxData2(&nci_data_len, 1) < 0) {
			I("I2C error while read out the NCI data length.\r\n");
			return -255;
		} else {
			I("2. [R] NCI data length read: %d\r\n", (int)nci_data_len);
			mdelay(NFC_READ_DELAY);
			if ( pn548_RxData2(receiverBuffer, nci_data_len) < 0) {
				I("I2C error while read out the NCI data.\r\n");
				return -255;
			} else {
				I("3. [R] NCI data: ");
				nfc_nci_dump_data(receiverBuffer, (int)nci_data_len);
			}
		}
	}

	
	
	if (0x40 == (nci_read_header[0] & 0xF0)) {
		GOID = nci_read_header[0] & 0x0F;
		GOID = (GOID << 8) | nci_read_header[1];
		I("GOID: 0x%08X\r\n", GOID);

		switch (GOID) {
		case 0x0000: 
			I("Response CORE_RESET_RSP received.\r\n");
			if (*(script->exp_resp_content)) {
				if (memcmp(nci_read_header, expect_resp_header, 2) == 0){
					I("Response type matched with command. exp_len:%x\r\n", script->exp_resp_content[0]);
					if (memcmp(&script->exp_resp_content[1], receiverBuffer, script->exp_resp_content[0]) == 0) {
						I("Response matched with expected response, res_achieved set.\r\n");
						res_achieved = 1;
					} else {
						I("Not expected response! Quit now.\r\n");
						return -255;
					}
				} else {
					I("Command-Response type not matched, ignore.\r\n");
				}
			}
			gDevice_info.NCI_version = receiverBuffer[1];
			break;
		case 0x0001: 
			I("Response CORE_INIT_RSP received.\r\n");
			if (*(script->exp_resp_content)) {
				if (memcmp(nci_read_header, expect_resp_header, 2) == 0){
					I("Response type matched with command.\r\n");
					if (memcmp(&script->exp_resp_content[1], receiverBuffer, script->exp_resp_content[0]) == 0) {
						I("Response matched with expected response, res_achieved set.\r\n");
						res_achieved = 1;
					} else {
						I("Not expected response! Quit now.\r\n");
						return -255;
					}
				} else {
					I("Command-Response type not matched, ignore.\r\n");
				}
			}
			rf_support_len = receiverBuffer[5];
			gDevice_info.NFCC_Features = ((unsigned int)receiverBuffer[4]) << 24 | ((unsigned int)receiverBuffer[3]) << 16 | ((unsigned int)receiverBuffer[2]) << 8 | receiverBuffer[1];
			gDevice_info.manufactor = receiverBuffer[12 + rf_support_len];
			gDevice_info.fwVersion = ((unsigned int)receiverBuffer[15 + rf_support_len]) << 8 | ((unsigned int)receiverBuffer[16 + rf_support_len]);
			I("FW Version 0x%07lX\r\n", gDevice_info.fwVersion);
			mfc_nfc_cmd_result = (int)gDevice_info.fwVersion;
			break;
		case 0x0103: 
			I("Response RF_DISCOVER_RSP received.\r\n");
			if (*(script->exp_resp_content)) {
				if (memcmp(nci_read_header, expect_resp_header, 2) == 0){
					I("Response type matched with command.\r\n");
					if (memcmp(&script->exp_resp_content[1], receiverBuffer, script->exp_resp_content[0]) == 0) {
						I("Response matched with expected response, res_achieved set.\r\n");
						res_achieved = 1;
					} else {
						I("Not expected response! Quit now.\r\n");
						return -255;
					}
				} else {
					I("Command-Response type not matched, ignore.\r\n");
				}
			}
			if (script->cmd[5] < 0x80)
				I("Start to detect Cards.\r\n");
			else
				I("Start to listen Reader.\r\n");
			
			expect_ntf_header[1] = 0x05;
			break;
		case 0x0200: 
			I("Response NFCEE_DISCOVER_RSP received.\r\n");
			if (*(script->exp_resp_content)) {
				if (memcmp(nci_read_header, expect_resp_header, 2) == 0){
					I("Response type matched with command.\r\n");
					if (memcmp(&script->exp_resp_content[1], receiverBuffer, script->exp_resp_content[0]) == 0) {
						I("Response matched with expected response, res_achieved set.\r\n");
						res_achieved = 1;
					} else {
						I("Not expected response! Quit now.\r\n");
						return -255;
					}
				} else {
					I("Command-Response type not matched, ignore.\r\n");
				}
			}
			
			expect_ntf_header[1] = 0x00;
			break;
		case 0x0201: 
			I("Response NFCEE_MODE_SET_RSP received.\r\n");
			if (*(script->exp_resp_content)) {
				if (memcmp(nci_read_header, expect_resp_header, 2) == 0){
					I("Response type matched with command.\r\n");
					if (memcmp(&script->exp_resp_content[1], receiverBuffer, script->exp_resp_content[0]) == 0) {
						I("Response matched with expected response, res_achieved set.\r\n");
						res_achieved = 1;
					} else {
						I("Not expected response! Quit now.\r\n");
						return -255;
					}
				} else {
					I("Command-Response type not matched, ignore.\r\n");
				}
			}
			
			expect_ntf_header[1] = 0x00;
			break;

#if FTM_NFC_CPLC
#endif 
		default:
			I("Response not defined.\r\n");
			if (*(script->exp_resp_content)) {
				if (memcmp(nci_read_header, expect_resp_header, 2) == 0){
					I("Response type matched with command.\r\n");
					if (memcmp(&script->exp_resp_content[1], receiverBuffer, script->exp_resp_content[0]) == 0) {
						I("Response matched with expected response, res_achieved set.\r\n");
						res_achieved = 1;
					} else {
						I("Not expected response! Quit now.\r\n");
						return -255;
					}
				} else {
					I("Command-Response type not matched, ignore.\r\n");
				}
			} else
				I("No response requirement.\r\n");
		}
	}

	
	if (0x00 == (nci_read_header[0] & 0xF0)) {
		I("Data Packet, Connection ID:0x%02X\r\n", (nci_read_header[0] & 0x0F));
		if (*(script->exp_resp_content)) {
			if (memcmp(nci_read_header, expect_resp_header, 2) == 0){
				I("Response type matched with command.\r\n");
				if (memcmp(&script->exp_resp_content[1], receiverBuffer, script->exp_resp_content[0]) == 0) {
					I("Response matched with expected response, res_achieved set.\r\n");
					res_achieved = 1;
				} else {
					I("Not expected response! Quit now.\r\n");
					return -255;
				}
			} else {
				I("Command-Response type not matched, ignore.\r\n");
			}
		} else
			I("No response requirement.\r\n");
		if (0x00 == (nci_read_header[0] & 0xF0))
			expect_ntf_header[1] = 0x06;
	}

	
	if (0x60 == (nci_read_header[0] & 0xF0)) {
		GOID = nci_read_header[0] & 0x0F;
		GOID = (GOID << 8) | nci_read_header[1];
		I("GOID: 0x%08X\r\n", GOID);

		switch (GOID) {
		case 0x0103:
			I("Notification RF_DISCOVER_NTF received.\r\n");
			if (*(script->exp_ntf)) { 
				if (memcmp(nci_read_header, expect_ntf_header, 2) == 0){
					I("Notification type matched with command.\r\n");
					if (memcmp(&script->exp_ntf[1], receiverBuffer, script->exp_ntf[0]) == 0) {
						I("Notification matched with expected Notification, ntf_achieved set.\r\n");
						ntf_achieved = 1;
					} else {
						I("Not expected Notification! Wait for another.\r\n");
						return -1;
					}
				} else {
					
					
					gDevice_info.NTF_queue[gDevice_info.NTF_count].RF_ID = receiverBuffer[0];
					gDevice_info.NTF_queue[gDevice_info.NTF_count].RF_Protocol = receiverBuffer[1];
					gDevice_info.NTF_queue[gDevice_info.NTF_count].RF_Technology = receiverBuffer[2];
					if (gDevice_info.target_rf_id == 255 &&
					 gDevice_info.NTF_queue[gDevice_info.NTF_count].RF_Protocol == gDevice_info.protocol_set)
						gDevice_info.target_rf_id = gDevice_info.NTF_queue[gDevice_info.NTF_count].RF_ID;

					if (receiverBuffer[nci_data_len - 1] == 0) {
						I("Last INTF_NTF reached.\r\n");
						I("Card detected!\r\n");
						select_rf_target[0].cmd[4] = gDevice_info.target_rf_id;
						select_rf_target[0].cmd[5] = gDevice_info.protocol_set;
						select_rf_target[0].cmd[6] = gDevice_info.intf_set;
					}
				}
			}
			gDevice_info.NTF_count++;
			break;
		case 0x0105: 
			I("Notification RF_INTF_ACTIVATED_NTF received.\r\n");
			if (*(script->exp_ntf)) {
				if (memcmp(nci_read_header, expect_ntf_header, 2) == 0){
					I("Notification type matched with command.\r\n");
					if (memcmp(&script->exp_ntf[1], receiverBuffer, script->exp_ntf[0]) == 0) {
						I("Notification matched with expected Notification, ntf_achieved set.\r\n");
						gDevice_info.activated_INTF = receiverBuffer[0];
						ntf_achieved = 1;
					} else {
						I("Not expected Notification! Wait for another.\r\n");
						return -1;
					}
				} else {
					I("Command-Notification type not matched, ignore.\r\n");
				}
			}
			if (receiverBuffer[3] < 0x80)
				I("Card detected!\r\n");
			else
				I("Reader detected!\r\n");
			break;
		case 0x0200: 
			I("Notification NFCEE_DISCOVER_NTF received.\r\n");
			if (*(script->exp_ntf)) {
				if (memcmp(nci_read_header, expect_ntf_header, 2) == 0){
					I("Notification type matched with command.\r\n");
					if (memcmp(&script->exp_ntf[1], receiverBuffer, script->exp_ntf[0]) == 0) {
						I("Notification matched with expected Notification, ntf_achieved set.\r\n");
						ntf_achieved = 1;
					} else {
						I("Not expected Notification! Wait for another.\r\n");
						return -1;
					}
				} else {
					I("Command-Notification type not matched, ignore.\r\n");
				}
			}
			gDevice_info.HW_model = receiverBuffer[1];
			break;
#if FTM_NFC_CPLC
#endif 
		default:
			I("Notification not defined.\r\n");
			if (*(script->exp_ntf)) {
				if (memcmp(nci_read_header, expect_ntf_header, 1) == 0){
					I("Notification type matched with command.\r\n");
					if (memcmp(&script->exp_ntf[1], receiverBuffer, script->exp_ntf[0]) == 0) {
						I("Notification matched with expected Notification, ntf_achieved set.\r\n");
						ntf_achieved = 1;
					} else {
						I("Not expected Notification! Wait for another.\r\n");
						return -1;
					}
				} else {
					I("Command-Notification type not matched, ignore.\r\n");
				}
			} else
				I("No Notification requirement.\r\n");
		}
	}

	if (*(script->exp_resp_content) != 0) {
		if (res_achieved) {
			if (*(script->exp_ntf) != 0) {
				if (ntf_achieved) {
					return 1;
				} else {
					I("Notification requirement not achieve, stay at current command.\r\n");
					if (watchdogEn == 1)
						watchdog_counter = 0;

					return -1;
				}
			} else {
				I("No NTF requirement, step to next command.\r\n");
				return 1;
			}
		} else {
			I("Response requirement not achieve, stay at current command.\r\n");

			if (watchdogEn == 1)
				watchdog_counter = 0;

			return -1;
		}
	} else if (*(script->exp_ntf) != 0) {
		if (ntf_achieved) {
			return 1;
		} else {
			I("Notification requirement not achieve, stay at current command.\r\n");

			if (watchdogEn == 1)
				watchdog_counter = 0;

			return -1;
		}
	} else {
		I("No requirement, step to next command.\r\n");
		return 1;
	}
}

#define CHECK_READER(void) \
do { \
	if (watchdogEn == 1) {\
		if (watchdog_counter++ > ((2000 / NFC_READ_DELAY) * watchdog_timeout)) {\
			I("watchdog timeout, command fail.\r\n");\
			goto TIMEOUT_FAIL; \
		} \
	} \
	if (gpio_get_value(pni->irq_gpio)) { \
		reader_resp = nci_Reader(&script[scriptIndex], scriptSize); \
		 \
		switch(reader_resp) { \
		case -255: \
			 \
			goto I2C_FAIL; \
			break; \
		case -1: \
			 \
			break; \
		case 0: \
			 \
			scriptIndex = 0; \
			break; \
		case 1: \
			 \
			scriptIndex++; \
			break; \
		default: \
			scriptIndex = reader_resp; \
		} \
	} \
} while(0)

int script_processor(control_msg_pack *script, unsigned int scriptSize) {
	int ret;
	int scriptIndex, reader_resp;
	int last_scriptIndex;
	struct pn548_dev *pni = pn_info;

	scriptSize = scriptSize/sizeof(control_msg_pack);

	I("script_processor script size: %d.\r\n", scriptSize);

	scriptIndex = 0;
	last_scriptIndex = -1;
	reader_resp = 1;

	do {
		if (reader_resp == -1) {
			CHECK_READER();
			mdelay(NFC_READ_DELAY);

			if (watchdogEn == 1)
				if (watchdog_counter++ > ((2000 / NFC_READ_DELAY) * watchdog_timeout)) {
					I("watchdog timeout, command fail.\r\n");
					goto TIMEOUT_FAIL;
				}

			continue;
		}

		if ( last_scriptIndex != scriptIndex) {
			I("script_processor pn548_TxData2()+\r\n");
			ret = pn548_TxData2(&script[scriptIndex].cmd[1], (int)script[scriptIndex].cmd[0]);
			I("script_processor pn548_TxData2()-\r\n");
			if (ret < 0) {
				E("%s, i2c Tx error!\n", __func__);
				nfc_nci_dump_data(&script[scriptIndex].cmd[1], (int)script[scriptIndex].cmd[0]);
				goto I2C_FAIL;
				break;
			}
			else {
					I("[W] i2c wrote: ");
					nfc_nci_dump_data(&script[scriptIndex].cmd[1], (int)script[scriptIndex].cmd[0]);
					mdelay(NFC_READ_DELAY + 20);
					last_scriptIndex = scriptIndex;
					I("script_processor CHECK_IRQ value :%d\r\n", gpio_get_value(pni->irq_gpio));
					CHECK_READER();
				}
		} else {
			CHECK_READER();

			if (watchdogEn == 1)
				if (watchdog_counter++ > ((2000 / NFC_READ_DELAY) * watchdog_timeout)) {
					I("watchdog timeout, command fail.\r\n");
					goto TIMEOUT_FAIL;
				}

		}
		mdelay(NFC_READ_DELAY);
	} while(scriptIndex < scriptSize);

	return 0;
I2C_FAIL:
	E("%s, I2C_FAIL!\n", __func__);
	mfc_nfc_cmd_result = -2;
	return 1;
TIMEOUT_FAIL:
	mfc_nfc_cmd_result = 0;
	return 1;
}

static ssize_t mfg_nfc_ctrl_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int ret = 0;
	I("%s mfc_nfc_cmd_result is %d\n", __func__, mfc_nfc_cmd_result);
	ret = sprintf(buf, "%d\n\n", mfc_nfc_cmd_result);
	return ret;
}

#ifdef NFC_OFF_MODE_CHARGING_ENABLE
static void pn548_process_irq(struct work_struct *work) {
	struct pn548_dev *pni = pn_info;
	char nci_read_header[2] = {0};
	uint8_t receiverBuffer[MAX_NFC_DATA_SIZE] ={0};
	unsigned char nci_data_len = 0;
	int ret;
	while(1) {
		mutex_lock(&pni->read_mutex);
		if (!gpio_get_value(pni->irq_gpio)) {
			pni->irq_enabled = true;
			enable_irq(pni->client->irq);
			D("%s: waiting read-event INT, because "
				"irq_gpio = 0\n", __func__);
			pni->isReadBlock = true;
			ret = wait_event_interruptible(pni->read_wq,
				gpio_get_value(pni->irq_gpio));
			pn548_disable_irq(pni);
			D("%s : wait_event_interruptible done\n", __func__);
			if (ret) {
			I("pn548_dev_read wait_event_interruptible breaked ret=%d\n", ret);
			}
		}
		pni->isReadBlock = false;
		wake_lock_timeout(&pni ->io_wake_lock, IO_WAKE_LOCK_TIMEOUT);
		mutex_unlock(&pni->read_mutex);
		if ( pn548_RxData2(nci_read_header, 2) < 0) {
			E("I2C error while read out the NCI header.\r\n");
			break;
		} else {
			I("1. [R] NCI header read: 0x%02X, 0x%02X\r\n", nci_read_header[0], nci_read_header[1]);
			mdelay(NFC_READ_DELAY);
			if ( pn548_RxData2(&nci_data_len, 1) < 0) {
				E("I2C error while read out the NCI data length.\r\n");
				break;
			} else {
				I("2. [R] NCI data length read: %d\r\n", (int)nci_data_len);
				mdelay(NFC_READ_DELAY);
				if ( pn548_RxData2(receiverBuffer, nci_data_len) < 0) {
					E("I2C error while read out the NCI data.\r\n");
					break;
				} else {
					I("3. [R] NCI data: ");
					nfc_nci_dump_data(receiverBuffer, (int)nci_data_len);
				}
			}
		}
	}
}
#endif



static int mfg_nfc_test(int code)
{
#ifdef NFC_IOEXT_PIN_CTRL_CONFIG
	struct pn548_dev *pni = pn_info;
#endif
	gDevice_info.NTF_count = 0;
	memset(gDevice_info.NTF_queue, 0x00, sizeof(gDevice_info.NTF_queue));
	gDevice_info.protocol_set = 4;
	gDevice_info.intf_set = 2;
	gDevice_info.target_rf_id = 255;
	mfc_nfc_cmd_result = -1;
	watchdog_counter = 0;
	watchdogEn = 1;
	I("%s: store value = %d\n", __func__, code);

	switch (code) {
	case 0:  
		I("%s: get nfcversion :\n", __func__);
		pn548_hw_reset_control(1);
		if (script_processor(nfc_version_script, sizeof(nfc_version_script)) == 0) {
			I("%s: store value = %d\n", __func__, code);
		}
		pn548_hw_reset_control(0);
		break;
	case 1:  
		I("%s: nfcreader test :\n", __func__);
		pn548_hw_reset_control(1);
#ifdef NFC_PLL_CLOCK_ENABLE
		if (script_processor(nfc_reader_clk_pll_script, sizeof(nfc_reader_clk_pll_script)) == 0) {
			I("%s: store value = %d USED PLL CLOCK\n", __func__, code);
			mfc_nfc_cmd_result = 1;
		}
#else
		if (script_processor(nfc_reader_script, sizeof(nfc_reader_script)) == 0) {
			I("%s: store value = %d\n", __func__, code);
			mfc_nfc_cmd_result = 1;
		}
#endif
		pn548_hw_reset_control(0);
		break;
	case 2:  
		I("%s: nfccard test :\n", __func__);
		pn548_hw_reset_control(1);
#ifdef NFC_PLL_CLOCK_ENABLE
		if (script_processor(nfc_card_clk_pll_script, sizeof(nfc_card_clk_pll_script)) == 0) {
			I("%s: store value = %d USED PLL CLOCK\n", __func__, code);
			mfc_nfc_cmd_result = 1;
		}
#else
		if (script_processor(nfc_card_script, sizeof(nfc_card_script)) == 0) {
			I("%s: store value = %d\n", __func__, code);
			mfc_nfc_cmd_result = 1;
		}
#endif
		pn548_hw_reset_control(0);
		break;
	case 3:
		I("%s: nfccard test 2 :\n", __func__);
		pn548_hw_reset_control(1);
		if (script_processor(nfc_card_script_plm, sizeof(nfc_card_script_plm)) == 0) {
			I("%s: store value = %d\n", __func__, code);
			mfc_nfc_cmd_result = 1;
		}
		pn548_hw_reset_control(0);
		break;
#if FTM_NFC_CPLC
#endif  
       case 88:
#ifdef NFC_OFF_MODE_CHARGING_ENABLE
		pn548_hw_reset_control(1);
		I("%s: off_mode_charging script :\n", __func__);
		if (script_processor(nfc_off_mode_charging_enble_script, sizeof(nfc_off_mode_charging_enble_script)) == 0) {
			I("%s: store value = %d\n", __func__, code);
			mfc_nfc_cmd_result = 1;
			INIT_DELAYED_WORK(&nfc_work, pn548_process_irq);
			nfc_wq = create_singlethread_workqueue("htc_nfc");
			queue_delayed_work(nfc_wq, &nfc_work, 0);
			mdelay(1);
			I("%s: off_mode_charging complete!!\n",__func__);
		} else {
			E("%s: off_mode_charging fail!!\n",__func__);
			pn548_hw_reset_control(0);
		}
#endif
		break;
	case 99:
		I("%s: nfc_standby_enble_script :\n", __func__);
		pn548_hw_reset_control(1);
		if (script_processor(nfc_standby_enble_script, sizeof(nfc_standby_enble_script)) == 0) {
			I("%s: store value = %d\n", __func__, code);
			mfc_nfc_cmd_result = 1;
		}
		pn548_hw_reset_control(0);
#ifndef NFC_IOEXT_PIN_CTRL_CONFIG
		pn548_htc_turn_off_pvdd();
#else
	if(pni->gpio_nfc_pwd)
	{
		gpio_set_value(pni->gpio_nfc_pwd, 0);
		I("%s : cmd-99, set pvdd off, gpio_nfc_pwd : %u, chk-value nfc_pvdd:%d\n", __func__, pni->gpio_nfc_pwd, gpio_get_value(pni->gpio_nfc_pwd));
	}
#endif
		break;
	default:
		E("%s: case default do nothing\n", __func__);
		break;
	}
	I("%s: END\n", __func__);
	return 0;
}

static ssize_t mfg_nfc_ctrl_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	int code = -1;
	sscanf(buf, "%d", &code);
	ret = mfg_nfc_test(code);
	return count;
}

static ssize_t mfg_nfc_timeout_store(struct device *dev,
                                struct device_attribute *attr,
                                const char *buf, size_t count)
{
        int input_timeout = -1;
        sscanf(buf, "%d", &input_timeout);
	if (input_timeout >= 1) {
		watchdog_timeout = input_timeout;
	}
	else {
		watchdog_timeout = WATCHDOG_FTM_TIMEOUT_SEC;
	}
        return count;
}

static ssize_t mfg_nfc_timeout_show(struct device *dev,
                        struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE,"%d\n", watchdog_timeout);
}



static ssize_t mfg_nfcversion(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int ret;
	watchdogEn = 1;
	I("%s watchdogEn is %d\n", __func__, watchdogEn);
	ret = mfg_nfc_test(0);
	if (mfc_nfc_cmd_result > 0) {
		return scnprintf(buf, PAGE_SIZE,
			"NFC firmware version: 0x%07x\n", mfc_nfc_cmd_result);
	}
	else if (mfc_nfc_cmd_result == 0) {
		return scnprintf(buf, PAGE_SIZE,
			"%s watchdog timeout fail\n",__func__);
	}
	else {
		return scnprintf(buf, PAGE_SIZE,
			"%s Fail %d\n",__func__,mfc_nfc_cmd_result);
	}
}


static ssize_t mfg_nfcreader(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int ret;
	watchdogEn = 1;
	I("%s watchdogEn is %d\n", __func__, watchdogEn);
	ret = mfg_nfc_test(1);
	if (mfc_nfc_cmd_result == 1) {
		return scnprintf(buf, PAGE_SIZE,
			"%s Succeed\n",__func__);
	}
	else if (mfc_nfc_cmd_result == 0) {
		return scnprintf(buf, PAGE_SIZE,
			"%s watchdog timeout fail\n",__func__);
	}
	else {
		return scnprintf(buf, PAGE_SIZE,
			"%s Fail %d\n",__func__,mfc_nfc_cmd_result);
	}
}



static ssize_t mfg_nfccard(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int ret;
	watchdogEn = 1;
	I("%s watchdogEn is %d\n", __func__, watchdogEn);
	ret = mfg_nfc_test(2);
	if (mfc_nfc_cmd_result == 1) {
		return scnprintf(buf, PAGE_SIZE,
			"%s Succeed\n",__func__);
	}
	else if (mfc_nfc_cmd_result == 0) {
		return scnprintf(buf, PAGE_SIZE,
			"%s watchdog timeout fail\n",__func__);
	}
	else {
		return scnprintf(buf, PAGE_SIZE,
			"%s Fail %d\n",__func__,mfc_nfc_cmd_result);
	}
}


static DEVICE_ATTR(mfg_nfc_ctrl, 0660, mfg_nfc_ctrl_show, mfg_nfc_ctrl_store);
static DEVICE_ATTR(mfg_nfcversion, 0440, mfg_nfcversion, NULL);
static DEVICE_ATTR(mfg_nfcreader, 0440, mfg_nfcreader, NULL);
static DEVICE_ATTR(mfg_nfccard, 0440, mfg_nfccard, NULL);
static DEVICE_ATTR(mfg_nfc_timeout, 0660, mfg_nfc_timeout_show, mfg_nfc_timeout_store);
#endif  

static int pn548_parse_dt(struct device *dev, struct pn548_i2c_platform_data *pdata)
{
	int ret;
	struct property *prop;
	struct device_node *dt = dev->of_node;

	u32 buf = 0;
	I("%s: Start\n", __func__);
	prop = of_find_property(dt, "nxp,ven_isinvert", NULL);
	if (prop) {
		of_property_read_u32(dt, "nxp,ven_isinvert", &pdata->ven_isinvert);
		printk(KERN_INFO "[NFC] %s:ven_isinvert = %d", __func__, pdata->ven_isinvert);
	} else {
		goto parse_error;
	}

	prop = of_find_property(dt, "nxp,isalive", NULL);
	if (prop) {
		of_property_read_u32(dt, "nxp,isalive", &is_alive);
		is_alive = pn548_htc_check_rfskuid(is_alive);
		printk(KERN_INFO "[NFC] %s:is_alive = %d", __func__, is_alive);
	} else {
		goto parse_error;
	}

	
	prop = of_find_property(dt, "nxp,irq-gpio", NULL);
	if (prop) {
		of_property_read_u32(dt, "nxp,irq-gpio", &buf);
		pdata->irq_gpio = buf;
		D("%s: irq_gpio = %lu\n", __func__, pdata->irq_gpio);
	} else
		I("%s: irq_gpio not found", __func__);
	if (!gpio_is_valid(pdata->irq_gpio))
		E("DT:irq_gpio is not valid\n");
	else
		I("DT:irq_gpio=%lu\n", pdata->irq_gpio);

	prop = of_find_property(dt, "nxp,ven-gpio", NULL);
	if (prop) {
		of_property_read_u32(dt, "nxp,ven-gpio", &buf);
		pdata->ven_gpio = buf;
		D("%s: ven_gpio = %lu\n", __func__, pdata->ven_gpio);
	} else
		I("%s: ven_gpio not found", __func__);
	if (!gpio_is_valid(pdata->ven_gpio))
		E("DT:ven_gpio is not valid\n");
	else
		I("DT:ven_gpio=%lu\n", pdata->ven_gpio);

#ifdef NFC_IOEXT_PIN_CTRL_CONFIG
	prop = of_find_property(dt, "nxp,1v8-gpio", NULL);
	if (prop) {
		of_property_read_u32(dt, "nxp,1v8-gpio", &buf);
		NFC_I2C1V8_GPIO = buf;
		D("%s: nfc_1v8_gpio = %u\n", __func__, NFC_I2C1V8_GPIO);
	} else
		I("%s: nfc_1v8_gpio not found", __func__);
	if (!gpio_is_valid(NFC_I2C1V8_GPIO))
		E("DT:nfc_1v8_gpio is not valid\n");
	else
		I("DT:nfc_1v8_gpio=%u\n", NFC_I2C1V8_GPIO);

	pdata->firm_gpio = of_get_named_gpio(dt, "nxp,fwdl-gpio", 0);
	pdata->gpio_nfc_pwd = of_get_named_gpio(dt, "nxp,pvdd-gpio", 0);
	I("%s: DT: check ioext pvdd_gpio:%u, firm_gpio:%lu\n", __func__, pdata->gpio_nfc_pwd, pdata->firm_gpio);
#else
	prop = of_find_property(dt, "nxp,fwdl-gpio", NULL);
	if (prop) {
		of_property_read_u32(dt, "nxp,fwdl-gpio", &buf);
		pdata->firm_gpio = buf;
		D("%s: firm_gpio = %lu\n", __func__, pdata->firm_gpio);
	} else
		I("%s: firm_gpio not found", __func__);

	if (!gpio_is_valid(pdata->firm_gpio))
		E("DT:firm_gpio is not valid\n");
	else
		I("DT:firm_gpio=%lu\n", pdata->firm_gpio);
#endif

	ret = pn548_htc_parse_dt(dev);
	if(!ret) {
		goto parse_error;
	}

	D("%s: End, irq_gpio:%lu, ven_gpio:%lu, firm_gpio:%lu\n", __func__, pdata->irq_gpio, pdata->ven_gpio,pdata->firm_gpio);
	return 0;
parse_error:
        return 1;
}

#ifdef NFC_GPIO_PINCTRL_ENABLE
static int pn548_pinctrl_init(struct device *dev)
{
	struct pinctrl *pn548_pinctrl;
	struct pinctrl_state *pn548_default;
	int ret = 0;

	pn548_pinctrl = devm_pinctrl_get(dev);
	if(IS_ERR(pn548_pinctrl)) {
		ret = PTR_ERR(pn548_pinctrl);
		E("Can't get pn548 pinctrl.\n");
		return ret;
	}
	pn548_default = pinctrl_lookup_state(pn548_pinctrl, "pn548_pins_default");
	if(IS_ERR(pn548_default)) {
		ret = PTR_ERR(pn548_default);
		E("Can't find pn548_pins_default state.\n");
		return ret;
	}
	ret = pinctrl_select_state(pn548_pinctrl, pn548_default);
	if(ret < 0) {
		E("pn548 setting default pins state failed!\n");
	}
	return ret;
}
#endif

static int pn548_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret;
	struct pn548_i2c_platform_data *platform_data;
	struct pn548_dev *pni;
	struct device_node *node = NULL;
	u32 ints[2] = {0,0};

	I("%s() Start\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		E("%s : need I2C_FUNC_I2C\n", __func__);
		return  -ENODEV;
	}

	if (client->dev.of_node) {
		 platform_data = kzalloc(sizeof(*platform_data), GFP_KERNEL);
		 if (platform_data == NULL) {
			 E("%s : nfc probe fail because platform_data \
				 is NULL\n", __func__);
			 return  -ENODEV;
		 }

		 #ifdef NFC_GPIO_PINCTRL_ENABLE
		 ret = pn548_pinctrl_init(&client->dev);
		 if(ret < 0) {
			E("Fail to init pn548 pins!\n");
			ret = -ENODEV;
			goto err_exit;
		 }
		 #endif

		 ret = pn548_parse_dt(&client->dev, platform_data);
		 if (ret) {
	                 E("%s : pn548_parse_dt fail\n", __func__);
	                 ret = -ENODEV;
	                 goto err_exit;
	         }
	} else {
		 platform_data = client->dev.platform_data;
		 if (platform_data == NULL) {
			 E("%s : nfc probe fail because platform_data \
				 is NULL\n", __func__);
			 return  -ENODEV;
		 }
	}

#ifdef NFC_IOEXT_PIN_CTRL_CONFIG
	g_nfcpinctrl_gpio.pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(g_nfcpinctrl_gpio.pinctrl)) {
		ret = PTR_ERR(g_nfcpinctrl_gpio.pinctrl);
		E("Cannot find htc nfc pinctrl!\n");
	}

	g_nfcpinctrl_gpio.pins_nfc_gpio_high = pinctrl_lookup_state(g_nfcpinctrl_gpio.pinctrl, "nfc-gpio_high");
    if (IS_ERR(g_nfcpinctrl_gpio.pins_nfc_gpio_high)) {
		g_nfcpinctrl_gpio.pins_nfc_gpio_high = (struct pinctrl_state *)NULL;
		ret = PTR_ERR(g_nfcpinctrl_gpio.pins_nfc_gpio_high);
		E("Cannot find htc nfc pinctrl pins_nfc_gpio_high!\n");
	}

	g_nfcpinctrl_gpio.pins_nfc_gpio_low = pinctrl_lookup_state(g_nfcpinctrl_gpio.pinctrl, "nfc-gpio_low");
    if (IS_ERR(g_nfcpinctrl_gpio.pins_nfc_gpio_low)) {
		g_nfcpinctrl_gpio.pins_nfc_gpio_low = (struct pinctrl_state *)NULL;
		ret = PTR_ERR(g_nfcpinctrl_gpio.pins_nfc_gpio_low);
		E("Cannot find htc nfc pinctrl pins_nfc_gpio_low!\n");
	}

	g_nfcpinctrl_gpio.pins_nfc_fw_high = pinctrl_lookup_state(g_nfcpinctrl_gpio.pinctrl, "nfc-fw_high");
    if (IS_ERR(g_nfcpinctrl_gpio.pins_nfc_fw_high)) {
		g_nfcpinctrl_gpio.pins_nfc_fw_high = (struct pinctrl_state *)NULL;
		ret = PTR_ERR(g_nfcpinctrl_gpio.pins_nfc_fw_high);
		E("Cannot find htc nfc pinctrl pins_nfc_fw_high!\n");
	}

	g_nfcpinctrl_gpio.pins_nfc_fw_low = pinctrl_lookup_state(g_nfcpinctrl_gpio.pinctrl, "nfc-fw_low");
    if (IS_ERR(g_nfcpinctrl_gpio.pins_nfc_fw_low)) {
		g_nfcpinctrl_gpio.pins_nfc_fw_low = (struct pinctrl_state *)NULL;
		ret = PTR_ERR(g_nfcpinctrl_gpio.pins_nfc_fw_low);
		E("Cannot find htc nfc pinctrl pins_nfc_fw_low!\n");
	}

	if(g_nfcpinctrl_gpio.pinctrl != NULL) {
		if(g_nfcpinctrl_gpio.pins_nfc_gpio_high != NULL) {
			pinctrl_select_state(g_nfcpinctrl_gpio.pinctrl, g_nfcpinctrl_gpio.pins_nfc_gpio_high);
		}

		if(g_nfcpinctrl_gpio.pins_nfc_fw_low != NULL) {
			pinctrl_select_state(g_nfcpinctrl_gpio.pinctrl, g_nfcpinctrl_gpio.pins_nfc_fw_low);
		}
	}

	if(platform_data->gpio_nfc_pwd > 0) {
		I("gpio_request(gpio_nfc_pwd)\n");
		gpio_request(platform_data->gpio_nfc_pwd, "nfc_pvdd_pin");
		I("gpio_direction_output(gpio_nfc_pwd, 1)\n");
		gpio_direction_output(platform_data->gpio_nfc_pwd, 1);
	}
#endif


#ifndef NFC_IOEXT_PIN_CTRL_CONFIG
	if(!pn548_htc_turn_on_pvdd(client)) {
		ret = -ENODEV;
		goto err_exit;
	}
#endif


	#ifdef NFC_PLL_CLOCK_ENABLE
        
	ret = pn548_pll_clk_ctrl(1);
	if (!ret) {
		E("%s : Enable NFC_PLL_CLK fail\n", __func__);
		ret = -ENODEV;
		goto err_exit;
	}
	#endif

	
	ret = gpio_request(platform_data->irq_gpio, "nfc_int");
	if (ret) {
		E("%s : request gpio%lu fail\n",
			__func__, platform_data->irq_gpio);
		ret = -ENODEV;
		goto err_exit;
	}

	
	ret = gpio_request(platform_data->ven_gpio, "nfc_en");
	if (ret) {
		E("%s : request gpio %lu fail\n",
			__func__, platform_data->ven_gpio);
		ret = -ENODEV;
		goto err_request_gpio_ven;
	}
	

	ret = gpio_request(platform_data->firm_gpio, "nfc_firm");
	if (ret) {
		E("%s : request gpio %lu fail\n",
			__func__, platform_data->firm_gpio);
		ret = -ENODEV;
		goto err_request_gpio_firm;
	}

	#ifdef NFC_GPIO_PINCTRL_ENABLE
	if (pn548_htc_check_pvdd_gpio()) {
		ret = gpio_request(pn548_htc_get_pvdd_gpio(), "nfc_pvdd");
		if (ret) {
			E("%s : request gpio %u fail\n",
				__func__, pn548_htc_get_pvdd_gpio());
			ret = -ENODEV;
			goto err_request_gpio_pvdd;
		}
	}
	#endif

	pni = kzalloc(sizeof(struct pn548_dev), GFP_KERNEL);
	if (pni == NULL) {
		dev_err(&client->dev, \
				"pn548_probe : failed to allocate \
				memory for module data\n");
		ret = -ENOMEM;
		goto err_kzalloc;
	}

	pn_info = pni;

	if (platform_data->gpio_init != NULL) {
		I("%s: gpio_init\n", __func__);
		platform_data->gpio_init();
	}

	pni->irq_gpio = platform_data->irq_gpio;
	pni->ven_gpio  = platform_data->ven_gpio;
	pni->firm_gpio  = platform_data->firm_gpio;
#ifdef NFC_IOEXT_PIN_CTRL_CONFIG
	pni->gpio_nfc_pwd = platform_data->gpio_nfc_pwd;
#endif
	pni->client   = client;
	pni->client->addr = (pni->client->addr & I2C_MASK_FLAG)| I2C_DMA_FLAG;
	pni->client->timing = 400;
	pni->gpio_init = platform_data->gpio_init;
	pni->ven_enable = !platform_data->ven_isinvert;
	pni->boot_mode = pn548_htc_get_bootmode();
	pni->isReadBlock = false;
	I("%s : irq_gpio:%lu, ven_gpio:%lu, firm_gpio:%lu, ven_enable:%d\n", __func__, pni->irq_gpio, pni->ven_gpio, pni->firm_gpio, pni->ven_enable);

	ret = gpio_direction_input(pni->irq_gpio);
	I("%s : irq_gpio set input %d \n", __func__,ret);
	ret = gpio_direction_output(pni->ven_gpio, 0);
	I("%s : ven_gpio set 0 %d \n", __func__,ret);
	ret = gpio_direction_output(pni->firm_gpio, 0);
	I("%s : firm_gpio set 0 %d \n", __func__,ret);

	I("%s(), dma_alloc_coherent()\n", __func__);

	I2CDMABuf_va_read = (u8 *)dma_alloc_coherent(&pni->client->adapter->dev, DMA_SIZE, (dma_addr_t *)&I2CDMABuf_pa_read, GFP_KERNEL);
	if (I2CDMABuf_va_read == NULL) {
		I2CDMABuf_pa_read = NULL;
		pr_err("%s: allocate phsical memory error\n", __func__);
		return -ENOMEM;
	}

	I2CDMABuf_va_write = (u8 *)dma_alloc_coherent(&pni->client->adapter->dev, DMA_SIZE, (dma_addr_t *)&I2CDMABuf_pa_write, GFP_KERNEL);
	if (I2CDMABuf_va_write == NULL) {
		I2CDMABuf_pa_write = NULL;
		pr_err("%s: allocate phsical memory error\n", __func__);
		return -ENOMEM;
	}

	

	
	init_waitqueue_head(&pni->read_wq);
	mutex_init(&pni->read_mutex);
	spin_lock_init(&pni->irq_enabled_lock);

	I("%s: init io_wake_lock\n", __func__);
	wake_lock_init(&pni->io_wake_lock, WAKE_LOCK_SUSPEND, PN548_I2C_NAME);

	pni->pn548_device.minor = MISC_DYNAMIC_MINOR;
	pni->pn548_device.name = "pn544";
	pni->pn548_device.fops = &pn548_dev_fops;

#if 1
	ret = misc_register(&pni->pn548_device);
#else
	ret = 0;
#endif
	if (ret) {
		E("%s : misc_register failed\n", __FILE__);
		goto err_misc_register;
	}

	pni->irq_enabled = true;

	I("%s: request_irq!!!!!\n", __func__);

	node = of_find_compatible_node(NULL, NULL, "mediatek, IRQ_NFC-eint");
	if(node){
	  of_property_read_u32_array(node , "debounce", ints, ARRAY_SIZE(ints));
	  gpio_set_debounce(ints[0], ints[1]);
	
	  client->irq = irq_of_parse_and_map(node, 0);
	
	  ret = request_irq(client->irq, pn548_dev_irq_handler, IRQF_TRIGGER_HIGH, "IRQ_NFC-eint", pni);
	  if(ret > 0){
		ret = -1;
		E("request_irq IRQ LINE NOT AVAILABLE!.\n");
		goto err_request_irq_failed;
	  }
	}else{
	  E("request_irq can not find touch eint device node!.\n");
	  ret = -1;
	  goto err_request_irq_failed;
	}

	I("%s: pn548_disable_irq(), irq:%d \n", __func__,pni->client->irq);
	pn548_disable_irq(pni);

	i2c_set_clientdata(client, pni);

	pni->pn548_class = class_create(THIS_MODULE, "NFC_sensor");
	if (IS_ERR(pni->pn548_class)) {
		ret = PTR_ERR(pni->pn548_class);
		pni->pn548_class = NULL;
		E("%s : class_create failed\n", __func__);
		goto err_create_class;
	}

	pni->pn_dev = device_create(pni->pn548_class, NULL, 0, "%s", "pn544");
	if (unlikely(IS_ERR(pni->pn_dev))) {
		ret = PTR_ERR(pni->pn_dev);
		pni->pn_dev = NULL;
		E("%s : device_create failed\n", __func__);
		goto err_create_pn_device;
	}

	
	ret = device_create_file(pni->pn_dev, &dev_attr_pn_temp1);
	if (ret) {
		E("%s : device_create_file dev_attr_pn_temp1 failed\n", __func__);
		goto err_create_pn_file;
	}

	ret = device_create_file(pni->pn_dev, &dev_attr_debug_enable);
	if (ret) {
		E("pn548_probe device_create_file dev_attr_debug_enable failed\n");
		goto err_create_pn_file;
	}

	pni->comn_dev = device_create(pni->pn548_class, NULL, 0, "%s", "comn");
	if (unlikely(IS_ERR(pni->comn_dev))) {
		ret = PTR_ERR(pni->comn_dev);
		pni->comn_dev = NULL;
		E("%s : device_create failed\n", __func__);
		goto err_create_pn_device;
	}

	ret = device_create_file(pni->comn_dev, &dev_attr_nxp_uicc_swp);
	if (ret) {
		E("pn548_probe device_create_file dev_attrnxp_uicc_swp failed\n");
	}
#if FTM_MODE
	watchdog_timeout = WATCHDOG_FTM_TIMEOUT_SEC; 
	I("%s: device_create_file for FTM mode+\n", __func__);
	ret = device_create_file(pni->comn_dev, &dev_attr_mfg_nfc_ctrl);
	if (ret) {
		E("pn548_probe device_create_file dev_attr_mfg_nfc_ctrl failed\n");
	}

	ret = device_create_file(pni->comn_dev, &dev_attr_mfg_nfcversion);
	if (ret) {
		E("pn548_probe device_create_file dev_attr_mfg_nfcversion failed\n");
	}

	ret = device_create_file(pni->comn_dev, &dev_attr_mfg_nfcreader);
	if (ret) {
		E("pn548_probe device_create_file dev_attr_mfg_nfcreader\n");
	}

	ret = device_create_file(pni->comn_dev, &dev_attr_mfg_nfccard);
	if (ret) {
		E("pn548_probe device_create_file dev_attr_mfg_nfccard failed\n");
	}

    ret = device_create_file(pni->comn_dev, &dev_attr_mfg_nfc_timeout);
    if (ret) {
        E("pn548_probe device_create_file dev_attr_mfg_nfc_timeout failed\n");
    }
    I("%s: device_create_file for FTM mode done -\n", __func__);
#endif  

	if (is_alive == 0) {
		I("%s: Without NFC, device_create_file dev_attr_nxp_chip_alive \n", __func__);
		ret = device_create_file(pni->pn_dev, &dev_attr_nxp_chip_alive);
		if (ret) {
			E("pn548_probe device_create_file dev_attr_nxp_chip_alive failed\n");
			goto err_create_pn_file;
		}
	} else {
		if(pni->boot_mode == NFC_BOOT_MODE_OFF_MODE_CHARGING ) {
#ifdef NFC_IOEXT_PIN_CTRL_CONFIG
			if(NFC_I2C1V8_GPIO)
			{
				ret = gpio_request(NFC_I2C1V8_GPIO, "nfc_i2c1v8");
				if (ret) {
					E("%s : OFF-Charging request 1V8I2C gpio %u fail\n",__func__, NFC_I2C1V8_GPIO);
					ret = -ENODEV;
					goto err_request_gpio_ven;
				}

				ret = gpio_direction_output(NFC_I2C1V8_GPIO, 1);
				I("%s : Off-mode charging, set NFC_I2C1V8_GPIO(%u) High, Value:%d\n", __func__, NFC_I2C1V8_GPIO, gpio_get_value(NFC_I2C1V8_GPIO));
			}
#endif
			mfg_nfc_test(88);
		}
	}
	I("%s: Probe success! is_alive : %d, is_uicc_swp : %d\n", __func__, is_alive, is_uicc_swp);
	return 0;

err_create_pn_file:
	device_unregister(pni->pn_dev);
err_create_pn_device:
	class_destroy(pni->pn548_class);
err_create_class:
err_request_irq_failed:
	misc_deregister(&pni->pn548_device);
err_misc_register:
	mutex_destroy(&pni->read_mutex);
	wake_lock_destroy(&pni->io_wake_lock);
	kfree(pni);
	pn_info = NULL;
err_kzalloc:
#ifdef NFC_GPIO_PINCTRL_ENABLE
	if (pn548_htc_check_pvdd_gpio()) {
		gpio_free(pn548_htc_get_pvdd_gpio());
	}
err_request_gpio_pvdd:
#endif
	gpio_free(platform_data->firm_gpio);
err_request_gpio_firm:
	gpio_free(platform_data->ven_gpio);
err_request_gpio_ven:
	gpio_free(platform_data->irq_gpio);
err_exit:
	kfree(platform_data);
	is_alive = 0;
	E("%s: prob fail\n", __func__);
	return ret;
}

static void pn548_shutdown(struct i2c_client *client) {
	pn548_power_off_seq(is_alive);
}


static int pn548_remove(struct i2c_client *client)
{
	struct pn548_dev *pn548_dev;
	I("%s:\n", __func__);

	if (I2CDMABuf_va_read != NULL) {
		dma_free_coherent(&client->adapter->dev, DMA_SIZE, I2CDMABuf_va_read, (dma_addr_t)I2CDMABuf_pa_read);
		I2CDMABuf_va_read = NULL;
		I2CDMABuf_pa_read = NULL;
	}
	if (I2CDMABuf_va_write != NULL) {
		dma_free_coherent(&client->adapter->dev, DMA_SIZE, I2CDMABuf_va_write, (dma_addr_t)I2CDMABuf_pa_write);
		I2CDMABuf_va_write = NULL;
		I2CDMABuf_pa_write = NULL;
	}
	pn548_dev = i2c_get_clientdata(client);
	free_irq(client->irq, pn548_dev);
	misc_deregister(&pn548_dev->pn548_device);
	mutex_destroy(&pn548_dev->read_mutex);
	wake_lock_destroy(&pn548_dev->io_wake_lock);
	gpio_free(pn548_dev->irq_gpio);
	gpio_free(pn548_dev->ven_gpio);
	gpio_free(pn548_dev->firm_gpio);
	#ifdef NFC_GPIO_PINCTRL_ENABLE
	if (pn548_htc_check_pvdd_gpio()) {
		gpio_free(pn548_htc_get_pvdd_gpio());
	}
	#endif
	kfree(pn548_dev);
	pn_info = NULL;

	return 0;
}

#ifdef CONFIG_PM
static int pn548_suspend(struct i2c_client *client, pm_message_t state)
{
	struct pn548_dev *pni = pn_info;

        I("%s: irq = %d, ven_gpio = %d, isEn = %d, isReadBlock =%d\n", __func__, \
        		gpio_get_value(pni->irq_gpio), gpio_get_value(pni->ven_gpio), pn548_isEn(), pni->isReadBlock);

	if (pni->ven_value && pni->isReadBlock && is_alive) {
		pni->irq_enabled = true;
		enable_irq(pni->client->irq);
		irq_set_irq_wake(pni->client->irq, 1);
	}

	return 0;
}

static int pn548_resume(struct i2c_client *client)
{
	struct pn548_dev *pni = pn_info;

        I("%s: irq = %d, ven_gpio = %d, isEn = %d, isReadBlock =%d\n", __func__, \
        		gpio_get_value(pni->irq_gpio), gpio_get_value(pni->ven_gpio), pn548_isEn(), pni->isReadBlock);

	if (pni->ven_value && pni->isReadBlock && is_alive) {
		pn548_disable_irq(pni);
		irq_set_irq_wake(pni->client->irq, 0);
	}

	return 0;
}
#endif

static const struct i2c_device_id pn548_id[] = {
	{ "pn548", 0 },
	{ }
};

static struct of_device_id pn548_match_table[] = {
	{ .compatible = "nxp,pn548-nfc",},
	{ },
};

static struct i2c_driver pn548_driver = {
	.id_table	= pn548_id,
	.probe		= pn548_probe,
	.remove		= pn548_remove,
	.shutdown = pn548_shutdown,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "pn548",
		.of_match_table = pn548_match_table,
	},
#if CONFIG_PM
	.suspend	= pn548_suspend,
	.resume		= pn548_resume,
#endif
};


static int __init pn548_dev_init(void)
{
	I("%s: Loading pn548 driver\n", __func__);
	return i2c_add_driver(&pn548_driver);
}
module_init(pn548_dev_init);

static void __exit pn548_dev_exit(void)
{
	I("%s: Unloading pn548 driver\n", __func__);
	i2c_del_driver(&pn548_driver);
}
module_exit(pn548_dev_exit);

MODULE_AUTHOR("HTC SSD NFC");
MODULE_DESCRIPTION("NFC PN548 driver");
MODULE_LICENSE("GPL");
