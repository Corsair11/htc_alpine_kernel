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
/*******************************************************************************
 *
 * Filename:
 * ---------
 *	mt_soc_pcm_voice_usb_echoref.c
 *
 * Project:
 * --------
 *	MT6797
 *
 * Description:
 * ------------
 *	Platform driver for usb phone call echo reference path
 *
 * Author:
 * -------
 *	Kai Chieh Chuang
 *
 *------------------------------------------------------------------------------
 *
 *
 *******************************************************************************/


/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include <linux/dma-mapping.h>
#include "AudDrv_Common.h"
#include "AudDrv_Def.h"
#include "AudDrv_Afe.h"
#include "AudDrv_Clk.h"
#include "mt_soc_afe_control.h"
#include "mt_soc_digital_type.h"
#include "mt_soc_pcm_common.h"
#include "AudDrv_Common_func.h"

#ifdef DEBUG_VOICE_USB_PLAYBACK
#define pr_usbp(format, args...)  pr_debug(format, ##args)
#else
#define pr_usbp(format, args...)
#endif

#ifdef DEBUG_VOICE_USB_CAPTURE
#define pr_usbc(format, args...)  pr_debug(format, ##args)
#else
#define pr_usbc(format, args...)
#endif

#if 0
/* debug */
#define NUM_DBG_LOG 60
#define DBG_LOG_LENGTH 256

struct usb_dbg_log {
	unsigned int idx;
	char log[DBG_LOG_LENGTH];
};

static struct usb_dbg_log dbg_log[NUM_DBG_LOG];
static unsigned int dbg_log_idx;

static void print_usb_dbg_log(void)
{
	unsigned int i = 0;

	for (i = 0; i < NUM_DBG_LOG; i++) {
		pr_err("%s(), idx %u, %s\n",
		       __func__, dbg_log[i].idx, dbg_log[i].log);
	}
}
#endif
/*
 *    function implementation
 */
static bool usb_prepare_done[2] = {false, false};
static bool usb_use_dram;
static int usb_mem_blk[2] = {Soc_Aud_Digital_Block_MEM_DL1,
			     Soc_Aud_Digital_Block_MEM_AWB};
static int usb_irq[2] = {Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE,
			 Soc_Aud_IRQ_MCU_MODE_IRQ2_MCU_MODE};

static struct snd_pcm_hardware mtk_pcm_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP |
	SNDRV_PCM_INFO_INTERLEAVED |
	SNDRV_PCM_INFO_RESUME |
	SNDRV_PCM_INFO_MMAP_VALID),
	.formats =      SND_SOC_ADV_MT_FMTS,
	.rates =        SOC_HIGH_USE_RATE,
	.rate_min =     SOC_HIGH_USE_RATE_MIN,
	.rate_max =     SOC_HIGH_USE_RATE_MAX,
	.channels_min =     SOC_NORMAL_USE_CHANNELS_MIN,
	.channels_max =     SOC_NORMAL_USE_CHANNELS_MAX,
	.buffer_bytes_max = MAX_BUFFER_SIZE,
	.period_bytes_max = MAX_PERIOD_SIZE,
	.periods_min =      2,
	.periods_max =      256,
	.fifo_size =        0,
};

static int usb_md_select;
static const char * const md_choose[] = {"md1", "md2"};
static const struct soc_enum speech_usb_enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(md_choose), md_choose),
};

static int Audio_USB_MD_Select_Control_Get(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("%s(), usb_md_select = %d\n", __func__, usb_md_select);
	ucontrol->value.integer.value[0] = usb_md_select;
	return 0;
}

static int Audio_USB_MD_Select_Control_Set(struct snd_kcontrol *kcontrol,
				       struct snd_ctl_elem_value *ucontrol)
{
	if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(md_choose)) {
		pr_warn("return -EINVAL\n");
		return -EINVAL;
	}
	usb_md_select = ucontrol->value.integer.value[0];
	pr_warn("%s(), usb_md_select = %d\n", __func__, usb_md_select);
	return 0;
}

enum USB_DBG_TYPE {
	USB_DBG_ASSERT_AT_STOP = 0x1 << 0,
};

static int usb_debug_enable;
static int Audio_USB_Debug_Get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = usb_debug_enable;
	return 0;

}

