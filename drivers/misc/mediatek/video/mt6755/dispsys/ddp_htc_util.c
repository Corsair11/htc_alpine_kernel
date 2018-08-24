#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/uaccess.h>
#include <linux/htc_lcm_id.h>
#include <primary_display.h>
#include "ddp_htc_util.h"

int htc_get_lcm_id(void)
{
#ifdef CONFIG_ALPINE_LCM
	if (!strcmp(get_disp_lcm_drv_name(), "r63452_fhd_dsi_cmd_jdi_evm"))
		return EVM_SOURCE;

	if (!strcmp(get_disp_lcm_drv_name(), "r63452_fhd_dsi_cmd_jdi"))
		return REAL_SOURCE;

	if (!strcmp(get_disp_lcm_drv_name(), "null_panel"))
		return NULL_PANEL;
#endif

	return UNKNOWN_SOURCE;
}

static ssize_t dsi_cmd_write(
	struct file *file,
	const char __user *buff,
	size_t count,
	loff_t *ppos)
{
	char debug_buf[DSI_DEBUG_BUF];
	struct LCM_setting_table debug_cmd;
	int type, value;
	int para_cnt, i;
	char *tmp;

	if (count >= sizeof(debug_buf) || count < DSI_MIN_COUNT)
		return -EFAULT;

	if (copy_from_user(debug_buf, buff, count))
		return -EFAULT;

	
	debug_buf[count] = 0;

	
	para_cnt = (count) / 3 - 2;

	
	sscanf(debug_buf, "%x", &type);

	sscanf(debug_buf + 3, "%x", &debug_cmd.cmd);
	debug_cmd.count = para_cnt;

	
	for (i = 0; i < para_cnt; i++) {
		if (i >= DCS_MAX_CNT) {
			printk("%s: DCS command count over DCS_MAX_CNT, Skip these commands.\n", __func__);
			break;
		}
		tmp = debug_buf + (3 * (i + 2));
		sscanf(tmp, "%x", &value);
		debug_cmd.para_list[i] = value;
	}

	for (i = 0; i < debug_cmd.count; i++)
		printk("%s: addr = 0x%x, para[%d] = 0x%x\n", __FUNCTION__, debug_cmd.cmd, i, debug_cmd.para_list[i]);

	primary_display_setlcm_cmd(&debug_cmd.cmd, &debug_cmd.count, debug_cmd.para_list);

	return count;
}

static const struct file_operations dsi_cmd_fops = {
	.write = dsi_cmd_write,
};

void htc_debugfs_init(void)
{
	struct dentry *dent = debugfs_create_dir("htc_debug", NULL);

	if (IS_ERR(dent)) {
		pr_err(KERN_ERR "%s(%d): debugfs_create_dir fail, error %ld\n",
			__FILE__, __LINE__, PTR_ERR(dent));
		return;
	}

	if (debugfs_create_file("dsi_cmd", 0644, dent, 0, &dsi_cmd_fops) == NULL) {
		pr_err(KERN_ERR "%s(%d): debugfs_create_file: index fail (dsi_cmd)\n",
			__FILE__, __LINE__);
	}
}


static int apply_disp_nvram_data = 0;
static int apply_bklt_nvram_data = 0;
static unsigned int last_bklt = 0;

static struct kobject *android_display;
struct disp_cali_gain gain, cur_gain;
struct disp_cali_gamma disp_cali_lut;
struct attr_status htc_attr_status[] = {
	{"disp_cali_enable", 0, 0, 0},
	{"disp_cali_read", 0, 0, 0},
	{"bklt_cali_enable", 0, 0, 0},
	{"bklt_cali_read", 0, 0, 0},
};

void htc_load_rgb_cali_data(void);
void htc_load_bklt_cali_data(void);
void bklt_cali_enable(void);

static ssize_t disp_vendor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	scnprintf(buf, PAGE_SIZE, "%s\n", get_disp_lcm_drv_name());
	ret = strlen(buf) + 1;

	return ret;
}

static ssize_t lm36274_reg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned length = 0;
#ifdef CONFIG_LM36274_SUPPORT
	unsigned char addr;
	uint8_t rdata;
	int i;

	length = scnprintf(buf, PAGE_SIZE, "LM36274 register dump:\n");
	for (i = 1; i < LM36274_REG_MAX; i++) {
		rdata = 0;
		addr = i;
		lm36274_read_bytes(addr, &rdata);
		length +=scnprintf(buf + length, PAGE_SIZE -length, "addr:0x%x, rdata = 0x%x\n", addr, rdata);
	}

	return length;
