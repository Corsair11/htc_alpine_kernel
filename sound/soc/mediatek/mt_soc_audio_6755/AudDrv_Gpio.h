/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _AUDDRV_GPIO_H_
#define _AUDDRV_GPIO_H_




#include "AudDrv_Common.h"
#include "AudDrv_Def.h"





#if !defined(CONFIG_MTK_LEGACY)
#include <linux/gpio.h>
#else
#include <mt-plat/mt_gpio.h>
#endif

#if !defined(CONFIG_MTK_LEGACY)
void AudDrv_GPIO_probe(void *dev);
int AudDrv_GPIO_PMIC_Select(int bEnable);
void AudDrv_GPIO_I2S0_probe(void *dev); 
int AudDrv_GPIO_I2S0_Select(int bEnable); 
int AudDrv_GPIO_I2S_Select(int bEnable);
int AudDrv_GPIO_EXTAMP_Select(int bEnable, int mode);
int AudDrv_GPIO_EXTAMP2_Select(int bEnable, int mode);
int AudDrv_GPIO_RCVSPK_Select(int bEnable);
int AudDrv_GPIO_HPDEPOP_Select(int bEnable);

#endif


#endif
