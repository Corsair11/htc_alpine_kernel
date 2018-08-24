/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/wait.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#ifdef CONFIG_OF
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif
#include <asm/scatterlist.h>
#include <asm/io.h>
#include <mt-plat/aee.h>
#include <mt-plat/sync_write.h>
#include <mt-plat/mt_lpae.h>
#ifdef CONFIG_MTK_CLKMGR
#include <mach/mt_clkmgr.h>
#endif
#include <mt_i2c.h>
#include <mt_cpufreq_hybrid.h>

#include <linux/gpio.h>
#include <linux/of_gpio.h>

#define TAG     "MT_I2C"

#define DMA_LOG_LEN 7
static struct i2c_dma_info g_dma_data[DMA_LOG_LEN];


typedef int (*pmaster_xfer) (struct i2c_adapter *adap, struct i2c_msg *msgs, int num);

void i2c_writel(struct mt_i2c_t *i2c, u8 offset, u16 value)
{
	
	mt_reg_sync_writel(value, (i2c->base) + (offset));
}

u32 i2c_readl(struct mt_i2c_t *i2c, u8 offset)
{
	return __raw_readl((void *)((i2c->base) + (offset)));
}

static void mt_i2c_clock_enable(struct mt_i2c_t *i2c);
static void mt_i2c_clock_disable(struct mt_i2c_t *i2c);

u32 I2C_TIMING_REG_BACKUP[7] = {0};
u32 I2C_HIGHSP_REG_BACKUP[7] = {0};

#ifdef CONFIG_OF
static void __iomem *ap_dma_base;
#endif
#ifdef I2C_DRIVER_IN_KERNEL
static struct mt_i2c_msg g_msg[2];
static struct mt_i2c_t *g_i2c[2];
#define I2C_DRV_NAME        "mt-i2c"
#endif

static bool test_recovery = false;

