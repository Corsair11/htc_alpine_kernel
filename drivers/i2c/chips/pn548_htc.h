






#ifndef NFC_READ_RFSKUID
#define NFC_READ_RFSKUID 0
#endif

#ifndef NFC_GET_BOOTMODE
#define NFC_GET_BOOTMODE 0
#endif

#ifndef NFC_OFF_MODE_CHARGING_ENABLE
#define NFC_OFF_MODE_CHARGING_ENABLE 0
#endif

#ifndef NFC_POWER_OFF_SEQUENCE
#define NFC_POWER_OFF_SEQUENCE 0
#endif

#ifndef NFC_PVDD_GPIO_DT
#define NFC_PVDD_GPIO_DT 0
#endif

#ifndef NFC_PVDD_LOAD_SWITCH
#define NFC_PVDD_LOAD_SWITCH 0
#endif

#ifndef NFC_READ_RFSKUID_MTK6795
#define NFC_READ_RFSKUID_MTK6795 0
#endif 

#ifndef NFC_READ_RFSKUID_A53ML
#define NFC_READ_RFSKUID_A53ML 0
#endif 

#ifdef NFC_PLL_CLOCK_ENABLE
enum clk_buf_id {
	CLK_BUF_BB_MD		= 0,
	CLK_BUF_CONN		= 1,
	CLK_BUF_NFC		= 2,
	CLK_BUF_AUDIO		= 3,
	CLK_BUF_INVALID		= 4,
};
#endif

#define NFC_BOOT_MODE_NORMAL 0
#define NFC_BOOT_MODE_FTM 1
#define NFC_BOOT_MODE_DOWNLOAD 2
#define NFC_BOOT_MODE_OFF_MODE_CHARGING 5



int pn548_htc_check_rfskuid(int in_is_alive);

int pn548_htc_get_bootmode(void);


bool pn548_htc_parse_dt(struct device *dev);


void pn548_htc_turn_off_pvdd(void);


void pn548_power_off_seq(int is_alive);


bool pn548_htc_turn_on_pvdd(struct i2c_client *client);

#ifdef NFC_PLL_CLOCK_ENABLE
int pn548_pll_clk_ctrl(int onoff);
#endif

#ifdef NFC_GPIO_PINCTRL_ENABLE
bool pn548_htc_check_pvdd_gpio(void);
unsigned int pn548_htc_get_pvdd_gpio(void);
#endif