#else
	length = scnprintf(buf, PAGE_SIZE, "not supported\n");
	return length;
#endif
}

static ssize_t disp_cali_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret =0;
	ret = scnprintf(buf, PAGE_SIZE, "%s%x\n%s%x\n%s%x\n",
				"GAIN_R = 0x", gain.r, "GAIN_G = 0x", gain.g, "GAIN_B = 0x", gain.b);

	return ret;
}

static ssize_t disp_cali_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned val_r, val_g, val_b;

	if (count < RGB_MIN_COUNT)
		return -EFAULT;


	if (sscanf(buf, "%x %x %x ", &val_r, &val_g, &val_b) != 3) {
		pr_err("[DISP] %s sscanf buf fail\n", __func__);
	} else if (RGB_CHECK(val_r) && RGB_CHECK(val_g) && RGB_CHECK(val_b)) {
		gain.r = val_r;
		gain.g = val_g;
		gain.b = val_b;
		pr_info("[DISP] %s: gain_r = 0x%x, gain_g = 0x%x, gain_b = 0x%x\n", __func__,
				gain.r, gain.g, gain.b);
	}

	return count;
}

static ssize_t disp_cali_gamma_r_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned length = 0;
	int i;

	length = scnprintf(buf, PAGE_SIZE, "< R >\n");
	for (i = 0; i < DISP_GAMMA_LUT_SIZE - 1; i++)
		length +=scnprintf(buf + length, PAGE_SIZE - length, "%d, ", disp_cali_lut.lut_r[i]);
	length +=scnprintf(buf + length, PAGE_SIZE - length, "%d\n", disp_cali_lut.lut_r[DISP_GAMMA_LUT_SIZE - 1]);

	return length;
}

static ssize_t disp_cali_gamma_g_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned length = 0;
	int i;

	length = scnprintf(buf, PAGE_SIZE, "< G >\n");
	for (i = 0; i < DISP_GAMMA_LUT_SIZE - 1; i++)
		length +=scnprintf(buf + length, PAGE_SIZE - length, "%d, ", disp_cali_lut.lut_g[i]);
	length +=scnprintf(buf + length, PAGE_SIZE - length, "%d\n", disp_cali_lut.lut_g[DISP_GAMMA_LUT_SIZE - 1]);

	return length;
}

static ssize_t disp_cali_gamma_b_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned length = 0;
	int i;

	length = scnprintf(buf, PAGE_SIZE, "< B >\n");
	for (i = 0; i < DISP_GAMMA_LUT_SIZE - 1; i++)
		length +=scnprintf(buf + length, PAGE_SIZE - length, "%d, ", disp_cali_lut.lut_b[i]);
	length +=scnprintf(buf + length, PAGE_SIZE - length, "%d\n", disp_cali_lut.lut_b[DISP_GAMMA_LUT_SIZE - 1]);

	return length;
}

static ssize_t bklt_cali_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret =0;
	ret = scnprintf(buf, PAGE_SIZE, "%s%d\n", "GAIN_BKLT=", gain.bklt);

	return ret;
}

static ssize_t bklt_cali_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0;

	if (sscanf(buf, "%d", &val) != 1) {
		pr_err("[DISP] %s: sscanf buf fail\n", __func__);
	} else if(BKLT_CHECK(val)) {
		gain.bklt = val;
		pr_info("[DISP] %s: gain_bklt = %d\n", __func__, gain.bklt);
	}

	return count;
}

static ssize_t attrs_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	int i;

	for (i = 0; i < ARRAY_SIZE(htc_attr_status); i++) {
		if (!strcmp(attr->attr.name, htc_attr_status[i].title)) {
			ret = scnprintf(buf, PAGE_SIZE, "%d\n", htc_attr_status[i].cur_value);
			break;
		}
	}

	return ret;
}