#ifdef I2C_DEBUG_FS
#define PORT_COUNT 7
#define MESSAGE_COUNT 16
#define I2C_T_DMA 1
#define I2C_T_TRANSFERFLOW 2
#define I2C_T_SPEED 3
u8 i2c_port[PORT_COUNT][MESSAGE_COUNT];
#if 0
#define I2CINFO(type, format, arg...) \
do { \
	if (type < MESSAGE_COUNT && type >= 0) { \
		if (i2c_port[i2c->id][0] != 0 && (i2c_port[i2c->id][type] != 0 || \
			i2c_port[i2c->id][MESSAGE_COUNT - 1] != 0)) { \
			I2CLOG(format, ##arg); \
		} \
	} \
} while (0)
#endif
#define I2CINFO(type, format, arg...) I2CLOG(format, ##arg)

#ifdef I2C_DRIVER_IN_KERNEL
static ssize_t show_config(struct device *dev, struct device_attribute *attr, char *buff)
{
	s32 i = 0;
	s32 j = 0;
	char *buf = buff;

	test_recovery = !test_recovery;
	I2CERR("%s: Test i2c recovery 17-clk! test_recovery = %d\n", __func__, test_recovery);

	for (i = 0; i < PORT_COUNT; i++) {
		for (j = 0; j < MESSAGE_COUNT; j++)
			i2c_port[i][j] += '0';
		strncpy(buf, (char *)i2c_port[i], MESSAGE_COUNT);
		buf += MESSAGE_COUNT;
		*buf = '\n';
		buf++;
		for (j = 0; j < MESSAGE_COUNT; j++)
			i2c_port[i][j] -= '0';
	}
	return buf - buff;
}

static ssize_t set_config(struct device *dev, struct device_attribute *attr, const char *buf,
			  size_t count)
{
	s32 port, type, status;

	if (sscanf(buf, "%d %d %d", &port, &type, &status) == 3) {
		if (port >= PORT_COUNT || port < 0 || type >= MESSAGE_COUNT || type < 0) {
			
			I2CERR("i2c debug system: Parameter overflowed!\n");
		} else {
			if (status != 0)
				i2c_port[port][type] = 1;
			else
				i2c_port[port][type] = 0;

			I2CLOG("port:%d type:%d status:%s\ni2c debug system: Parameter accepted!\n",
			       port, type, status ? "on" : "off");
		}
	} else {
		
		I2CERR("i2c debug system: Parameter invalid!\n");
	}
	return count;
}

static DEVICE_ATTR(debug, S_IRUGO | S_IWUSR, show_config, set_config);
#endif
#else
#define I2CINFO(type, format, arg...)
#endif
static int i2c_get_semaphore(struct mt_i2c_t *i2c)
{
	if (i2c->id != 3)
		return 0;

	if (cpuhvfs_get_dvfsp_semaphore(SEMA_I2C_DRV) != 0) {
		I2CERR("sema time out 2ms\n");
		if (cpuhvfs_get_dvfsp_semaphore(SEMA_I2C_DRV) != 0) {
			I2CERR("sema time out 4ms\n");
			BUG_ON(1);
			return -EBUSY;
		}
	}

	return 0;
}

static int i2c_release_semaphore(struct mt_i2c_t *i2c)
{
	if (i2c->id != 3)
		return 0;

	cpuhvfs_release_dvfsp_semaphore(SEMA_I2C_DRV);

	return 0;
}

char *mt_i2c_bus_to_virt(unsigned long address)
{
	return (char *)address;
}

static s32 i2c_set_speed(struct mt_i2c_t *i2c)
{
	s32 ret = 0;
	s32 mode = 0;
	u32 khz = 0;

	
	u16 step_cnt_div = 0;
	u16 sample_cnt_div = 0;
	u32 tmp, sclk, hclk = i2c->clk;
	u16 max_step_cnt_div = 0;
	u32 diff, min_diff = i2c->clk;
	u16 sample_div = MAX_SAMPLE_CNT_DIV;
	u16 step_div = 0;
	
	
	

	if ((i2c->speed == i2c->last_speed) && (i2c->mode == i2c->last_mode)) {
		ret = 0;
		goto end;
	}

	mode = i2c->mode;
	khz = i2c->speed;

	max_step_cnt_div = (mode == HS_MODE) ? MAX_HS_STEP_CNT_DIV : MAX_STEP_CNT_DIV;
	step_div = max_step_cnt_div;

	if ((mode == FS_MODE && khz > MAX_FS_MODE_SPEED)
	    || (mode == HS_MODE && khz > MAX_HS_MODE_SPEED)) {
		I2CERR(" the speed is too fast for this mode.\n");
		I2C_BUG_ON((mode == FS_MODE && khz > MAX_FS_MODE_SPEED)
			   || (mode == HS_MODE && khz > MAX_HS_MODE_SPEED));
		ret = -EINVAL_I2C;
		goto end;
	}
	
	for (sample_cnt_div = 1; sample_cnt_div <= MAX_SAMPLE_CNT_DIV; sample_cnt_div++) {
		for (step_cnt_div = 1; step_cnt_div <= max_step_cnt_div; step_cnt_div++) {
			sclk = (hclk >> 1) / (sample_cnt_div * step_cnt_div);
			if (sclk > khz)
				continue;
			diff = khz - sclk;
			if (diff < min_diff) {
				min_diff = diff;
				sample_div = sample_cnt_div;
				step_div = step_cnt_div;
			}
		}
	}
	sample_cnt_div = sample_div;
	step_cnt_div = step_div;
	sclk = hclk / (2 * sample_cnt_div * step_cnt_div);
	if (sclk > khz) {
		I2CERR("%s mode: unsupported speed (%ldkhz)\n", (mode == HS_MODE) ? "HS" : "ST/FT",
		       (long int)khz);
		I2CLOG
		    ("i2c->clk=%d,sclk=%d khz=%d,i2c->speed=%d hclk=%d sample_cnt_div=%d,step_cnt_div=%d.\n",
		     i2c->clk, sclk, khz, i2c->speed, hclk, sample_cnt_div, step_cnt_div);
		I2C_BUG_ON(sclk > khz);
		ret = -ENOTSUPP_I2C;
		goto end;
	}

	step_cnt_div--;
	sample_cnt_div--;

	

	if (mode == HS_MODE) {

		
		tmp = i2c_readl(i2c, OFFSET_TIMING) & ~((0x7 << 8) | (0x3f << 0));
		tmp = (0 & 0x7) << 8 | (16 & 0x3f) << 0 | tmp;
		i2c->timing_reg = tmp;
		if (0 == i2c->timing_reg) {
			I2CLOG("hs base address 0x%p,tmp =0x%x\n", i2c->base, tmp);
			
		}
		
		

		
		tmp = i2c_readl(i2c, OFFSET_HS) & ~((0x7 << 12) | (0x7 << 8));
		tmp = (sample_cnt_div & 0x7) << 12 | (step_cnt_div & 0x7) << 8 | tmp;
		
		tmp |= 0x0001;
		i2c->high_speed_reg = tmp;
		
		
	} else {
		
		tmp = i2c_readl(i2c, OFFSET_TIMING) & ~((0x7 << 8) | (0x3f << 0));
		tmp = (sample_cnt_div & 0x7) << 8 | (step_cnt_div & 0x3f) << 0 | tmp;
		i2c->timing_reg = tmp;
		if (0 == i2c->timing_reg) {
			I2CLOG
			    ("n-hs base address 0x%p, tmp=0x%x, sample_cnt_div=0x%x, step_cnt_div=0x%x\n",
			     i2c->base, tmp, sample_cnt_div, step_cnt_div);
			
		}
		
		
		
		
		tmp = i2c_readl(i2c, OFFSET_HS) & ~(0x0001);
		
		i2c->high_speed_reg = tmp;
		
		
		
	}
	
	I2CINFO(I2C_T_SPEED, " set sclk to %dkhz(orig:%dkhz), sample=%d,step=%d\n", sclk, khz,
		sample_cnt_div, step_cnt_div);
end:
	
	i2c->last_speed = i2c->speed;
	i2c->last_mode = i2c->mode;
	return ret;
}

void _i2c_dump_info(struct mt_i2c_t *i2c)
{
	
	I2CLOG("I2C structure:\n"
	       I2CTAG "Clk=%d,Id=%d,Speed mode=%x,St_rs=%x,Dma_en=%x,Op=%x,Poll_en=%x,Irq_stat=%x\n"
	       I2CTAG "Trans_len=%x,Trans_num=%x,Trans_auxlen=%x,Data_size=%x,speed=%d\n"
	       I2CTAG "Trans_stop=%u,Trans_comp=%u,Trans_error=%u\n",
	       i2c->clk, i2c->id, i2c->mode, i2c->st_rs, i2c->dma_en, i2c->op, i2c->poll_en,
	       i2c->irq_stat, i2c->trans_data.trans_len, i2c->trans_data.trans_num,
	       i2c->trans_data.trans_auxlen, i2c->trans_data.data_size, i2c->speed,
	       atomic_read(&i2c->trans_stop), atomic_read(&i2c->trans_comp),
	       atomic_read(&i2c->trans_err));

	I2CLOG("base address 0x%p\n", i2c->base);
	I2CLOG("I2C register:\n"
	       I2CTAG "SLAVE_ADDR=%x,INTR_MASK=%x,INTR_STAT=%x,CONTROL=%x,TRANSFER_LEN=%x\n"
	       I2CTAG "TRANSAC_LEN=%x,DELAY_LEN=%x,TIMING=%x,START=%x,FIFO_STAT=%x\n"
	       I2CTAG "IO_CONFIG=%x,HS=%x,DCM_EN=%x,DEBUGSTAT=%x,EXT_CONF=%x,TRANSFER_LEN_AUX=%x\n",
	       (i2c_readl(i2c, OFFSET_SLAVE_ADDR)),
	       (i2c_readl(i2c, OFFSET_INTR_MASK)),
	       (i2c_readl(i2c, OFFSET_INTR_STAT)),
	       (i2c_readl(i2c, OFFSET_CONTROL)),
	       (i2c_readl(i2c, OFFSET_TRANSFER_LEN)),
	       (i2c_readl(i2c, OFFSET_TRANSAC_LEN)),
	       (i2c_readl(i2c, OFFSET_DELAY_LEN)),
	       (i2c_readl(i2c, OFFSET_TIMING)),
	       (i2c_readl(i2c, OFFSET_START)),
	       (i2c_readl(i2c, OFFSET_FIFO_STAT)),
	       (i2c_readl(i2c, OFFSET_IO_CONFIG)),
	       (i2c_readl(i2c, OFFSET_HS)),
	       (i2c_readl(i2c, OFFSET_DCM_EN)),
	       (i2c_readl(i2c, OFFSET_DEBUGSTAT)),
	       (i2c_readl(i2c, OFFSET_EXT_CONF)), (i2c_readl(i2c, OFFSET_TRANSFER_LEN_AUX)));

	I2CLOG("before enable DMA register(0x%p):\n"
	       I2CTAG "INT_FLAG=%x,INT_EN=%x,EN=%x,RST=%x,\n"
	       I2CTAG "STOP=%x,FLUSH=%x,CON=%x,TX_MEM_ADDR=%x, RX_MEM_ADDR=%x\n"
	       I2CTAG "TX_LEN=%x,RX_LEN=%x,INT_BUF_SIZE=%x,DEBUG_STATUS=%x\n"
	       I2CTAG "ARHP=%x,AWHP=%x\n",
	       g_dma_data[i2c->id].base, g_dma_data[i2c->id].int_flag,
	       g_dma_data[i2c->id].int_en, g_dma_data[i2c->id].en,
	       g_dma_data[i2c->id].rst, g_dma_data[i2c->id].stop,
	       g_dma_data[i2c->id].flush, g_dma_data[i2c->id].con,
	       g_dma_data[i2c->id].tx_mem_addr, g_dma_data[i2c->id].rx_mem_addr,
	       g_dma_data[i2c->id].tx_len, g_dma_data[i2c->id].rx_len,
	       g_dma_data[i2c->id].int_buf_size, g_dma_data[i2c->id].debug_sta,
	       g_dma_data[i2c->id].arhp, g_dma_data[i2c->id].awhp);

	if (i2c->dma_en) {
		I2CLOG("DMA register(0x%p):\n"
		       I2CTAG "INT_FLAG=%x,INT_EN=%x,EN=%x,RST=%x,CON=%x\n"
		       I2CTAG "TX_MEM_ADDR=%x,RX_MEM_ADDR=%x,TX_LEN=%x,RX_LEN=%x\n"
		       I2CTAG "INT_BUF_SIZE=%x,DEBUG_STATUS=%x,ARHP=%x,AWHP=%x\n"
		       I2CTAG "STOP=%x,FLUSH=%x\n",
		       i2c->pdmabase,
		       (__raw_readl(i2c->pdmabase + OFFSET_INT_FLAG)),
		       (__raw_readl(i2c->pdmabase + OFFSET_INT_EN)),
		       (__raw_readl(i2c->pdmabase + OFFSET_EN)),
		       (__raw_readl(i2c->pdmabase + OFFSET_RST)),
		       (__raw_readl(i2c->pdmabase + OFFSET_CON)),
		       (__raw_readl(i2c->pdmabase + OFFSET_TX_MEM_ADDR)),
		       (__raw_readl(i2c->pdmabase + OFFSET_RX_MEM_ADDR)),
		       (__raw_readl(i2c->pdmabase + OFFSET_TX_LEN)),
		       (__raw_readl(i2c->pdmabase + OFFSET_RX_LEN)),
		       (__raw_readl(i2c->pdmabase + OFFSET_INT_BUF_SIZE)),
		       (__raw_readl(i2c->pdmabase + OFFSET_DEBUG_STATUS)),
		       (__raw_readl(i2c->pdmabase + OFFSET_ARHP)),
		       (__raw_readl(i2c->pdmabase + OFFSET_AWHP)),
		       (__raw_readl(i2c->pdmabase + OFFSET_STOP)),
		       (__raw_readl(i2c->pdmabase + OFFSET_FLUSH)));

		I2CLOG("AP_DMA global: 0x8: 0x%x, 0xC: 0x%x, 0x70: 0x%x\n",
		       __raw_readl(ap_dma_base + 0x08),
		       __raw_readl(ap_dma_base + 0x0C),
		       __raw_readl(ap_dma_base + 0x70));
	}

}

#define DMA_POLL_THRES 10
static int dma_busy_wait_ready(struct mt_i2c_t *i2c)
{
	int dma_tmo_poll = DMA_POLL_THRES;

	if (NULL == i2c) {
		I2CERR("dma_busy_wait_ready NULL pointer error\n");
		return -1;
	}

	
	while (1 == (__raw_readl(i2c->pdmabase + OFFSET_EN))) {
		
		udelay(5);
		dma_tmo_poll--;
		if (dma_tmo_poll == 0)
			return -1;
	}
	return 0;
}

static void dma_reset(struct mt_i2c_t *i2c)
{
	int dma_tmo_poll = DMA_POLL_THRES;

	if (NULL == i2c) {
		I2CERR("dma_reset NULL pointer error\n");
		return;
	}

	
	mt_reg_sync_writel(0x0001, i2c->pdmabase + OFFSET_RST);

	
	while (1 == (__raw_readl(i2c->pdmabase + OFFSET_RST))) {
		
		udelay(5);
		dma_tmo_poll--;
		if (dma_tmo_poll == 0) {
			I2CERR("Wait for DMA warm reset error\n");
			return;
		}
	}
}

static s32 _i2c_deal_result(struct mt_i2c_t *i2c)
{
#ifdef I2C_DRIVER_IN_KERNEL
	long tmo = i2c->adap.timeout;
#else
	long tmo = 1;
#endif
	u16 data_size = 0;
	u8 *ptr = i2c->msg_buf;
	s32 ret = i2c->msg_len;
	long tmo_poll = 0xffff;
	int dma_err = 0;
	
	

	if (i2c->poll_en) {	
		for (;;) {	
			i2c->irq_stat = i2c_readl(i2c, OFFSET_INTR_STAT);
			
			if (i2c->irq_stat & (I2C_HS_NACKERR | I2C_ACKERR | I2C_TRANSAC_COMP)) {
				atomic_set(&i2c->trans_stop, 1);
				spin_lock(&i2c->lock);
				
				i2c_writel(i2c, OFFSET_INTR_STAT,
					   (I2C_HS_NACKERR | I2C_ACKERR | I2C_TRANSAC_COMP));
				spin_unlock(&i2c->lock);
				break;
			}
			tmo_poll--;
			if (tmo_poll == 0) {
				tmo = 0;
				break;
			}
		}
	} else {		
		tmo = wait_event_timeout(i2c->wait, atomic_read(&i2c->trans_stop), tmo);
	}

	
#ifdef I2C_DRIVER_IN_KERNEL
	if (i2c->irq_stat & I2C_TRANSAC_COMP) {
		atomic_set(&i2c->trans_err, 0);
		atomic_set(&i2c->trans_comp, 1);
	}
	atomic_set(&i2c->trans_err, i2c->irq_stat & (I2C_HS_NACKERR | I2C_ACKERR));
#endif
	if (test_recovery && (i2c->id == 1)) {
		I2CERR("Assign tmo to 0\n");
		tmo = 0;
	}

	
	if (!(tmo == 0 || atomic_read(&i2c->trans_err))) {
		
		if ((!i2c->dma_en) && (i2c->op == I2C_MASTER_RD || i2c->op == I2C_MASTER_WRRD)) {
			data_size = (i2c_readl(i2c, OFFSET_FIFO_STAT) >> 4) & 0x000F;

			if (i2c->op == I2C_MASTER_RD && data_size > i2c->msg_len) {
				I2CERR("data_size=%d, msg_len=%d\n", data_size, i2c->msg_len);
				_i2c_dump_info(i2c);
				BUG_ON(data_size > i2c->msg_len);
			}

			if (i2c->op == I2C_MASTER_WRRD && data_size > i2c->trans_data.trans_auxlen) {
				I2CERR("data_size=%d, msg_len=%d\n", data_size, i2c->msg_len);
				_i2c_dump_info(i2c);
				BUG_ON(data_size > i2c->trans_data.trans_auxlen);
			}

			while (data_size--) {
				*ptr = i2c_readl(i2c, OFFSET_DATA_PORT);
				
				ptr++;
			}
		}
		if (i2c->dma_en) {
			dma_err = dma_busy_wait_ready(i2c);
			if (dma_err) {
				I2CERR("i2c ok wait dma ready err\n");
				_i2c_dump_info(i2c);
				dma_reset(i2c);
			}
		}
#ifdef I2C_DEBUG_FS
		_i2c_dump_info(i2c);
#endif
	} else {
		
		if (tmo == 0) {
			I2CERR("id=%d,addr: %x, transfer timeout\n", i2c->id, i2c->addr);
			I2CERR("id=%d,addr: %x, Try to recover by 17 clk, i2c->recover_clk_cnt = %d\n", i2c->id, i2c->addr, i2c->recover_clk_cnt);

			if (i2c->recover_clk_cnt) {
				bool gpio_clk_status = false;
				int i, rc;
				struct pinctrl_state *pins_state;
				const char           *pins_state_name;

				I2CERR("%s: GPIO recover start, recover_clk_cnt = "
					    "%d\n", __func__, i2c->recover_clk_cnt);

				disable_irq(i2c->irqnr);

				pins_state      = i2c->gpio_state_recovery;
				pins_state_name = "i2c_recovery";

				if (!IS_ERR_OR_NULL(pins_state)) {
					int ret = pinctrl_select_state(i2c->pinctrl,
								       pins_state);
					if (ret) {
						dev_err(i2c->dev,
						"error pinctrl_select_state(%s) err:%d\n",
						pins_state_name, ret);
						goto after_gpio_operation_2;
					}
				} else {
					dev_err(i2c->dev,
						"error pinctrl state-name:'%s' is not "
						"configured\n", pins_state_name);
					goto after_gpio_operation_2;
				}

				rc = gpio_request(i2c->scl_gpio, "gpio_i2c-recovery_scl");
				if (rc) {
					dev_err(i2c->dev,
						"%s: gpio_request %d failed, rc = %d\n",
						__func__, i2c->scl_gpio, rc);
					goto after_gpio_operation_2;
				}

				rc = gpio_request(i2c->sda_gpio, "gpio_i2c-recovery_sda");
				if (rc) {
					dev_err(i2c->dev,
						"%s: gpio_request %d failed, rc = %d\n",
						__func__, i2c->sda_gpio, rc);
					goto after_gpio_operation_1;
				}

				I2CERR("%s: Ready to GPIO operation, scl = %d, sda = "
					 "%d\n", __func__, gpio_get_value(i2c->scl_gpio),
					 gpio_get_value(i2c->sda_gpio));

				for (i = 0; i < i2c->recover_clk_cnt; i++) {
					if (gpio_get_value(i2c->sda_gpio) && gpio_clk_status)
						break;
					gpio_direction_output(i2c->scl_gpio, 0);
					udelay(5);
					gpio_direction_output(i2c->sda_gpio, 0);
					udelay(5);
					gpio_direction_input(i2c->scl_gpio);
					udelay(5);
					if (!gpio_get_value(i2c->scl_gpio))
						udelay(20);
					if (!gpio_get_value(i2c->scl_gpio))
						usleep_range(10000, 10000);
					gpio_clk_status = gpio_get_value(i2c->scl_gpio);
					gpio_direction_input(i2c->sda_gpio);
					udelay(5);
				}
				if (gpio_get_value(i2c->sda_gpio) && gpio_get_value(i2c->scl_gpio)) {
					dev_info(i2c->dev, "Bus busy looks cleared, i = %d, scl = %d,"
						 " sda = %d\n", i,
						 gpio_get_value(i2c->scl_gpio),
						 gpio_get_value(i2c->sda_gpio));
				} else {
					dev_info(i2c->dev, "Bus looks still busy, i = %d, scl = %d,"
						 " sda = %d\n", i,
						 gpio_get_value(i2c->scl_gpio),
						 gpio_get_value(i2c->sda_gpio));
				}

				gpio_free(i2c->sda_gpio);
after_gpio_operation_1:
				gpio_free(i2c->scl_gpio);
after_gpio_operation_2:
				
				pins_state      = i2c->gpio_state_active;
				pins_state_name = "i2c_func";
				if (!IS_ERR_OR_NULL(pins_state)) {
					int ret = pinctrl_select_state(i2c->pinctrl,
								       pins_state);
					if (ret) {
						dev_err(i2c->dev,
						"error pinctrl_select_state_2(%s) err:%d\n",
						pins_state_name, ret);
					}
				} else {
					dev_err(i2c->dev,
						"error pinctrl_2 state-name:'%s' is not "
						"configured\n", pins_state_name);
				}

				udelay(10);

				enable_irq(i2c->irqnr);

				I2CERR("Bus revocery finished\n");
			}

			ret = -ETIMEDOUT_I2C;
		} else {
			I2CERR("id=%d,addr: %x, transfer error\n", i2c->id, i2c->addr);
			ret = -EREMOTEIO_I2C;
		}
		if (i2c->irq_stat & I2C_HS_NACKERR)
			I2CERR("I2C_HS_NACKERR\n");
		if (i2c->irq_stat & I2C_ACKERR)
			I2CERR("I2C_ACKERR\n");
		if (i2c->filter_msg == false)	
			_i2c_dump_info(i2c);

		spin_lock(&i2c->lock);
		
		i2c_writel(i2c, OFFSET_SOFTRESET, 0x0001);
		
		i2c_writel(i2c, OFFSET_SLAVE_ADDR, 0x0000);
		
		i2c_writel(i2c, OFFSET_INTR_STAT, (I2C_HS_NACKERR | I2C_ACKERR | I2C_TRANSAC_COMP));
		
		i2c_writel(i2c, OFFSET_FIFO_ADDR_CLR, 0x0001);
		
		i2c->last_mode = -1;
		i2c->last_speed = 0;
		spin_unlock(&i2c->lock);
		if (i2c->dma_en)
			dma_reset(i2c);

	}
	return ret;
}

static void record_i2c_dma_info(struct mt_i2c_t *i2c)
{
	if (i2c->id >= DMA_LOG_LEN) {
		I2CERR(" no space to record i2c dma log\n");
		return;
	}
	g_dma_data[i2c->id].base = i2c->pdmabase;
	g_dma_data[i2c->id].int_flag = (__raw_readl(i2c->pdmabase + OFFSET_INT_FLAG));
	g_dma_data[i2c->id].int_en = (__raw_readl(i2c->pdmabase + OFFSET_INT_EN));
	g_dma_data[i2c->id].en = (__raw_readl(i2c->pdmabase + OFFSET_EN));
	g_dma_data[i2c->id].rst = (__raw_readl(i2c->pdmabase + OFFSET_RST));
	g_dma_data[i2c->id].stop = (__raw_readl(i2c->pdmabase + OFFSET_STOP));
	g_dma_data[i2c->id].flush = (__raw_readl(i2c->pdmabase + OFFSET_FLUSH));
	g_dma_data[i2c->id].con = (__raw_readl(i2c->pdmabase + OFFSET_CON));
	g_dma_data[i2c->id].tx_mem_addr = (__raw_readl(i2c->pdmabase + OFFSET_TX_MEM_ADDR));
	g_dma_data[i2c->id].rx_mem_addr = (__raw_readl(i2c->pdmabase + OFFSET_RX_MEM_ADDR));
	g_dma_data[i2c->id].tx_len = (__raw_readl(i2c->pdmabase + OFFSET_TX_LEN));
	g_dma_data[i2c->id].rx_len = (__raw_readl(i2c->pdmabase + OFFSET_RX_LEN));
	g_dma_data[i2c->id].int_buf_size = (__raw_readl(i2c->pdmabase + OFFSET_INT_BUF_SIZE));
	g_dma_data[i2c->id].debug_sta = (__raw_readl(i2c->pdmabase + OFFSET_DEBUG_STATUS));
	g_dma_data[i2c->id].arhp = (__raw_readl(i2c->pdmabase + OFFSET_ARHP));
	g_dma_data[i2c->id].awhp = (__raw_readl(i2c->pdmabase + OFFSET_AWHP));
}

static void _i2c_write_reg(struct mt_i2c_t *i2c)
{
	u8 *ptr = i2c->msg_buf;
	u32 data_size = i2c->trans_data.data_size;
	u32 addr_reg = 0;
	

	i2c_writel(i2c, OFFSET_CONTROL, i2c->control_reg);

	
	if (i2c->speed <= 100)
		i2c_writel(i2c, OFFSET_EXT_CONF, 0x8001);
	else
		i2c_writel(i2c, OFFSET_EXT_CONF, 0x1801);

	
	i2c_writel(i2c, OFFSET_TIMING, i2c->timing_reg);
	i2c_writel(i2c, OFFSET_HS, i2c->high_speed_reg);

	if (0 == i2c->delay_len)
		i2c->delay_len = 2;
	if (~i2c->control_reg & I2C_CONTROL_RS) {	
		i2c_writel(i2c, OFFSET_DELAY_LEN, i2c->delay_len);
	}

	
	if (i2c->pushpull)
		i2c_writel(i2c, OFFSET_IO_CONFIG, 0x0000);
	else
		i2c_writel(i2c, OFFSET_IO_CONFIG, 0x0003);

	

	addr_reg = i2c->read_flag ? ((i2c->addr << 1) | 0x1) : ((i2c->addr << 1) & ~0x1);
	i2c_writel(i2c, OFFSET_SLAVE_ADDR, addr_reg);
	
	i2c_writel(i2c, OFFSET_INTR_STAT, (I2C_HS_NACKERR | I2C_ACKERR | I2C_TRANSAC_COMP));
	
	i2c_writel(i2c, OFFSET_FIFO_ADDR_CLR, 0x0001);
	
	if (i2c->poll_en) {
		
		i2c_writel(i2c, OFFSET_INTR_MASK, i2c_readl(i2c, OFFSET_INTR_MASK)
			 & ~(I2C_HS_NACKERR | I2C_ACKERR | I2C_TRANSAC_COMP));
	} else {
		
		i2c_writel(i2c, OFFSET_INTR_MASK, i2c_readl(i2c, OFFSET_INTR_MASK)
			 | (I2C_HS_NACKERR | I2C_ACKERR | I2C_TRANSAC_COMP));
	}
	
	i2c_writel(i2c, OFFSET_TRANSFER_LEN, i2c->trans_data.trans_len & 0xFFFF);
	i2c_writel(i2c, OFFSET_TRANSFER_LEN_AUX, i2c->trans_data.trans_auxlen & 0xFFFF);
	
	i2c_writel(i2c, OFFSET_TRANSAC_LEN, i2c->trans_data.trans_num & 0xFF);

	

	if (i2c->dma_en) {
		
		dma_reset(i2c);
		if (I2C_MASTER_RD == i2c->op) {
			mt_reg_sync_writel(0x0000, i2c->pdmabase + OFFSET_INT_FLAG);
			mt_reg_sync_writel(0x0001, i2c->pdmabase + OFFSET_CON);
			mt_reg_sync_writel((u32) ((long)i2c->msg_buf),
					   i2c->pdmabase + OFFSET_RX_MEM_ADDR);
			mt_reg_sync_writel(i2c->trans_data.data_size,
					   i2c->pdmabase + OFFSET_RX_LEN);
			if (enable_4G())
				mt_reg_sync_writel(0x1, i2c->pdmabase + OFFSET_AWHP);
		} else if (I2C_MASTER_WR == i2c->op) {
			mt_reg_sync_writel(0x0000, i2c->pdmabase + OFFSET_INT_FLAG);
			mt_reg_sync_writel(0x0000, i2c->pdmabase + OFFSET_CON);
			mt_reg_sync_writel((u32) ((long)i2c->msg_buf),
					   i2c->pdmabase + OFFSET_TX_MEM_ADDR);
			mt_reg_sync_writel(i2c->trans_data.data_size,
					   i2c->pdmabase + OFFSET_TX_LEN);
			if (enable_4G())
				mt_reg_sync_writel(0x1, i2c->pdmabase + OFFSET_ARHP);
		} else {
			mt_reg_sync_writel(0x0000, i2c->pdmabase + OFFSET_INT_FLAG);
			mt_reg_sync_writel(0x0000, i2c->pdmabase + OFFSET_CON);
			mt_reg_sync_writel((u32) ((long)i2c->msg_buf),
					   i2c->pdmabase + OFFSET_TX_MEM_ADDR);
			mt_reg_sync_writel((u32) ((long)i2c->msg_buf),
					   i2c->pdmabase + OFFSET_RX_MEM_ADDR);
			mt_reg_sync_writel(i2c->trans_data.trans_len,
					   i2c->pdmabase + OFFSET_TX_LEN);
			mt_reg_sync_writel(i2c->trans_data.trans_auxlen,
					   i2c->pdmabase + OFFSET_RX_LEN);
			if (enable_4G()) {
				mt_reg_sync_writel(0x1, i2c->pdmabase + OFFSET_AWHP);
				mt_reg_sync_writel(0x1, i2c->pdmabase + OFFSET_ARHP);
			}
		}
		
		record_i2c_dma_info(i2c);

		
		mb();
		mt_reg_sync_writel(0x0001, i2c->pdmabase + OFFSET_EN);

		I2CINFO(I2C_T_DMA, "addr %.2x dma %.2X byte\n", i2c->addr,
			i2c->trans_data.data_size);
		I2CINFO(I2C_T_DMA, "DMA Register:INT_FLAG:0x%x,CON:0x%x,TX_MEM_ADDR:0x%x",
					readl(i2c->pdmabase + OFFSET_INT_FLAG),
					readl(i2c->pdmabase + OFFSET_CON),
					readl(i2c->pdmabase + OFFSET_TX_MEM_ADDR));
		I2CINFO(I2C_T_DMA, "RX_MEM_ADDR:0x%x,TX_LEN:0x%x,RX_LEN:0x%x,EN:0x%x\n",
					readl(i2c->pdmabase + OFFSET_RX_MEM_ADDR),
					readl(i2c->pdmabase + OFFSET_TX_LEN),
					readl(i2c->pdmabase + OFFSET_RX_LEN),
					readl(i2c->pdmabase + OFFSET_EN));
	} else {
		
		if (I2C_MASTER_RD == i2c->op) {
			
		} else {	
			while (data_size--) {
				i2c_writel(i2c, OFFSET_DATA_PORT, *ptr);
				
				ptr++;
			}
		}
	}
	
	i2c->trans_data.data_size = data_size;

	if (0x0 == (i2c_readl(i2c, OFFSET_TIMING))) {
		
		i2c_writel(i2c, OFFSET_TIMING, 0x1410);
		aee_kernel_warning(I2CTAG, "@%s():%d,\n", __func__, __LINE__);
		
	}
}

static s32 _i2c_get_transfer_len(struct mt_i2c_t *i2c)
{
	s32 ret = I2C_OK;
	u16 trans_num = 0;
	u16 data_size = 0;
	u16 trans_len = 0;
	u16 trans_auxlen = 0;
	
	
	if (false == i2c->dma_en) {	
		if (I2C_MASTER_WRRD != i2c->op) {
			trans_len = (i2c->msg_len) & 0xFFFF;
			trans_num = (i2c->msg_len >> 16) & 0xFF;
			if (0 == trans_num)
				trans_num = 1;
			trans_auxlen = 0;
			data_size = trans_len * trans_num;

			if (!trans_len || !trans_num || data_size > I2C_FIFO_SIZE) {
				I2CERR("non-WRRD trans_len is wrong. trans_len=%x, tans_num=%x, trans_auxlen=%x\n",
					trans_len, trans_num, trans_auxlen);
				I2C_BUG_ON(!trans_len || !trans_num || data_size > I2C_FIFO_SIZE);
				ret = -EINVAL_I2C;
			}
		} else {
			trans_len = (i2c->msg_len) & 0xFF;
			trans_auxlen = (i2c->msg_len >> 8) & 0xFF;
			trans_num = 2;
			data_size = trans_len;
			if (!trans_len || !trans_auxlen || trans_len > I2C_FIFO_SIZE
			    || trans_auxlen > I2C_FIFO_SIZE) {
				I2CERR("WRRD trans_len is wrong. trans_len=%x, tans_num=%x, trans_auxlen=%x\n",
					trans_len, trans_num, trans_auxlen);
				I2C_BUG_ON(!trans_len || !trans_auxlen || trans_len > I2C_FIFO_SIZE
					   || trans_auxlen > I2C_FIFO_SIZE);
				ret = -EINVAL_I2C;
			}
		}
	} else {		
		if (I2C_MASTER_WRRD != i2c->op) {
			trans_len = (i2c->msg_len) & 0xFFFF;
			trans_num = (i2c->msg_len >> 16) & 0xFF;
			if (0 == trans_num)
				trans_num = 1;
			trans_auxlen = 0;
			data_size = trans_len * trans_num;

			if (!trans_len || !trans_num || trans_len > MAX_DMA_TRANS_SIZE
			    || trans_num > MAX_DMA_TRANS_NUM) {
				I2CERR("DMA non-WRRD trans_len is wrong. trans_len=%x, tans_num=%x, trans_auxlen=%x\n",
				     trans_len, trans_num, trans_auxlen);
				I2C_BUG_ON(!trans_len || !trans_num
					   || trans_len > MAX_DMA_TRANS_SIZE
					   || trans_num > MAX_DMA_TRANS_NUM);
				ret = -EINVAL_I2C;
			}
			I2CINFO(I2C_T_DMA,
				"DMA non-WRRD mode!trans_len=%x, tans_num=%x, trans_auxlen=%x\n",
				trans_len, trans_num, trans_auxlen);
		} else {
			trans_len = (i2c->msg_len) & 0xFF;
			trans_auxlen = (i2c->msg_len >> 8) & 0xFF;
			trans_num = 2;
			data_size = trans_len;
			if (!trans_len || !trans_auxlen || trans_len > MAX_DMA_TRANS_SIZE
			    || trans_auxlen > MAX_DMA_TRANS_NUM) {
				I2CERR("DMA WRRD trans_len is wrong. trans_len=%x, tans_num=%x, trans_auxlen=%x\n",
				       trans_len, trans_num, trans_auxlen);
				I2C_BUG_ON(!trans_len || !trans_auxlen
					   || trans_len > MAX_DMA_TRANS_SIZE
					   || trans_auxlen > MAX_DMA_TRANS_NUM);
				ret = -EINVAL_I2C;
			}
			I2CINFO(I2C_T_DMA,
				"DMA WRRD mode!trans_len=%x, tans_num=%x, trans_auxlen=%x\n",
				trans_len, trans_num, trans_auxlen);
		}
	}

	i2c->trans_data.trans_num = trans_num;
	i2c->trans_data.trans_len = trans_len;
	i2c->trans_data.data_size = data_size;
	i2c->trans_data.trans_auxlen = trans_auxlen;

	return ret;
}

static s32 _i2c_transfer_interface(struct mt_i2c_t *i2c)
{
	s32 return_value = 0;
	s32 ret = 0;
	u8 *ptr = i2c->msg_buf;
	

	if (i2c->dma_en) {
		I2CINFO(I2C_T_DMA, "DMA Transfer mode!\n");
		if (i2c->pdmabase == 0) {
			I2CERR(" I2C%d doesnot support DMA mode!\n", i2c->id);
			I2C_BUG_ON(i2c->pdmabase == NULL);
			ret = -EINVAL_I2C;
			goto err;
		}
		
		if (virt_addr_valid(ptr)) {
			I2CERR(" DMA mode should use physical buffer address!\n");
			I2C_BUG_ON(virt_addr_valid(ptr));
			ret = -EINVAL_I2C;
			goto err;
		}
	}
#ifdef I2C_DRIVER_IN_KERNEL
	atomic_set(&i2c->trans_stop, 0);
	atomic_set(&i2c->trans_comp, 0);
	atomic_set(&i2c->trans_err, 0);
#endif
	i2c->irq_stat = 0;

	return_value = _i2c_get_transfer_len(i2c);
	if (return_value < 0) {
		I2CERR("_i2c_get_transfer_len fail,return_value=%d\n", return_value);
		ret = -EINVAL_I2C;
		goto err;
	}
	
#ifdef CONFIG_MT_I2C_FPGA_ENABLE
	i2c->clk = I2C_CLK_RATE;
#else
	
	i2c->clk = I2C_CLK_RATE;
#endif

	return_value = i2c_set_speed(i2c);
	if (return_value < 0) {
		I2CERR("i2c_set_speed fail,return_value=%d\n", return_value);
		ret = -EINVAL_I2C;
		goto err;
	}
	
	i2c->control_reg = I2C_CONTROL_ACKERR_DET_EN | I2C_CONTROL_CLK_EXT_EN;
	if (i2c->dma_en)
		i2c->control_reg |= I2C_CONTROL_DMA_EN;

	if (I2C_MASTER_WRRD == i2c->op)
		i2c->control_reg |= I2C_CONTROL_DIR_CHANGE;

	if (HS_MODE == i2c->mode
	    || (i2c->trans_data.trans_num > 1 && I2C_TRANS_REPEATED_START == i2c->st_rs)) {
		i2c->control_reg |= I2C_CONTROL_RS;
	}

	
	if (i2c_get_semaphore(i2c) != 0)
		return -EBUSY;

	spin_lock(&i2c->lock);
	_i2c_write_reg(i2c);

	
	mb();
#ifdef I2C_DRIVER_IN_KERNEL
	
	if (i2c->i2c_3dcamera_flag) {
		spin_unlock(&i2c->lock);
		if (g_i2c[0] == NULL)
			g_i2c[0] = i2c;
		else
			g_i2c[1] = i2c;

		goto end;
	}
#endif
	I2CINFO(I2C_T_TRANSFERFLOW, "Before start .....\n");

	
	i2c_writel(i2c, OFFSET_START, 0x0001);
	spin_unlock(&i2c->lock);
	ret = _i2c_deal_result(i2c);

	
	if (i2c_release_semaphore(i2c) != 0)
		ret = -EBUSY;

	I2CINFO(I2C_T_TRANSFERFLOW, "After i2c transfer .....\n");
err:
end:
	return ret;
}

static void _i2c_translate_msg(struct mt_i2c_t *i2c, struct mt_i2c_msg *msg)
{
  
	if (msg->addr & 0xFF00)
		msg->ext_flag |= msg->addr & 0xFF00;
	I2CINFO(I2C_T_TRANSFERFLOW, "Before i2c transfer .....\n");

	i2c->msg_buf = msg->buf;
	i2c->msg_len = msg->len;
	if (msg->ext_flag & I2C_RS_FLAG)
		i2c->st_rs = I2C_TRANS_REPEATED_START;
	else
		i2c->st_rs = I2C_TRANS_STOP;

	if (msg->ext_flag & I2C_DMA_FLAG)
		i2c->dma_en = true;
	else
		i2c->dma_en = false;

	if (msg->ext_flag & I2C_WR_FLAG)
		i2c->op = I2C_MASTER_WRRD;
	else {
		if (msg->flags & I2C_M_RD)
			i2c->op = I2C_MASTER_RD;
		else
			i2c->op = I2C_MASTER_WR;
	}
	if (msg->ext_flag & I2C_POLLING_FLAG)
		i2c->poll_en = true;
	else
		i2c->poll_en = false;

	if (msg->ext_flag & I2C_A_FILTER_MSG)
		i2c->filter_msg = true;
	else
		i2c->filter_msg = false;
	i2c->delay_len = (msg->timing & 0xff0000) >> 16;

	
	if (0 == (msg->timing & 0xFFFF)) {
		i2c->mode = FS_MODE;
		i2c->speed = MAX_FS_MODE_SPEED;
	} else {
		if (msg->ext_flag & I2C_HS_FLAG)
			i2c->mode = HS_MODE;
		else
			i2c->mode = FS_MODE;

		i2c->speed = msg->timing & 0xFFFF;
	}

	
	if (msg->ext_flag & I2C_PUSHPULL_FLAG)
		i2c->pushpull = true;
	else
		i2c->pushpull = false;

	if (msg->ext_flag & I2C_3DCAMERA_FLAG)
		i2c->i2c_3dcamera_flag = true;
	else
		i2c->i2c_3dcamera_flag = false;

}

static s32 mt_i2c_start_xfer(struct mt_i2c_t *i2c, struct mt_i2c_msg *msg)
{
	s32 return_value = 0;
	s32 ret = msg->len;
	
	
	
	i2c->read_flag = (msg->flags & I2C_M_RD);
	i2c->addr = msg->addr;
	if (i2c->addr == 0) {
		I2CERR(" addr is invalid.\n");
		I2C_BUG_ON(i2c->addr == NULL);
		ret = -EINVAL_I2C;
		goto err;
	}

	if (msg->buf == NULL) {
		I2CERR(" data buffer is NULL.\n");
		I2C_BUG_ON(msg->buf == NULL);
		ret = -EINVAL_I2C;
		goto err;
	}
	if (g_i2c[0] == i2c || g_i2c[1] == i2c) {
		I2CERR("mt-i2c%d: Current I2C Adapter is busy.\n", i2c->id);
		ret = -EINVAL_I2C;
		goto err;
	}
	
	_i2c_translate_msg(i2c, msg);
#ifdef I2C_DRIVER_IN_KERNEL
	
	if (i2c->i2c_3dcamera_flag) {
		if (g_msg[0].buf == NULL)
			memcpy((void *)&g_msg[0], msg, sizeof(struct mt_i2c_msg));
		else
			memcpy((void *)&g_msg[1], msg, sizeof(struct mt_i2c_msg));
	}
#endif
	
	mt_i2c_clock_enable(i2c);
	return_value = _i2c_transfer_interface(i2c);
	if (!(msg->ext_flag & I2C_3DCAMERA_FLAG))
		mt_i2c_clock_disable(i2c);
	if (return_value < 0) {
		ret = -EINVAL_I2C;
		goto err;
	}
err:
	return ret;
}

static s32 mt_i2c_do_transfer(struct mt_i2c_t *i2c, struct mt_i2c_msg *msgs, s32 num)
{
	s32 ret = 0;
	s32 left_num = num;

	while (left_num--) {
		ret = mt_i2c_start_xfer(i2c, msgs++);
		if (ret < 0) {
			if (ret != -EINVAL_I2C)	
				return -EAGAIN;
			else
				return -EINVAL_I2C;
		}
	}
	
	return num;
}

#ifndef CONFIG_MTK_I2C_EXTENSION
static s32 standard_i2c_start_xfer(struct mt_i2c_t *i2c, struct i2c_msg *msg)
{
	s32 return_value = 0;
	s32 ret = msg->len;
	u8 *temp_for_dma = 0;
	bool dma_need_copy_back = false;
	struct mt_i2c_msg msg_ext;
	
	
	
	

	msg_ext.addr = msg->addr;
	msg_ext.flags = msg->flags;
	msg_ext.len = msg->len;
	msg_ext.buf = msg->buf;
#ifdef COMPATIBLE_WITH_AOSP
	msg_ext.ext_flag = msg->ext_flag;
	if (msg->timing <= 0) {
		msg_ext.timing = i2c->default_speed;
		
	} else {
		msg_ext.timing = msg->timing;
		
	}
#else
	msg_ext.ext_flag = 0;
	msg_ext.timing = i2c->default_speed;
#endif
	
#ifdef COMPATIBLE_WITH_AOSP
	if (((msg->ext_flag & 0xffff) & I2C_DMA_FLAG) && (msg->ext_flag >> 16 == 0xdead)) {
		
		
	} else {
		
		if (msg->len > I2C_FIFO_SIZE) {
			
			
			dma_need_copy_back = true;
			msg_ext.ext_flag |= I2C_DMA_FLAG;
			temp_for_dma = msg_ext.buf;
			memcpy(i2c->dma_buf.vaddr, (msg_ext.buf), (msg_ext.len & 0x00FF));
			msg_ext.buf = (u8 *) i2c->dma_buf.paddr;
			
		}
	}
#else
	
	if (msg->len > I2C_FIFO_SIZE) {
		
		dma_need_copy_back = true;
		msg_ext.ext_flag |= I2C_DMA_FLAG;
		temp_for_dma = msg_ext.buf;
		memcpy(i2c->dma_buf.vaddr, (msg_ext.buf), (msg_ext.len & 0x00FF));
		msg_ext.buf = (u8 *) mt_i2c_bus_to_virt(i2c->dma_buf.paddr);
	}
#endif
	

	i2c->read_flag = (msg_ext.flags & I2C_M_RD);
	i2c->addr = msg_ext.addr;

	if (i2c->addr == 0) {
		I2CERR(" addr is invalid.\n");
		I2C_BUG_ON(i2c->addr == NULL);
		ret = -EINVAL_I2C;
		goto err;
	}


	if (msg->buf == NULL) {
		I2CERR(" data buffer is NULL.\n");
		I2C_BUG_ON(msg->buf == NULL);
		ret = -EINVAL_I2C;
		goto err;
	}

	if (g_i2c[0] == i2c || g_i2c[1] == i2c) {
		I2CERR("mt-i2c%d: Current I2C Adapter is busy.\n", i2c->id);
		ret = -EINVAL_I2C;
		goto err;
	}
	
	_i2c_translate_msg(i2c, &msg_ext);
	
	mt_i2c_clock_enable(i2c);
	return_value = _i2c_transfer_interface(i2c);

	if (true == dma_need_copy_back) {
		
		memcpy(temp_for_dma, i2c->dma_buf.vaddr, msg->len & 0xFF);
		msg->buf = temp_for_dma;
		
	}

	if (!(msg_ext.ext_flag & I2C_3DCAMERA_FLAG))
		mt_i2c_clock_disable(i2c);
	if (return_value < 0) {
		ret = -EINVAL_I2C;
		goto err;
	}
err:
	return ret;
}

static s32 standard_i2c_do_transfer(struct mt_i2c_t *i2c, struct i2c_msg *msgs, s32 num)
{
	s32 ret = 0;
	s32 left_num = num;

	while (left_num--) {
		ret = standard_i2c_start_xfer(i2c, msgs++);
		if (ret < 0) {
			if (ret != -EINVAL_I2C)	
				return -EAGAIN;
			else
				return -EINVAL_I2C;
		}
	}
	
	return num;
}

static s32 standard_i2c_transfer(struct i2c_adapter *adap, struct i2c_msg msgs[], s32 num)
{
	s32 ret = 0;
	s32 retry;
	struct mt_i2c_t *i2c = i2c_get_adapdata(adap);

	mutex_lock(&i2c->mutex);
	for (retry = 0; retry < adap->retries; retry++) {
		ret = standard_i2c_do_transfer(i2c, msgs, num);
		if (ret != -EAGAIN)
			break;
		if (retry < adap->retries - 1)
			udelay(100);
	}
	mutex_unlock(&i2c->mutex);

	if (ret != -EAGAIN)
		return ret;
	else
		return -EREMOTEIO;
}
#endif

int mtk_i2c_transfer(struct i2c_adapter *adap, struct mt_i2c_msg msgs[], s32 num)
{
	s32 ret = 0;
	s32 retry;
	struct mt_i2c_t *i2c = i2c_get_adapdata(adap);

	mutex_lock(&i2c->mutex);
	for (retry = 0; retry < adap->retries; retry++) {
		ret = mt_i2c_do_transfer(i2c, msgs, num);
		if (ret != -EAGAIN)
			break;
		if (retry < adap->retries - 1)
			udelay(100);
	}
	mutex_unlock(&i2c->mutex);

	if (ret != -EAGAIN)
		return ret;
	else
		return -EREMOTEIO;
}

int mtk_i2c_master_send(const struct i2c_client *client,
			const char *buf, int count, u32 ext_flag, u32 timing)
{
	int ret;
	struct i2c_adapter *adap = client->adapter;
	struct mt_i2c_msg msg;

	msg.addr = client->addr;
	msg.flags = client->flags & I2C_M_TEN;
	msg.len = count;
	msg.timing = timing;
	msg.buf = (char *)buf;
	msg.ext_flag = ext_flag;
	ret = mtk_i2c_transfer(adap, &msg, 1);

	return (ret == 1) ? count : ret;
}
EXPORT_SYMBOL(mtk_i2c_master_send);

int mtk_i2c_master_recv(const struct i2c_client *client,
			char *buf, int count, u32 ext_flag, u32 timing)
{
	struct i2c_adapter *adap = client->adapter;
	struct mt_i2c_msg msg;
	int ret;

	msg.addr = client->addr;
	msg.flags = client->flags & I2C_M_TEN;
	msg.timing = timing;
	msg.flags |= I2C_M_RD;
	msg.len = count;
	msg.buf = buf;
	msg.ext_flag = ext_flag;
	ret = mtk_i2c_transfer(adap, &msg, 1);

	return (ret == 1) ? count : ret;
}
EXPORT_SYMBOL(mtk_i2c_master_recv);

#ifdef I2C_DRIVER_IN_KERNEL
static s32 _i2c_deal_result_3dcamera(struct mt_i2c_t *i2c, struct mt_i2c_msg *msg)
{
	u16 addr = msg->addr;
	u16 read = (msg->flags & I2C_M_RD);

	i2c->msg_buf = msg->buf;
	i2c->msg_len = msg->len;
	i2c->addr = read ? ((addr << 1) | 0x1) : ((addr << 1) & ~0x1);
	return _i2c_deal_result(i2c);
}
#endif

static void mt_i2c_clock_enable(struct mt_i2c_t *i2c)
{
#if (!defined(CONFIG_MT_I2C_FPGA_ENABLE))
	if (i2c->dma_en) {
		I2CINFO(I2C_T_TRANSFERFLOW, "Before dma clock enable .....\n");
#ifdef CONFIG_MTK_CLKMGR
		enable_clock(MT_CG_APDMA_SW_CG, "i2c");
#else
		clk_prepare_enable(i2c->clk_dma);
#endif
	}
	I2CINFO(I2C_T_TRANSFERFLOW, "Before i2c clock enable .....\n");
#ifdef CONFIG_MTK_CLKMGR
	enable_clock(i2c->pdn, "i2c");
#else
	clk_prepare_enable(i2c->clk_main);
#endif
	I2CINFO(I2C_T_TRANSFERFLOW, "clock enable done.....\n");
#endif
}

static void mt_i2c_clock_disable(struct mt_i2c_t *i2c)
{
#if (!defined(CONFIG_MT_I2C_FPGA_ENABLE))
	if (i2c->dma_en) {
		I2CINFO(I2C_T_TRANSFERFLOW, "Before dma clock disable .....\n");
#ifdef CONFIG_MTK_CLKMGR
		disable_clock(MT_CG_APDMA_SW_CG, "i2c");
#else
		clk_disable_unprepare(i2c->clk_dma);
#endif
	}
	I2CINFO(I2C_T_TRANSFERFLOW, "Before i2c clock disable .....\n");
#ifdef CONFIG_MTK_CLKMGR
	disable_clock(i2c->pdn, "i2c");
#else
	clk_disable_unprepare(i2c->clk_main);
#endif
	I2CINFO(I2C_T_TRANSFERFLOW, "clock disable done.....\n");
#endif
}

#ifdef CONFIG_TRUSTONIC_TEE_SUPPORT
int i2c_tui_enable_clock(void)
{
	struct i2c_adapter *adap;
	struct mt_i2c_t *i2c;

	adap = i2c_get_adapter(0);
	if (!adap) {
		I2CERR("Cannot get adapter\n");
		return -1;
	}

	i2c = i2c_get_adapdata(adap);
	clk_prepare_enable(i2c->clk_main);
	clk_prepare_enable(i2c->clk_dma);

	return 0;
}

int i2c_tui_disable_clock(void)
{
	struct i2c_adapter *adap;
	struct mt_i2c_t *i2c;

	adap = i2c_get_adapter(0);
	if (!adap) {
		I2CERR("Cannot get adapter\n");
		return -1;
	}

	i2c = i2c_get_adapdata(adap);
	clk_disable_unprepare(i2c->clk_dma);
	clk_disable_unprepare(i2c->clk_main);

	return 0;
}
#endif

static irqreturn_t mt_i2c_irq(s32 irqno, void *dev_id)
{
	struct mt_i2c_t *i2c = (struct mt_i2c_t *) dev_id;
	

	I2CINFO(I2C_T_TRANSFERFLOW, "i2c interrupt coming.....\n");
	
	
	i2c_writel(i2c, OFFSET_INTR_MASK,
		   i2c_readl(i2c,
			     OFFSET_INTR_MASK) & ~(I2C_HS_NACKERR | I2C_ACKERR | I2C_TRANSAC_COMP));
	
	i2c->irq_stat = i2c_readl(i2c, OFFSET_INTR_STAT);
	
	i2c_writel(i2c, OFFSET_INTR_STAT, (I2C_HS_NACKERR | I2C_ACKERR | I2C_TRANSAC_COMP));
	

	
	atomic_set(&i2c->trans_stop, 1);
	wake_up(&i2c->wait);
	return IRQ_HANDLED;
}


s32 mt_wait4_i2c_complete(void)
{
	struct mt_i2c_t *i2c0 = g_i2c[0];
	struct mt_i2c_t *i2c1 = g_i2c[1];
	s32 result0, result1;
	s32 ret = 0;

	if ((i2c0 == NULL) || (i2c1 == NULL)) {
		
		ret = -EINVAL_I2C;
		goto end;
	}

	result0 = _i2c_deal_result_3dcamera(i2c0, &g_msg[0]);
	result1 = _i2c_deal_result_3dcamera(i2c1, &g_msg[1]);

	if (result0 < 0 || result1 < 0)
		ret = -EINVAL_I2C;
	if (NULL != i2c0)
		mt_i2c_clock_disable(i2c0);
	if (NULL != i2c1)
		mt_i2c_clock_disable(i2c1);

end:
	g_i2c[0] = NULL;
	g_i2c[1] = NULL;

	g_msg[0].buf = NULL;
	g_msg[1].buf = NULL;

	return ret;
}

static u32 mt_i2c_functionality(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static struct i2c_algorithm mt_i2c_algorithm = {
#ifdef CONFIG_MTK_I2C_EXTENSION
	.master_xfer = (pmaster_xfer)mtk_i2c_transfer,
#else
	.master_xfer = standard_i2c_transfer,
#endif
	.functionality = mt_i2c_functionality,
};

static inline void mt_i2c_init_hw(struct mt_i2c_t *i2c)
{
	i2c_writel(i2c, OFFSET_SOFTRESET, 0x0001);
	i2c_writel(i2c, OFFSET_DCM_EN, 0x0);
}

static void mt_i2c_free(struct mt_i2c_t *i2c)
{
	if (!i2c)
		return;

	free_irq(i2c->irqnr, i2c);
	i2c_del_adapter(&i2c->adap);
	kfree(i2c);
}

static struct pinctrl_state *
i2c_gpio_get_state(struct mt_i2c_t *i2c, const char *name)
{
	struct pinctrl_state *pin_state
			= pinctrl_lookup_state(i2c->pinctrl, name);

	if (IS_ERR_OR_NULL(pin_state))
		dev_info(i2c->dev, "note pinctrl_lookup_state(%s) err:%ld\n",
						name, PTR_ERR(pin_state));
	return pin_state;
}

static int i2c_gpio_pinctrl_init(struct mt_i2c_t *i2c)
{
	i2c->pinctrl = devm_pinctrl_get(i2c->dev);
	if (IS_ERR_OR_NULL(i2c->pinctrl)) {
		dev_err(i2c->dev, "error devm_pinctrl_get() failed err:%ld\n",
			PTR_ERR(i2c->pinctrl));
		return PTR_ERR(i2c->pinctrl);
	}

	i2c->gpio_state_active = i2c_gpio_get_state(i2c, "i2c_func");

	i2c->gpio_state_recovery = i2c_gpio_get_state(i2c, "i2c_recovery");

	return 0;
}

static s32 mt_i2c_probe(struct platform_device *pdev)
{
	int ret, irq = 0;
	struct mt_i2c_t *i2c = NULL;
	struct resource *res;

	
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL)
		return -ENODEV;

	
	if (!request_mem_region(res->start, resource_size(res), pdev->name))
		return -ENOMEM;

	i2c = kzalloc(sizeof(struct mt_i2c_t), GFP_KERNEL);
	if (NULL == i2c)
		return -ENOMEM;

#ifdef CONFIG_OF
	i2c->base = of_iomap(pdev->dev.of_node, 0);
	if (!i2c->base) {
		I2CERR("I2C iomap failed\n");
		ret = -ENODEV;
		goto free1;
	}

	if (of_property_read_u32(pdev->dev.of_node, "cell-index", &pdev->id)) {
		I2CERR("I2C get cell-index failed\n");
		ret = -ENODEV;
		goto free1;
	}
	i2c->id = pdev->id;

	irq = irq_of_parse_and_map(pdev->dev.of_node, 0);
	if (!irq) {
		I2CERR("I2C get irq failed\n");
		ret = -ENODEV;
		goto free1;
	}

	if (of_property_read_u32(pdev->dev.of_node, "def_speed", &pdev->id)) {
		
		i2c->default_speed = 100;
	} else {
		i2c->default_speed = pdev->id;
	}

	if (of_property_read_u32(pdev->dev.of_node, "recover_clk_cnt", &pdev->id)) {
		i2c->recover_clk_cnt = 0;
		pr_err("I2C get recover_clk_cnt failed, recover_clk_cnt = %d\n",
		       i2c->recover_clk_cnt);
	} else {
		i2c->recover_clk_cnt = pdev->id;
		pr_info("I2C get recover_clk_cnt  = %d\n", i2c->recover_clk_cnt);
	}

	if (of_property_read_u32(pdev->dev.of_node, "scl-gpio", &pdev->id)) {
		i2c->scl_gpio = 0;
		pr_err("I2C get scl_gpio failed, scl_gpio = %d\n",
		       i2c->scl_gpio);
	} else {
		i2c->scl_gpio = pdev->id;
		pr_info("I2C get scl_gpio  = %d\n", i2c->scl_gpio);
	}

	if (of_property_read_u32(pdev->dev.of_node, "sda-gpio", &pdev->id)) {
		i2c->sda_gpio = 0;
		pr_err("I2C get sda_gpio failed, sda_gpio = %d\n",
		       i2c->sda_gpio);
	} else {
		i2c->sda_gpio = pdev->id;
		pr_info("I2C get sda_gpio  = %d\n", i2c->sda_gpio);
	}
#else

	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		return -ENODEV;

	
	i2c->id = pdev->id;
	i2c->base = IO_PHYS_TO_VIRT(res->start);

#endif
	i2c->irqnr = irq;

#ifdef CONFIG_MTK_CLKMGR

#if (defined(CONFIG_MT_I2C_FPGA_ENABLE))
	i2c->clk = I2C_CLK_RATE;
#else
	
	i2c->clk = I2C_CLK_RATE;

	switch (i2c->id) {
	case 0:
		i2c->pdn = MT_CG_I2C0_SW_CG;
		break;
	case 1:
		i2c->pdn = MT_CG_I2C1_SW_CG;
		break;
	case 2:
		i2c->pdn = MT_CG_I2C2_SW_CG;
		break;
	case 3:
		i2c->pdn = MT_CG_I2C3_SW_CG;
		break;
	case 4:
		i2c->pdn = MT_CG_I2C4_SW_CG;
		break;
	default:
		dev_err(&pdev->dev, "Error id %d\n", i2c->id);
		break;
	}
#endif

#else
	i2c->clk = I2C_CLK_RATE;
	
	

	i2c->clk_main = devm_clk_get(&pdev->dev, "main");
	if (IS_ERR(i2c->clk_main)) {
		I2CERR("cannot get main clock, err=%ld\n", PTR_ERR(i2c->clk_main));
		
		ret = PTR_ERR(i2c->clk_main);
		goto free1;
	}

	i2c->clk_dma = devm_clk_get(&pdev->dev, "dma");
	if (IS_ERR(i2c->clk_dma)) {
		I2CERR("cannot get dma clock, err=%ld\n", PTR_ERR(i2c->clk_dma));
		
		ret = PTR_ERR(i2c->clk_dma);
		goto free1;
	}
#endif

	i2c->dev = &i2c->adap.dev;

	i2c->adap.dev.parent = &pdev->dev;
#ifdef CONFIG_OF
	i2c->adap.dev.of_node = pdev->dev.of_node;
#endif
	i2c->adap.nr = i2c->id;
	i2c->adap.owner = THIS_MODULE;
	i2c->adap.algo = &mt_i2c_algorithm;
	i2c->adap.algo_data = NULL;
	i2c->adap.timeout = 2 * HZ;	
	i2c->adap.retries = 1;	
	
	i2c->dma_buf.vaddr =
	    dma_alloc_coherent(&pdev->dev, MAX_DMA_TRANS_NUM,
			       &i2c->dma_buf.paddr, GFP_KERNEL | GFP_DMA32);


	if (!i2c->dma_buf.vaddr)
	{
		I2CLOG("mt-i2c:[Error] Allocate DMA I2C Buffer failed!\n");
		return PTR_ERR(i2c->dma_buf.vaddr);
	}
	memset(i2c->dma_buf.vaddr, 0, MAX_DMA_TRANS_NUM);
	snprintf(i2c->adap.name, sizeof(i2c->adap.name), dev_name(&pdev->dev));

#ifdef CONFIG_OF
	i2c->pdmabase = DMA_I2C_BASE(i2c->id, ap_dma_base);
#else
	i2c->pdmabase = DMA_I2C_BASE_CH(i2c->id);
#endif

	I2CLOG(" id: %d, reg: 0x%p, dma_reg: 0x%p, irq: %d\n", i2c->id, i2c->base, i2c->pdmabase,
	       i2c->irqnr);

	spin_lock_init(&i2c->lock);
	mutex_init(&i2c->mutex);
	init_waitqueue_head(&i2c->wait);

	mt_i2c_clock_enable(i2c);
	mt_i2c_init_hw(i2c);
	mt_i2c_clock_disable(i2c);

	ret = request_irq(i2c->irqnr, mt_i2c_irq, IRQF_TRIGGER_LOW,
			  dev_name(&pdev->dev), i2c);

	if (ret) {
		dev_err(&pdev->dev, "Can Not request I2C IRQ %d\n", i2c->irqnr);
		goto free;
	}
	
	I2CLOG("i2c-bus%d speed is %dKhz\n", i2c->id, i2c->default_speed);

	i2c_set_adapdata(&i2c->adap, i2c);
	ret = i2c_add_numbered_adapter(&i2c->adap);
	if (ret) {
		dev_err(&pdev->dev, "failed to add i2c bus to i2c core\n");
		goto free;
	}
	platform_set_drvdata(pdev, i2c);

	i2c_gpio_pinctrl_init(i2c);

#ifdef I2C_DEBUG_FS
	ret = device_create_file(i2c->dev, &dev_attr_debug);
	if (ret)
		I2CERR("i2c create attr file failed\n");
#endif

	return ret;

free:
	mt_i2c_free(i2c);
	I2CERR("i2c probe fail\n");
	return ret;
free1:
	if(i2c)
		kfree(i2c);
	I2CERR("i2c probe fail\n");
	return ret;

}


static s32 mt_i2c_remove(struct platform_device *pdev)
{
	struct mt_i2c_t *i2c = platform_get_drvdata(pdev);

	if (i2c) {
		platform_set_drvdata(pdev, NULL);
		mt_i2c_free(i2c);
	}
	return 0;
}

#ifdef CONFIG_PM
static s32 mt_i2c_suspend(struct platform_device *pdev, pm_message_t state)
{
	
	
	return 0;
}

static s32 mt_i2c_resume(struct platform_device *pdev)
{
	
	
	return 0;
}
#else
#define mt_i2c_suspend	NULL
#define mt_i2c_resume	NULL
#endif

static const struct of_device_id mt_i2c_of_match[] = {
	{.compatible = "mediatek,mt6755-i2c",},
	{},
};

MODULE_DEVICE_TABLE(of, mt_i2c_of_match);

static struct platform_driver mt_i2c_driver = {
	.probe = mt_i2c_probe,
	.remove = mt_i2c_remove,
	.suspend = mt_i2c_suspend,
	.resume = mt_i2c_resume,
	.driver = {
		.name = I2C_DRV_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = mt_i2c_of_match,
#endif
	},
};

static s32 __init mt_i2c_init(void)
{
#ifdef CONFIG_OF
	struct device_node *ap_dma_node;

	
	ap_dma_node = of_find_compatible_node(NULL, NULL, "mediatek,ap_dma");
	if (!ap_dma_node) {
		I2CERR("Cannot find AP_DMA node\n");
		return -ENODEV;
	}

	ap_dma_base = of_iomap(ap_dma_node, 0);
	if (!ap_dma_base) {
		I2CERR("AP_DMA iomap failed\n");
		return -ENOMEM;
	}
#endif

	return platform_driver_register(&mt_i2c_driver);
}

static void __exit mt_i2c_exit(void)
{
	platform_driver_unregister(&mt_i2c_driver);
}

module_init(mt_i2c_init);
module_exit(mt_i2c_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek I2C Bus Driver");
MODULE_AUTHOR("Infinity Chen <infinity.chen@mediatek.com>");