static int Audio_USB_Debug_Set(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *ucontrol)
{
	usb_debug_enable = ucontrol->value.integer.value[0];
	pr_warn("%s(), usb_debug_enable = 0x%x\n", __func__, usb_debug_enable);

	return 0;
}

static const struct snd_kcontrol_new speech_usb_controls[] = {
	SOC_ENUM_EXT("USB_EchoRef_Modem_Select",
		     speech_usb_enum[0],
		     Audio_USB_MD_Select_Control_Get,
		     Audio_USB_MD_Select_Control_Set),
	SOC_SINGLE_EXT("USB_EchoRef_Voice_Debug", SND_SOC_NOPM, 0, 0xFFFFFFFF, 0,
		       Audio_USB_Debug_Get, Audio_USB_Debug_Set),
};

static int mtk_usb_echoref_close(struct snd_pcm_substream *substream)
{
	int stream = substream->stream;

	pr_warn("%s(), stream %d, prepare %d\n",
		__func__, stream, usb_prepare_done[stream]);

	if (usb_prepare_done[substream->stream]) {
		usb_prepare_done[substream->stream] = false;
		RemoveMemifSubStream(usb_mem_blk[stream], substream);

		if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
			if (usb_md_select) {
				SetConnection(Soc_Aud_InterCon_DisConnect,
					      Soc_Aud_InterConnectionInput_I05,
					      Soc_Aud_InterConnectionOutput_O27);
			} else {
				SetConnection(Soc_Aud_InterCon_DisConnect,
					      Soc_Aud_InterConnectionInput_I05,
					      Soc_Aud_InterConnectionOutput_O24);
			}

			/* resume pbuf size */
			set_memif_pbuf_size(usb_mem_blk[stream], MEMIF_PBUF_SIZE_256_BYTES);
		}
	}

	EnableAfe(false);
	AudDrv_Clk_Off();

	return 0;
}

static int mtk_usb_echoref_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret = 0;

	AudDrv_Clk_On();

	pr_warn("%s()\n", __func__);

	runtime->hw = mtk_pcm_hardware;
	memcpy((void *)(&(runtime->hw)), (void *)&mtk_pcm_hardware, sizeof(struct snd_pcm_hardware));

	ret = snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);

	if (ret < 0)
		pr_warn("snd_pcm_hw_constraint_integer failed\n");

	if (ret < 0) {
		mtk_usb_echoref_close(substream);
		return ret;
	}

	pr_warn("%s(), return\n", __func__);
	return 0;
}

static int mtk_usb_echoref_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int stream = substream->stream;

	pr_warn("%s(), stream %d, rate = %d, channels = %d period_size = %lu, prepare %d\n",
		__func__,
		stream,
		runtime->rate,
		runtime->channels,
		runtime->period_size,
		usb_prepare_done[stream]);

	if (!usb_prepare_done[stream]) {
		SetMemifSubStream(usb_mem_blk[stream], substream);

		/* set memif format */
		if (runtime->format == SNDRV_PCM_FORMAT_S32_LE ||
		    runtime->format == SNDRV_PCM_FORMAT_U32_LE)
			SetMemIfFetchFormatPerSample(usb_mem_blk[stream],
						     AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
		else
			SetMemIfFetchFormatPerSample(usb_mem_blk[stream],
						     AFE_WLEN_16_BIT);

		SetSampleRate(usb_mem_blk[stream], runtime->rate);
		SetChannels(usb_mem_blk[stream], runtime->channels);
		if (runtime->channels == 1)
			SetMemifMonoSel(usb_mem_blk[stream], false);

		/* set pbuf size for latency */
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			set_memif_pbuf_size(usb_mem_blk[stream], MEMIF_PBUF_SIZE_32_BYTES);

		/* set connection */
		if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
			if (usb_md_select) {
				SetConnection(Soc_Aud_InterCon_Connection,
					      Soc_Aud_InterConnectionInput_I05,
					      Soc_Aud_InterConnectionOutput_O27);
			} else {
				SetConnection(Soc_Aud_InterCon_Connection,
					      Soc_Aud_InterConnectionInput_I05,
					      Soc_Aud_InterConnectionOutput_O24);
			}
		}

		usb_prepare_done[stream] = true;
	}

	ClearMemBlock(usb_mem_blk[stream]);

	EnableAfe(true);
	return 0;
}