static ssize_t attr_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long res;
	int rc, i;

	rc = kstrtoul(buf, 10, &res);
	if (rc) {
		pr_err("[DISP] invalid parameter, %s %d\n", buf, rc);
		count = -EINVAL;
		goto err_out;
	}

	for (i = 0; i < ARRAY_SIZE(htc_attr_status); i++) {
		if (!strcmp(attr->attr.name, htc_attr_status[i].title)) {
			htc_attr_status[i].req_value = res;

			
			if (!strcmp(attr->attr.name, "disp_cali_read") && res == 1)
				htc_load_rgb_cali_data();

			
			if (!strcmp(attr->attr.name, "bklt_cali_read") && res == 1)
				htc_load_bklt_cali_data();

			if (!strcmp(attr->attr.name, "disp_cali_enable") && res == 0)
				apply_disp_nvram_data = 0;

			if (!strcmp(attr->attr.name, "bklt_cali_enable"))
				bklt_cali_enable();

			break;
		}
	}

err_out:
	return count;
}

static DEVICE_ATTR(vendor, S_IRUGO, disp_vendor_show, NULL);
static DEVICE_ATTR(lm36274_reg, S_IRUGO, lm36274_reg_show, NULL);
static DEVICE_ATTR(disp_cali, S_IRUGO | S_IWUSR, disp_cali_show, disp_cali_store);
static DEVICE_ATTR(disp_cali_enable, S_IRUGO | S_IWUSR, attrs_show, attr_store);
static DEVICE_ATTR(disp_cali_read, S_IRUGO | S_IWUSR, attrs_show, attr_store);
static DEVICE_ATTR(disp_cali_gamma_r, S_IRUGO, disp_cali_gamma_r_show, NULL);
static DEVICE_ATTR(disp_cali_gamma_g, S_IRUGO, disp_cali_gamma_g_show, NULL);
static DEVICE_ATTR(disp_cali_gamma_b, S_IRUGO, disp_cali_gamma_b_show, NULL);
static DEVICE_ATTR(bklt_cali, S_IRUGO | S_IWUSR, bklt_cali_show, bklt_cali_store);
static DEVICE_ATTR(bklt_cali_enable, S_IRUGO | S_IWUSR, attrs_show, attr_store);
static DEVICE_ATTR(bklt_cali_read, S_IRUGO | S_IWUSR, attrs_show, attr_store);

static struct attribute *htc_extend_attrs[] = {
	&dev_attr_vendor.attr,
	&dev_attr_lm36274_reg.attr,
	&dev_attr_disp_cali.attr,
	&dev_attr_disp_cali_enable.attr,
	&dev_attr_disp_cali_read.attr,
	&dev_attr_disp_cali_gamma_r.attr,
	&dev_attr_disp_cali_gamma_g.attr,
	&dev_attr_disp_cali_gamma_b.attr,
	&dev_attr_bklt_cali.attr,
	&dev_attr_bklt_cali_enable.attr,
	&dev_attr_bklt_cali_read.attr,
	NULL,
};

static struct attribute_group htc_extend_attr_group = {
	.attrs = htc_extend_attrs,
};

void htc_display_sysfs_init(void)
{
	if (!android_display) {
		int i;

		
		android_display = kobject_create_and_add("android_display", NULL);
		if (!android_display) {
			pr_err(KERN_ERR "%s(%d): kobject_create_and_add for android_display failed!\n",
				__FILE__, __LINE__);
			goto err;
		}

		if (sysfs_create_group(android_display, &htc_extend_attr_group)) {
			pr_err(KERN_ERR "%s(%d): sysfs group creation failed!\n",
				__FILE__, __LINE__);
			goto err;
		}

		
		gain.r = cur_gain.r = RGB_CALI_BASE;
		gain.g = cur_gain.g = RGB_CALI_BASE;
		gain.b = cur_gain.b = RGB_CALI_BASE;
		
		gain.bklt = cur_gain.bklt = BKLT_CALI_BASE;

		
		for (i = 0; i < DISP_GAMMA_LUT_SIZE; i++) {
			disp_cali_lut.lut_r[i] = 0;
			disp_cali_lut.lut_g[i] = 0;
			disp_cali_lut.lut_b[i] = 0;
		}
	}

	return;

err:
	kobject_del(android_display);
	android_display = NULL;
}

