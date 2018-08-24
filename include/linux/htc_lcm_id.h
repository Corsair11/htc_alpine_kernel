#ifndef HTC_LCM_ID_H
#define HTC_LCM_ID_H

enum {
	EVM_SOURCE = 0,
	REAL_SOURCE,
	MAIN_SOURCE,
	SECOND_SOURCE,
	NULL_PANEL,
	UNKNOWN_SOURCE,
};

int htc_get_lcm_id(void);

#endif