static void SetDL1Buffer(struct snd_pcm_substream *substream,
			 struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int stream = substream->stream;
	AFE_BLOCK_T *pblock = &Get_Mem_ControlT(usb_mem_blk[stream])->rBlock;

	pblock->pucPhysBufAddr =  runtime->dma_addr;
	pblock->pucVirtBufAddr =  runtime->dma_area;
	pblock->u4BufferSize = runtime->dma_bytes;
	pblock->u4SampleNumMask = 0x001f;  /* 32 byte align */
	pblock->u4WriteIdx     = 0;
	pblock->u4DMAReadIdx    = 0;
	pblock->u4DataRemained  = 0;
	pblock->u4fsyncflag     = false;
	pblock->uResetFlag      = true;
	pr_warn("SetDL1Buffer u4BufferSize = %d pucVirtBufAddr = %p pucPhysBufAddr = 0x%x\n",
	       pblock->u4BufferSize, pblock->pucVirtBufAddr, pblock->pucPhysBufAddr);
	/* set dram address top hardware */
	Afe_Set_Reg(AFE_DL1_BASE , pblock->pucPhysBufAddr , 0xffffffff);
	Afe_Set_Reg(AFE_DL1_END  , pblock->pucPhysBufAddr + (pblock->u4BufferSize - 1),
		    0xffffffff);
	memset_io((void *)pblock->pucVirtBufAddr, 0, pblock->u4BufferSize);
}

static int mtk_usb_echoref_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int stream = substream->stream;
	int ret = 0;

	runtime->dma_bytes = params_buffer_bytes(hw_params);

	if (false/*AllocateAudioSram(&runtime->dma_addr,
			      &runtime->dma_area,
			      runtime->dma_bytes,
			      substream) == 0*/) {
		usb_use_dram = false;
		SetHighAddr(usb_mem_blk[stream], false);
	} else {
		ret = snd_pcm_lib_malloc_pages(substream,
					       runtime->dma_bytes);
		if (ret < 0) {
			pr_err("%s(), allocate dram fail, ret %d\n",
			       __func__, ret);
			return ret;
		}
		usb_use_dram = true;
		SetHighAddr(usb_mem_blk[stream], true);
		AudDrv_Emi_Clk_On();
	}

	if (stream == SNDRV_PCM_STREAM_PLAYBACK)
		SetDL1Buffer(substream, hw_params);

	pr_warn("%s(), substream %p, stream %d, dma_bytes = %zu, dma_area = %p, dma_addr = 0x%lx, use_dram %d\n",
		__func__, substream, stream,
		runtime->dma_bytes, runtime->dma_area,
		(long)runtime->dma_addr, usb_use_dram);

	return ret;
}

static int mtk_usb_echoref_hw_free(struct snd_pcm_substream *substream)
{
	pr_warn("%s(), substream = %p, stream %d\n", __func__, substream, substream->stream);

	if (usb_use_dram) {
		AudDrv_Emi_Clk_Off();
		usb_use_dram = false;
		return snd_pcm_lib_free_pages(substream);
	} else {
		return 0;/*freeAudioSram((void *)substream);*/
	}
}

static int mtk_usb_echoref_start(struct snd_pcm_substream *substream)
{
	int stream = substream->stream;

	pr_warn("%s(), stream %d\n", __func__, stream);

	SetMemoryPathEnable(usb_mem_blk[stream], true);

	/* here to set interrupt */
	irq_add_user(substream,
		     usb_irq[stream],
		     substream->runtime->rate,
		     substream->runtime->period_size);

	EnableAfe(true);

	return 0;
}

static int mtk_usb_echoref_stop(struct snd_pcm_substream *substream)
{
	int stream = substream->stream;

	pr_warn("%s(), stream %d\n", __func__, stream);

#if 0
	if (usb_debug_enable & USB_DBG_ASSERT_AT_STOP) {
		if (stream == SNDRV_PCM_STREAM_PLAYBACK)
			print_usb_dbg_log();
	}
#endif

	irq_remove_user(substream, usb_irq[stream]);

	SetMemoryPathEnable(usb_mem_blk[stream], false);

	ClearMemBlock(usb_mem_blk[stream]);
	return 0;
}