int get_data_from_nvram(char *filename, char *data, ssize_t len, int offset)
{
	struct file *fd;
	int retLen = -1;

	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);

	fd = filp_open(filename, O_RDONLY, 0644);
	if (IS_ERR(fd)) {
		pr_err("[DISP] %s: failed to open!!\n", __FUNCTION__);
		return -1;
	}

	do {
		if ((fd->f_op == NULL) || (fd->f_op->read == NULL)) {
			pr_err("[DISP]  %s: file can not be read!!\n", __FUNCTION__);
			break;
		}

		if (fd->f_pos != offset) {
			if (fd->f_op->llseek) {
				if (fd->f_op->llseek(fd, offset, 0) != offset) {
					pr_err("[DISP] %s: failed to seek!!\n", __FUNCTION__);
					break;
				}
			} else {
				fd->f_pos = offset;
			}
		}

		retLen = fd->f_op->read(fd, data, len, &fd->f_pos);
	} while (0);

	filp_close(fd, NULL);
	set_fs(old_fs);

	return retLen;
}

void htc_load_rgb_cali_data(void)
{
	unsigned char nvram_r[1], nvram_g[1], nvram_b[1];
	unsigned char gain_temp = 0;
	int ret;

	htc_attr_status[RGB_CALI_READ_INDEX].cur_value = htc_attr_status[RGB_CALI_READ_INDEX].req_value;

	
	ret = get_data_from_nvram(DISP_CALI_NVRAM_FILE_NAME,
								(char *)nvram_r,
								sizeof(unsigned char),
								((unsigned long)&(((struct disp_cali_gain *)0)->r)));
	if (ret >= 0) {
		gain_temp = nvram_r[0];
		if (RGB_CHECK(gain_temp)) {
			gain.r = gain_temp;
			pr_info("[DISP] %s: R gain = 0x%x\n", __FUNCTION__, gain_temp);
		} else {
			pr_err("[DISP] %s: invalid R gain (0x%x) !\n", __FUNCTION__, gain_temp);
		}
	}

	ret = get_data_from_nvram(DISP_CALI_NVRAM_FILE_NAME,
								(char *)nvram_g,
								sizeof(unsigned char),
								((unsigned long)&(((struct disp_cali_gain *)0)->g)));
	if (ret >= 0) {
		gain_temp = nvram_g[0];
		if (RGB_CHECK(gain_temp)) {
			gain.g = gain_temp;
			pr_info("[DISP] %s: G gain = 0x%x\n", __FUNCTION__, gain_temp);
		} else {
			pr_err("[DISP] %s: invalid G gain (0x%x) !\n", __FUNCTION__, gain_temp);
		}
	}

	ret = get_data_from_nvram(DISP_CALI_NVRAM_FILE_NAME,
								(char *)nvram_b,
								sizeof(unsigned char),
								((unsigned long)&(((struct disp_cali_gain *)0)->b)));
	if (ret >= 0) {
		gain_temp = nvram_b[0];
		if (RGB_CHECK(gain_temp)) {
			gain.b = gain_temp;
			pr_info("[DISP] %s: B gain = 0x%x\n", __FUNCTION__, gain_temp);
		} else {
			pr_err("[DISP] %s: invalid B gain (0x%x) !\n", __FUNCTION__, gain_temp);
		}
	}

	if ((gain.r != RGB_CALI_BASE) || (gain.g != RGB_CALI_BASE) || (gain.b != RGB_CALI_BASE))
		apply_disp_nvram_data = 1;
}

void htc_load_bklt_cali_data(void)
{
	int nvram_bklt[1];
	int gain_temp = 0;
	int ret;

	htc_attr_status[BKLT_CALI_READ_INDEX].cur_value = htc_attr_status[BKLT_CALI_READ_INDEX].req_value;

	
	ret = get_data_from_nvram(DISP_CALI_NVRAM_FILE_NAME,
								(char *)nvram_bklt,
								sizeof(int),
								((unsigned long)&(((struct disp_cali_gain *)0)->bklt)));
	if (ret >= 0) {
		gain_temp = nvram_bklt[0];
		if (BKLT_CHECK(gain_temp)) {
			gain.bklt = gain_temp;
			pr_info("[DISP] %s: backlight gain = %d\n", __FUNCTION__, gain_temp);
		} else {
			pr_err("[DISP] %s: invalid backlight gain (%d) !\n", __FUNCTION__, gain_temp);
		}
	}

	if (gain.bklt != BKLT_CALI_BASE)
		apply_bklt_nvram_data = 1;
}

