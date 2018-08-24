#ifdef CONFIG_USB_MTK_DUALMODE
extern struct pinctrl *pinctrl;
extern struct pinctrl_state *pinctrl_iddig;

static int usbid_pinctrl(int enable)
{
	char *pinctrrl_name;
	int ret = 0;

	pinctrrl_name = enable ? "iddig_output":"iddig_init";
	pinctrl_iddig = pinctrl_lookup_state(pinctrl, pinctrrl_name);
	
	if (IS_ERR(pinctrl_iddig)) {
		printk("[Cable] %s: Cannot find usb pinctrl: %s\n", __func__, pinctrrl_name);
		ret = -1;
	}
	else
		ret = pinctrl_select_state(pinctrl, pinctrl_iddig);

	return ret;
}

static int htc_get_usbid_adc(int channel)
{
	int id_volt = 0;
	IMM_GetOneChannelValue_Cali(channel, &id_volt);
	return id_volt/1000;
}

static int htc_second_detect(void)
{
	int adc_value = UNKNOWN_ADC;
	int id_status = 1;
	int acce_type = DOCK_STATE_UNDEFINED;
	int ret = 0;

	ret = usbid_pinctrl(1);
	id_status = mt_get_gpio_in(ints[0]);
	CABLE_INFO("[2nd] start id_status = %d, usbid_pinctrl(1)->ret = %d\n", id_status, ret);

	adc_value = htc_get_usbid_adc(AUXADC_USBID_ADC_CHANNEL);
	CABLE_INFO("[2nd] accessory adc = %d\n", adc_value);

	if ((adc_value >= 645 && adc_value <= 1010))
#ifdef CONFIG_HTC_MHL_DETECTION
		acce_type = DOCK_STATE_MHL;
#else
		acce_type = DOCK_STATE_UNDEFINED; 
#endif
	else if (adc_value <= 200)
		acce_type = DOCK_STATE_USB_HOST; 
	else
		acce_type = DOCK_STATE_UNDEFINED; 

	ret = usbid_pinctrl(0);
	id_status = mt_get_gpio_in(ints[0]);
	CABLE_INFO("[2nd] done, id_status = %d, usbid_pinctrl(0)->ret = %d\n", id_status, ret);

	return acce_type;
}
static int htc_cable_detect_get_type(void)
{
	int id_status = 1;
	static int prev_type;
	static int stable_count = 0;
	int adc_value = UNKNOWN_ADC;
	int acce_type = DOCK_STATE_UNDEFINED;

	id_status = mt_get_gpio_in(ints[0]);

	if (stable_count >= ADC_RETRY)
		stable_count = 0;
	if (id_status == 0) {
		CABLE_INFO(" %s: id pin low\n", __func__);
		adc_value = htc_get_usbid_adc(AUXADC_USBID_ADC_CHANNEL);
		CABLE_INFO("[1st] accessory adc = %d\n", adc_value);
		if (adc_value > -100 && adc_value < 90)
			acce_type = htc_second_detect();
		else {
			if (adc_value >= 90 && adc_value < 150)
				acce_type = DOCK_STATE_CAR;
			else if (adc_value > 250 && adc_value < 350)
				acce_type = DOCK_STATE_USB_HEADSET;
			else if (adc_value > 370 && adc_value < 450)
				acce_type = DOCK_STATE_DMB;
			else if (adc_value > 550 && adc_value < 900)
				acce_type = DOCK_STATE_DESK;
			else
				acce_type = DOCK_STATE_UNDEFINED;
		}
	} else {
		CABLE_INFO(" %s: id pin high, adc_value = %d\n", __func__, htc_get_usbid_adc(AUXADC_USBID_ADC_CHANNEL));
		acce_type = DOCK_STATE_UNDOCKED;
	}
	if (prev_type == acce_type)
		stable_count++;
	else
		stable_count = 0;

	CABLE_INFO(" %s: prev_type %d, acce_type %d, stable_count %d\n",
			__func__, prev_type, acce_type, stable_count);

	prev_type = acce_type;

	return (stable_count >= ADC_RETRY) ? acce_type : ADC_NON_STABLE;
}

static void  htc_accessory_ineserted(int accessory_type)
{

	switch (accessory_type) {
		case DOCK_STATE_CAR:
			CABLE_INFO("Car kit inserted\n");
			
			
			mtk_idpin_cur_stat = IDPIN_IN_DEVICE;
			mtk_set_iddig_out_detect();
			break;
		case DOCK_STATE_USB_HEADSET:
			CABLE_INFO("USB headset inserted\n");
			mtk_idpin_cur_stat = IDPIN_IN_DEVICE;
			mtk_set_iddig_out_detect();
			break;
		case DOCK_STATE_MHL:
			CABLE_INFO("MHL inserted\n");
			switch_set_state(&dock_switch, DOCK_STATE_MHL);
			mtk_idpin_cur_stat = IDPIN_IN_DEVICE;
			mtk_set_iddig_out_detect();
			break;
		case DOCK_STATE_USB_HOST:
			CABLE_INFO("USB Host inserted\n");
			switch_set_state(&dock_switch, DOCK_STATE_USB_HOST);
			break;
		case DOCK_STATE_DMB:
			CABLE_INFO("DMB inserted\n");
			
			switch_set_state(&dock_switch, DOCK_STATE_DMB);
			mtk_idpin_cur_stat = IDPIN_IN_DEVICE;
			mtk_set_iddig_out_detect();
			break;
		case DOCK_STATE_AUDIO_DOCK:
			CABLE_INFO("Audio Dock inserted\n");
			switch_set_state(&dock_switch, DOCK_STATE_DESK);
			mtk_idpin_cur_stat = IDPIN_IN_DEVICE;
			mtk_set_iddig_out_detect();
			break;
		default :
			CABLE_INFO("Unknow cable inserted\n");
			
			mtk_idpin_cur_stat = IDPIN_IN_DEVICE;
			mtk_set_iddig_out_detect();
			break;
	}
}
static void htc_accessory_removed(int accessory_type)
{
	switch (accessory_type) {
		case DOCK_STATE_CAR:
			CABLE_INFO("Car kit removed\n");
			switch_set_state(&dock_switch, DOCK_STATE_UNDOCKED);
			break;
		case DOCK_STATE_USB_HEADSET:
			CABLE_INFO("USB headset removed\n");
			break;
		case DOCK_STATE_MHL:
			CABLE_INFO("MHL removed\n");
			switch_set_state(&dock_switch, DOCK_STATE_UNDOCKED);
			break;
		case DOCK_STATE_USB_HOST:
			CABLE_INFO("USB Host removed\n");
			switch_set_state(&dock_switch, DOCK_STATE_UNDOCKED);
			break;
		case DOCK_STATE_DMB:
			CABLE_INFO("DMB removed\n");
			switch_set_state(&dock_switch, DOCK_STATE_UNDOCKED);
			break;
		case DOCK_STATE_AUDIO_DOCK:
			CABLE_INFO("Audio Dock removed\n");
			switch_set_state(&dock_switch, DOCK_STATE_UNDOCKED);
			break;
		default :
			CABLE_INFO("Unknow cable removed\n");
			switch_set_state(&dock_switch, DOCK_STATE_UNDOCKED);
			break;
	}
}
#endif