static int mtk_usb_echoref_trigger(struct snd_pcm_substream *substream, int cmd)
{
	/* pr_warn("mtk_pcm_I2S0dl1_trigger cmd = %d\n", cmd); */

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_usb_echoref_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_usb_echoref_stop(substream);
	}
	return -EINVAL;
}

static snd_pcm_uframes_t mtk_usb_echoref_pointer
	(struct snd_pcm_substream *substream)
{
	unsigned int hw_ptr;
	int stream = substream->stream;
	AFE_MEM_CONTROL_T *mem_ctl = Get_Mem_ControlT(usb_mem_blk[stream]);
	AFE_BLOCK_T *Afe_Block = &mem_ctl->rBlock;

	hw_ptr = Afe_Get_Reg(AFE_DL1_CUR);
	if (hw_ptr == 0) {
		pr_warn("%s(), hw_ptr == 0\n", __func__);
		hw_ptr = Afe_Block->pucPhysBufAddr;
	}

	return bytes_to_frames(substream->runtime,
			       hw_ptr - Afe_Block->pucPhysBufAddr);
}

static struct snd_pcm_ops mtk_usb_echoref_ops = {
	.open =		mtk_usb_echoref_open,
	.close =	mtk_usb_echoref_close,
	.ioctl =	snd_pcm_lib_ioctl,
	.hw_params =	mtk_usb_echoref_hw_params,
	.hw_free =	mtk_usb_echoref_hw_free,
	.prepare =	mtk_usb_echoref_prepare,
	.trigger =	mtk_usb_echoref_trigger,
	.pointer =	mtk_usb_echoref_pointer,
};

static int mtk_usb_echoref_platform_probe(struct snd_soc_platform *platform)
{
	pr_warn("mtk_voice_platform_probe\n");
	snd_soc_add_platform_controls(platform, speech_usb_controls,
				      ARRAY_SIZE(speech_usb_controls));
	return 0;
}

static int mtk_usb_echoref_pcm_new(struct snd_soc_pcm_runtime *rtd)
{
	size_t size;
	struct snd_card *card = rtd->card->snd_card;
	struct snd_pcm *pcm = rtd->pcm;

	size = mtk_pcm_hardware.buffer_bytes_max;

	return snd_pcm_lib_preallocate_pages_for_all(pcm, SNDRV_DMA_TYPE_DEV,
						     card->dev, size, size);
}

static void mtk_usb_echoref_pcm_free(struct snd_pcm *pcm)
{
	snd_pcm_lib_preallocate_free_for_all(pcm);
}

static struct snd_soc_platform_driver mtk_soc_usb_echoref_platform = {
	.ops        = &mtk_usb_echoref_ops,
	.probe      = mtk_usb_echoref_platform_probe,
	.pcm_new    = mtk_usb_echoref_pcm_new,
	.pcm_free   = mtk_usb_echoref_pcm_free,
};

static int mtk_usb_echoref_probe(struct platform_device *pdev)
{
	pr_warn("%s()\n", __func__);

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);

	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_VOICE_USB_ECHOREF);

	pr_warn("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_platform(&pdev->dev,
					 &mtk_soc_usb_echoref_platform);
}

static int mtk_usb_echoref_remove(struct platform_device *pdev)
{
	pr_debug("%s()\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

static const struct of_device_id mt_soc_pcm_usb_echoref_of_ids[] = {
	{ .compatible = "mediatek,mt_soc_pcm_voice_usb_echoref", },
	{}
};

static struct platform_driver mtk_usb_echoref_driver = {
	.driver = {
		.name = MT_SOC_VOICE_USB_ECHOREF,
		.owner = THIS_MODULE,
		.of_match_table = mt_soc_pcm_usb_echoref_of_ids,
	},
	.probe = mtk_usb_echoref_probe,
	.remove = mtk_usb_echoref_remove,
};

module_platform_driver(mtk_usb_echoref_driver);

MODULE_DESCRIPTION("AFE PCM module platform driver");
MODULE_LICENSE("GPL");