void htc_update_rgb_cali_data(DISP_GAMMA_LUT_T *gamma_lut)
{
	int i;

	if (htc_attr_status[RGB_CALI_ENABLE_INDEX].req_value < 0)
		return;

	htc_attr_status[RGB_CALI_ENABLE_INDEX].cur_value = htc_attr_status[RGB_CALI_ENABLE_INDEX].req_value;

	
	if ((gain.r != RGB_CALI_BASE) || (gain.g != RGB_CALI_BASE) || (gain.b != RGB_CALI_BASE)) {
		cur_gain.r = gain.r;
		cur_gain.g = gain.g;
		cur_gain.b = gain.b;

		
		if (htc_attr_status[RGB_CALI_ENABLE_INDEX].cur_value || apply_disp_nvram_data) {
			
			for (i = 0; i < DISP_GAMMA_LUT_SIZE; i++) {
				disp_cali_lut.lut_r[i] = RGB_CALI(((gamma_lut->lut[i] >> 20) & 0x3FF), cur_gain.r);
				disp_cali_lut.lut_g[i] = RGB_CALI(((gamma_lut->lut[i] >> 10) & 0x3FF), cur_gain.g);
				disp_cali_lut.lut_b[i] = RGB_CALI((gamma_lut->lut[i] & 0x3FF), cur_gain.b);
				gamma_lut->lut[i] = GAMMA_ENTRY(disp_cali_lut.lut_r[i],
								disp_cali_lut.lut_g[i],
								disp_cali_lut.lut_b[i]);
			}

			pr_info("[DISP] %s: RGB is calibrated (0x%x, 0x%x, 0x%x)\n",
					__func__, cur_gain.r, cur_gain.g, cur_gain.b);
		} else if (!htc_attr_status[RGB_CALI_ENABLE_INDEX].cur_value) {
			
			for (i = 0; i < DISP_GAMMA_LUT_SIZE; i++) {
				disp_cali_lut.lut_r[i] = (unsigned int)((gamma_lut->lut[i] >> 20) & 0x3FF);
				disp_cali_lut.lut_g[i] = (unsigned int)((gamma_lut->lut[i] >> 10) & 0x3FF);
				disp_cali_lut.lut_b[i] = (unsigned int)(gamma_lut->lut[i] & 0x3FF);
			}
		}
	}
}

void bklt_cali_enable(void)
{
	unsigned int cali_bklt;

	if (htc_attr_status[BKLT_CALI_ENABLE_INDEX].req_value < 0)
		return;

	htc_attr_status[BKLT_CALI_ENABLE_INDEX].cur_value = htc_attr_status[BKLT_CALI_ENABLE_INDEX].req_value;

	if (!htc_attr_status[BKLT_CALI_ENABLE_INDEX].cur_value) {
		apply_bklt_nvram_data = 0;

		if (last_bklt) {
			gain.bklt = BKLT_CALI_BASE;
			cur_gain.bklt = gain.bklt;
			primary_display_setbacklight(last_bklt, 1);

			pr_info("[DISP] %s: disable. restore brightness value to %d\n", __func__, last_bklt);
		}
	} else {
		if (((cur_gain.bklt != gain.bklt) || (cur_gain.bklt != BKLT_CALI_BASE)) && last_bklt) {
			cur_gain.bklt = gain.bklt;
			cali_bklt = BKLT_CALI(last_bklt, cur_gain.bklt);

			pr_info("[DISP] %s: enable. brightness is calibrated from %d to %d\n", __func__, last_bklt, cali_bklt);

			primary_display_setbacklight(cali_bklt, 1);
		}
	}
}

unsigned int htc_update_bklt_cali_data(unsigned int last_brightness)
{
	unsigned int cali_bklt;

	last_bklt = last_brightness;

	
	if (htc_attr_status[BKLT_CALI_ENABLE_INDEX].cur_value || apply_bklt_nvram_data) {
		apply_bklt_nvram_data = 0;
		cur_gain.bklt = gain.bklt;
		cali_bklt = BKLT_CALI(last_brightness, cur_gain.bklt);

		pr_info("[DISP] %s: brightness is calibrated from %d to %d\n", __func__, last_brightness, cali_bklt);

		return cali_bklt;
	} else {
		
		return last_brightness;
	}
}
