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
 *   mt_soc_pcm_I2S0dl1.c
 *
 * Project:
 * --------
 *    Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio I2S0dl1 and Dl1 playback
 *
 * Author:
 * -------
 * Chipeng Chang
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
#include "AudDrv_Ana.h"
#include "AudDrv_Clk.h"
#include "AudDrv_Kernel.h"
#include "mt_soc_afe_control.h"
#include "mt_soc_digital_type.h"
#include "mt_soc_pcm_common.h"

#define _DEBUG_6328_CLK

#define MAGIC_NUMBER 0xFFFFFFC0
static DEFINE_SPINLOCK(auddrv_I2S0dl1_lock);
static AFE_MEM_CONTROL_T *pI2S0dl1MemControl;
static struct snd_dma_buffer *Dl1_Playback_dma_buf;
static unsigned int mPlaybackSramState = SRAM_STATE_FREE;

#if defined(CONFIG_SND_SOC_FLORIDA)
static AudioDigtalI2S *mAudioDigitalI2S = NULL;
static void ConfigAdcI2S(struct snd_pcm_substream *substream)
{
    mAudioDigitalI2S->mLR_SWAP = Soc_Aud_LR_SWAP_NO_SWAP;
    mAudioDigitalI2S->mBuffer_Update_word = 8;
    mAudioDigitalI2S->mFpga_bit_test = 0;
    mAudioDigitalI2S->mFpga_bit = 0;
    mAudioDigitalI2S->mloopback = 0;
    mAudioDigitalI2S->mINV_LRCK = Soc_Aud_INV_LRCK_NO_INVERSE;
    mAudioDigitalI2S->mI2S_FMT = Soc_Aud_I2S_FORMAT_I2S;
    mAudioDigitalI2S->mI2S_WLEN = Soc_Aud_I2S_WLEN_WLEN_32BITS; //Soc_Aud_I2S_WLEN_WLEN_16BITS;
    mAudioDigitalI2S->mI2S_SAMPLERATE = (substream->runtime->rate);
}
#endif
/*
 *    function implementation
 */

static int mtk_I2S0dl1_probe(struct platform_device *pdev);
static int mtk_pcm_I2S0dl1_close(struct snd_pcm_substream *substream);
static int mtk_asoc_pcm_I2S0dl1_new(struct snd_soc_pcm_runtime *rtd);
static int mtk_afe_I2S0dl1_probe(struct snd_soc_platform *platform);

static int mI2S0dl1_hdoutput_control;
static int m2note_i2s0dl1_force_no_hd = 1;
static int m2note_i2s0dl1_force_i2s16 = 1;
static int m2note_i2s0dl1_force_mem16;
static bool mPrepareDone;
static bool mStartDeferred;

static const void *irq_user_id;
static uint32 irq1_cnt;

static struct device *mDev;

static unsigned int m2note_audio_copy_trace_count;
static unsigned int m2note_audio_pointer_trace_count;

#define M2NOTE_DL1_COPY_TRACE_LIMIT 16
#define M2NOTE_DL1_POINTER_TRACE_LIMIT 12

static void m2note_audio_dl1_trace(const char *stage,
				   struct snd_pcm_substream *substream,
				   unsigned int request_bytes,
				   int copy_size)
{
	struct snd_pcm_runtime *runtime = substream ? substream->runtime : NULL;
	AFE_BLOCK_T *pblock = NULL;

	if (pI2S0dl1MemControl)
		pblock = &pI2S0dl1MemControl->rBlock;

	if (runtime && runtime->status && runtime->control) {
		pr_warn("M2NOTE_AUDIO_DL1_RUNTIME_TRACE stage=%s state=%d access=%u appl=%lu hw=%lu avail=%lu hw_avail=%ld avail_min=%lu start=%lu stop=%lu boundary=%lu delay=%ld deferred=%d prepared=%d sram=%u req=%u copy=%d\n",
			stage, runtime->status->state, runtime->access,
			(unsigned long)runtime->control->appl_ptr,
			(unsigned long)runtime->status->hw_ptr,
			(unsigned long)snd_pcm_playback_avail(runtime),
			(long)snd_pcm_playback_hw_avail(runtime),
			(unsigned long)runtime->control->avail_min,
			(unsigned long)runtime->start_threshold,
			(unsigned long)runtime->stop_threshold,
			(unsigned long)runtime->boundary,
			(long)runtime->delay, mStartDeferred, mPrepareDone,
			mPlaybackSramState, request_bytes, copy_size);
	} else {
		pr_warn("M2NOTE_AUDIO_DL1_RUNTIME_TRACE stage=%s runtime=%p status=%p control=%p deferred=%d prepared=%d sram=%u req=%u copy=%d\n",
			stage, runtime, runtime ? runtime->status : NULL,
			runtime ? runtime->control : NULL, mStartDeferred,
			mPrepareDone, mPlaybackSramState, request_bytes,
			copy_size);
	}

	pr_warn("M2NOTE_AUDIO_DL1_TRACE stage=%s rate=%u fmt=%u ch=%u "
		"period=%lu buffer=%lu dma_bytes=%zu sram=%u prepared=%d "
		"irq_cnt=%u irq_user=%p req=%u copy=%d "
		"block_phys=0x%x block_virt=%p block_size=%u "
		"write=%u read=%u remained=%u memif=%d "
		"afe_base=0x%x afe_cur=0x%x afe_end=0x%x "
		"irq_con=0x%x irq_cnt1=0x%x irq_en=0x%x "
		"top1=0x%x dac0=0x%x dac1=0x%x i2s1=0x%x i2s3=0x%x\n",
		stage,
		runtime ? runtime->rate : 0,
		runtime ? runtime->format : 0,
		runtime ? runtime->channels : 0,
		runtime ? (unsigned long)runtime->period_size : 0,
		runtime ? (unsigned long)runtime->buffer_size : 0,
		runtime ? runtime->dma_bytes : 0,
		mPlaybackSramState, mPrepareDone, irq1_cnt, irq_user_id,
		request_bytes, copy_size,
		pblock ? pblock->pucPhysBufAddr : 0,
		pblock ? pblock->pucVirtBufAddr : NULL,
		pblock ? pblock->u4BufferSize : 0,
		pblock ? pblock->u4WriteIdx : 0,
		pblock ? pblock->u4DMAReadIdx : 0,
		pblock ? pblock->u4DataRemained : 0,
		GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1),
		Afe_Get_Reg(AFE_DL1_BASE), Afe_Get_Reg(AFE_DL1_CUR),
		Afe_Get_Reg(AFE_DL1_END), Afe_Get_Reg(AFE_IRQ_MCU_CON),
		Afe_Get_Reg(AFE_IRQ_MCU_CNT1), Afe_Get_Reg(AFE_IRQ_MCU_EN),
		Afe_Get_Reg(AUDIO_TOP_CON1), Afe_Get_Reg(AFE_DAC_CON0),
		Afe_Get_Reg(AFE_DAC_CON1), Afe_Get_Reg(AFE_I2S_CON1),
		Afe_Get_Reg(AFE_I2S_CON3));
}

static const char const *I2S0dl1_HD_output[] = {"Off", "On"};

static const struct soc_enum Audio_I2S0dl1_Enum[] = {
	SOC_ENUM_SINGLE_EXT(ARRAY_SIZE(I2S0dl1_HD_output), I2S0dl1_HD_output),
};


static int Audio_I2S0dl1_hdoutput_Get(struct snd_kcontrol *kcontrol,
				      struct snd_ctl_elem_value *ucontrol)
{
	pr_warn("Audio_AmpR_Get = %d\n", mI2S0dl1_hdoutput_control);
	ucontrol->value.integer.value[0] = mI2S0dl1_hdoutput_control;
	return 0;
}

#if defined(CONFIG_SND_SOC_FLORIDA)
static int Audio_I2S0dl1_hdoutput_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    static unsigned int on_count=0;

    printk("%s()\n", __func__);
    if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(I2S0dl1_HD_output))
    {
        printk("return -EINVAL\n");
        return -EINVAL;
    }
    printk("control=%d,value=%ld\n",mI2S0dl1_hdoutput_control, ucontrol->value.integer.value[0]);
    
    if (ucontrol->value.integer.value[0])
    {
        if(0 == on_count)
        {
            // set APLL clock setting
            AudDrv_Clk_On();
            EnableApll1(true);
            EnableApll2(true);
            EnableI2SDivPower(AUDIO_APLL1_DIV0, true);
            EnableI2SDivPower(AUDIO_APLL2_DIV0, true);
            EnableI2SDivPower(AUDIO_APLL12_DIV2, true);
            EnableI2SDivPower(AUDIO_APLL12_DIV3, true);  //I2S2 APLL
            AudDrv_APLL1Tuner_Clk_On();
            AudDrv_APLL2Tuner_Clk_On();
    	    mI2S0dl1_hdoutput_control = true;
        }
		on_count++;
    }
    else
    {
        on_count--;
        if(0 == on_count)
        {
            mI2S0dl1_hdoutput_control = false;
            // set APLL clock setting
            EnableApll1(false);
            EnableApll2(false);
            EnableI2SDivPower(AUDIO_APLL1_DIV0, false);
            EnableI2SDivPower(AUDIO_APLL2_DIV0, false);
            EnableI2SDivPower(AUDIO_APLL12_DIV2, false);
            EnableI2SDivPower(AUDIO_APLL12_DIV3, false);  //I2S2 APLL
            AudDrv_APLL1Tuner_Clk_Off();
            AudDrv_APLL2Tuner_Clk_Off();
            AudDrv_Clk_Off();
        }
    }
    return 0;
}

#else
static int Audio_I2S0dl1_hdoutput_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
    printk("%s() control=%d value=%ld\n", __func__,
	   mI2S0dl1_hdoutput_control, ucontrol->value.integer.value[0]);
    if (ucontrol->value.enumerated.item[0] > ARRAY_SIZE(I2S0dl1_HD_output))
    {
        printk("return -EINVAL\n");
        return -EINVAL;
    }

    mI2S0dl1_hdoutput_control = ucontrol->value.integer.value[0];

    if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_HDMI) == true)
    {
        printk("return HDMI enabled\n");
        return 0;
    }

    return 0;
}
#endif

static int Audio_Irqcnt1_Get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	AudDrv_Clk_On();
	ucontrol->value.integer.value[0] = Afe_Get_Reg(AFE_IRQ_MCU_CNT1);
	AudDrv_Clk_Off();
	return 0;
}

static int Audio_Irqcnt1_Set(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	irq1_cnt = ucontrol->value.integer.value[0];

	pr_warn("%s()\n", __func__);
	AudDrv_Clk_On();
	if (irq_user_id && irq1_cnt)
		irq_update_user(irq_user_id,
				Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE,
				0,
				irq1_cnt);
	else {
		pr_warn("store irq counter for next start, user_id = %p, irq1_cnt = %d\n",
			irq_user_id, irq1_cnt);
		m2note_audio_dl1_trace("irqcnt1_store", NULL, 0, 0);
	}

	m2note_audio_dl1_trace("irqcnt1_set", NULL, 0, 0);
	AudDrv_Clk_Off();
	return 0;
}

static int Audio_m2note_i2s0dl1_force_no_hd_Get(struct snd_kcontrol *kcontrol,
						struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = m2note_i2s0dl1_force_no_hd;
	return 0;
}

static int Audio_m2note_i2s0dl1_force_no_hd_Set(struct snd_kcontrol *kcontrol,
						struct snd_ctl_elem_value *ucontrol)
{
	m2note_i2s0dl1_force_no_hd = !!ucontrol->value.integer.value[0];
	pr_warn("M2NOTE_AUDIO_I2S0DL1_SAFE_CTL force_no_hd=%d force_i2s16=%d force_mem16=%d requested_hd=%d\n",
		m2note_i2s0dl1_force_no_hd, m2note_i2s0dl1_force_i2s16,
		m2note_i2s0dl1_force_mem16, mI2S0dl1_hdoutput_control);
	return 0;
}

static int Audio_m2note_i2s0dl1_force_i2s16_Get(struct snd_kcontrol *kcontrol,
						struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = m2note_i2s0dl1_force_i2s16;
	return 0;
}

static int Audio_m2note_i2s0dl1_force_i2s16_Set(struct snd_kcontrol *kcontrol,
						struct snd_ctl_elem_value *ucontrol)
{
	m2note_i2s0dl1_force_i2s16 = !!ucontrol->value.integer.value[0];
	pr_warn("M2NOTE_AUDIO_I2S0DL1_SAFE_CTL force_no_hd=%d force_i2s16=%d force_mem16=%d requested_hd=%d\n",
		m2note_i2s0dl1_force_no_hd, m2note_i2s0dl1_force_i2s16,
		m2note_i2s0dl1_force_mem16, mI2S0dl1_hdoutput_control);
	return 0;
}

static int Audio_m2note_i2s0dl1_force_mem16_Get(struct snd_kcontrol *kcontrol,
					       struct snd_ctl_elem_value *ucontrol)
{
	ucontrol->value.integer.value[0] = m2note_i2s0dl1_force_mem16;
	return 0;
}

static int Audio_m2note_i2s0dl1_force_mem16_Set(struct snd_kcontrol *kcontrol,
					       struct snd_ctl_elem_value *ucontrol)
{
	m2note_i2s0dl1_force_mem16 = !!ucontrol->value.integer.value[0];
	pr_warn("M2NOTE_AUDIO_I2S0DL1_SAFE_CTL force_no_hd=%d force_i2s16=%d force_mem16=%d requested_hd=%d\n",
		m2note_i2s0dl1_force_no_hd, m2note_i2s0dl1_force_i2s16,
		m2note_i2s0dl1_force_mem16, mI2S0dl1_hdoutput_control);
	return 0;
}

static const struct snd_kcontrol_new Audio_snd_I2S0dl1_controls[] = {
	SOC_ENUM_EXT("Audio_I2S0dl1_hd_Switch", Audio_I2S0dl1_Enum[0],
		Audio_I2S0dl1_hdoutput_Get, Audio_I2S0dl1_hdoutput_Set),
	SOC_SINGLE_EXT("Audio_m2note_i2s0dl1_force_no_hd", SND_SOC_NOPM, 0,
		       1, 0, Audio_m2note_i2s0dl1_force_no_hd_Get,
		       Audio_m2note_i2s0dl1_force_no_hd_Set),
	SOC_SINGLE_EXT("Audio_m2note_i2s0dl1_force_i2s16", SND_SOC_NOPM, 0,
		       1, 0, Audio_m2note_i2s0dl1_force_i2s16_Get,
		       Audio_m2note_i2s0dl1_force_i2s16_Set),
	SOC_SINGLE_EXT("Audio_m2note_i2s0dl1_force_mem16", SND_SOC_NOPM, 0,
		       1, 0, Audio_m2note_i2s0dl1_force_mem16_Get,
		       Audio_m2note_i2s0dl1_force_mem16_Set),
	SOC_SINGLE_EXT("Audio IRQ1 CNT", SND_SOC_NOPM, 0, IRQ_MAX_RATE, 0,
			       Audio_Irqcnt1_Get, Audio_Irqcnt1_Set),
};

static struct snd_pcm_hardware mtk_I2S0dl1_hardware = {
	.info = (SNDRV_PCM_INFO_INTERLEAVED |
	SNDRV_PCM_INFO_RESUME),
	/*
	 * The legacy MTK HAL opens DL1 as S32/8_24 even when policy exposes
	 * PCM16. Keep the advertised MTK mask wide and clamp m2note I2S/DAC
	 * width in prepare() via the force_i2s16/force_no_hd guards.
	 */
	.formats =   SND_SOC_ADV_MT_FMTS,
	.rates =        SOC_HIGH_USE_RATE,
	.rate_min =     SOC_HIGH_USE_RATE_MIN,
	.rate_max =     SOC_HIGH_USE_RATE_MAX,
	.channels_min =     SOC_NORMAL_USE_CHANNELS_MIN,
	.channels_max =     SOC_NORMAL_USE_CHANNELS_MAX,
	.buffer_bytes_max = SOC_NORMAL_USE_BUFFERSIZE_MAX,
	.period_bytes_max = SOC_NORMAL_USE_BUFFERSIZE_MAX,
	.periods_min =      SOC_NORMAL_USE_PERIODS_MIN,
	.periods_max =     SOC_NORMAL_USE_PERIODS_MAX,
	.fifo_size =        0,
};

static unsigned int mtk_pcm_I2S0dl1_prefill_threshold_bytes(
	struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream ? substream->runtime : NULL;

	if (!runtime || !runtime->period_size)
		return 0;

	return audio_frame_to_bytes(substream, runtime->period_size);
}

static bool mtk_pcm_I2S0dl1_has_start_prefill(
	struct snd_pcm_substream *substream)
{
	AFE_BLOCK_T *Afe_Block;
	unsigned int threshold;

	if (!pI2S0dl1MemControl)
		return false;

	Afe_Block = &pI2S0dl1MemControl->rBlock;
	threshold = mtk_pcm_I2S0dl1_prefill_threshold_bytes(substream);
	if (!threshold)
		threshold = 64;

	return Afe_Block->u4DataRemained >= threshold;
}

static int mtk_pcm_I2S0dl1_enable_path(struct snd_pcm_substream *substream,
				       const char *stage)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int ret;

	SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I05,
		      Soc_Aud_InterConnectionOutput_O00);
	SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I06,
		      Soc_Aud_InterConnectionOutput_O01);
	SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I05,
		      Soc_Aud_InterConnectionOutput_O03);
	SetConnection(Soc_Aud_InterCon_Connection, Soc_Aud_InterConnectionInput_I06,
		      Soc_Aud_InterConnectionOutput_O04);

	if (!irq_user_id) {
		ret = irq_add_user(substream,
				   Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE,
				   runtime->rate,
				   irq1_cnt ? irq1_cnt : runtime->period_size);
		if (ret) {
			pr_err("%s irq_add_user failed ret=%d\n", __func__, ret);
			m2note_audio_dl1_trace("enable_path_irq_failed",
					       substream, ret, 0);
			return ret;
		}
		irq_user_id = substream;
	} else if (irq_user_id != substream) {
		pr_err("%s unexpected irq owner current=%p new=%p\n",
		       __func__, irq_user_id, substream);
		m2note_audio_dl1_trace("enable_path_irq_busy", substream, 0, 0);
		return -EBUSY;
	}

	SetSampleRate(Soc_Aud_Digital_Block_MEM_DL1, runtime->rate);
	SetChannels(Soc_Aud_Digital_Block_MEM_DL1, runtime->channels);
	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, true);

	EnableAfe(true);
	m2note_audio_dl1_trace(stage, substream, 0, 0);
	return 0;
}

static int mtk_pcm_I2S0dl1_stop(struct snd_pcm_substream *substream)
{
	/* AFE_BLOCK_T *Afe_Block = &(pI2S0dl1MemControl->rBlock); */

    printk("%s \n", __func__);
	m2note_audio_dl1_trace("stop_entry", substream, 0, 0);

	mStartDeferred = false;
	if (irq_user_id == substream) {
		irq_remove_user(irq_user_id, Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE);
		irq_user_id = NULL;
	} else if (irq_user_id) {
		m2note_audio_dl1_trace("stop_irq_owner_mismatch", substream, 0, 0);
	} else {
		m2note_audio_dl1_trace("stop_no_irq_user", substream, 0, 0);
	}

	SetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1, false);

    // here start digital part
    SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I05, Soc_Aud_InterConnectionOutput_O00);
    SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I06, Soc_Aud_InterConnectionOutput_O01);
    SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I05, Soc_Aud_InterConnectionOutput_O03);
    SetConnection(Soc_Aud_InterCon_DisConnect, Soc_Aud_InterConnectionInput_I06, Soc_Aud_InterConnectionOutput_O04);

	ClearMemBlock(Soc_Aud_Digital_Block_MEM_DL1);
	m2note_audio_dl1_trace("stop_done", substream, 0, 0);

	return 0;
}

static snd_pcm_uframes_t mtk_pcm_I2S0dl1_pointer(struct snd_pcm_substream
						 *substream)
{
	kal_int32 HW_memory_index = 0;
	kal_int32 HW_Cur_ReadIdx = 0;
	kal_uint32 Frameidx = 0;
	kal_int32 Afe_consumed_bytes = 0;
	AFE_BLOCK_T *Afe_Block = &pI2S0dl1MemControl->rBlock;
	unsigned long flags;
	/* struct snd_pcm_runtime *runtime = substream->runtime; */

	spin_lock_irqsave(&auddrv_I2S0dl1_lock, flags);
	PRINTK_AUD_DL1(" %s Afe_Block->u4DMAReadIdx = 0x%x\n", __func__,
		       Afe_Block->u4DMAReadIdx);

	/* get total bytes to copy */
	/* Frameidx = audio_bytes_to_frame(substream , Afe_Block->u4DMAReadIdx); */
	/* return Frameidx; */

	if (GetMemoryPathEnable(Soc_Aud_Digital_Block_MEM_DL1) == true) {
		HW_Cur_ReadIdx = Afe_Get_Reg(AFE_DL1_CUR);
		if (HW_Cur_ReadIdx == 0) {
			PRINTK_AUDDRV("[Auddrv] HW_Cur_ReadIdx ==0\n");
			HW_Cur_ReadIdx = Afe_Block->pucPhysBufAddr;
		}

		HW_memory_index = (HW_Cur_ReadIdx - Afe_Block->pucPhysBufAddr);
		if (HW_memory_index >=  Afe_Block->u4DMAReadIdx)
			Afe_consumed_bytes = HW_memory_index - Afe_Block->u4DMAReadIdx;
		else {
			Afe_consumed_bytes = Afe_Block->u4BufferSize + HW_memory_index -
					     Afe_Block->u4DMAReadIdx;
		}

		Afe_consumed_bytes = Align64ByteSize(Afe_consumed_bytes);
		if (Afe_consumed_bytes < 0 ||
		    Afe_consumed_bytes > Afe_Block->u4BufferSize) {
			pr_warn("M2NOTE_AUDIO_DL1_UNDERRUN_GUARD stage=bad_consumed consumed=%d hw_index=%d read=%u write=%u remained=%u buffer=%u\n",
				Afe_consumed_bytes, HW_memory_index,
				Afe_Block->u4DMAReadIdx, Afe_Block->u4WriteIdx,
				Afe_Block->u4DataRemained, Afe_Block->u4BufferSize);
			Afe_consumed_bytes = 0;
		}

		if ((kal_uint32)Afe_consumed_bytes > Afe_Block->u4DataRemained) {
			pr_warn("M2NOTE_AUDIO_DL1_UNDERRUN_GUARD stage=clamp consumed=%d hw_index=%d read=%u write=%u remained=%u buffer=%u\n",
				Afe_consumed_bytes, HW_memory_index,
				Afe_Block->u4DMAReadIdx, Afe_Block->u4WriteIdx,
				Afe_Block->u4DataRemained, Afe_Block->u4BufferSize);
			Afe_Block->u4DataRemained = 0;
			Afe_Block->u4DMAReadIdx = Afe_Block->u4WriteIdx;
		} else {
			Afe_Block->u4DataRemained -= Afe_consumed_bytes;
			Afe_Block->u4DMAReadIdx += Afe_consumed_bytes;
			Afe_Block->u4DMAReadIdx %= Afe_Block->u4BufferSize;
		}
		PRINTK_AUD_DL1("[Auddrv] HW_Cur_ReadIdx =0x%x HW_memory_index = 0x%x Afe_consumed_bytes  = 0x%x\n",
			       HW_Cur_ReadIdx,
			       HW_memory_index, Afe_consumed_bytes);
		Frameidx = audio_bytes_to_frame(substream , Afe_Block->u4DMAReadIdx);
		spin_unlock_irqrestore(&auddrv_I2S0dl1_lock, flags);
		if (m2note_audio_pointer_trace_count < M2NOTE_DL1_POINTER_TRACE_LIMIT) {
			m2note_audio_pointer_trace_count++;
			m2note_audio_dl1_trace("pointer_memif_on", substream,
					       Afe_consumed_bytes, HW_memory_index);
		}

		return Frameidx;
	}

	Frameidx = audio_bytes_to_frame(substream , Afe_Block->u4DMAReadIdx);
	spin_unlock_irqrestore(&auddrv_I2S0dl1_lock, flags);
	if (m2note_audio_pointer_trace_count < M2NOTE_DL1_POINTER_TRACE_LIMIT) {
		m2note_audio_pointer_trace_count++;
		m2note_audio_dl1_trace("pointer_memif_off", substream, 0, 0);
	}
	return Frameidx;

}


static void SetDL1Buffer(struct snd_pcm_substream *substream,
			 struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	AFE_BLOCK_T *pblock = &pI2S0dl1MemControl->rBlock;

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
	m2note_audio_dl1_trace("set_buffer", substream, pblock->u4BufferSize, 0);

}

static int mtk_pcm_I2S0dl1_hw_params(struct snd_pcm_substream *substream,
				     struct snd_pcm_hw_params *hw_params)
{
	int ret = 0;

	substream->runtime->dma_bytes = params_buffer_bytes(hw_params);
	/* pr_warn("mtk_pcm_hw_params dma_bytes = %d\n",substream->runtime->dma_bytes); */
#if 1
	if (mPlaybackSramState == SRAM_STATE_PLAYBACKFULL) {
		/* substream->runtime->dma_bytes = AFE_INTERNAL_SRAM_SIZE; */
		substream->runtime->dma_area = (unsigned char *)Get_Afe_SramBase_Pointer();
		substream->runtime->dma_addr = AFE_INTERNAL_SRAM_PHY_BASE;
		AudDrv_Allocate_DL1_Buffer(mDev, substream->runtime->dma_bytes);
	} else {
		substream->runtime->dma_bytes = params_buffer_bytes(hw_params);
		substream->runtime->dma_area = Dl1_Playback_dma_buf->area;
		substream->runtime->dma_addr = Dl1_Playback_dma_buf->addr;
		SetDL1Buffer(substream, hw_params);
	}
#else /* old */
	/* here to allcoate sram to hardware --------------------------- */
	AudDrv_Allocate_mem_Buffer(Soc_Aud_Digital_Block_MEM_DL1,
				   substream->runtime->dma_bytes);
#ifdef AUDIO_MEMORY_SRAM
	/* substream->runtime->dma_bytes = AFE_INTERNAL_SRAM_SIZE; */
	substream->runtime->dma_area = (unsigned char *)Get_Afe_SramBase_Pointer();
	substream->runtime->dma_addr = AFE_INTERNAL_SRAM_PHY_BASE;
#else
	pI2S0dl1MemControl = Get_Mem_ControlT(Soc_Aud_Digital_Block_MEM_DL1);
	Afe_Block = &pI2S0dl1MemControl->rBlock;

	substream->runtime->dma_area = (unsigned char *)Afe_Block->pucVirtBufAddr;
	substream->runtime->dma_addr = Afe_Block->pucPhysBufAddr;
#endif
	/* ------------------------------------------------------- */
#endif
	m2note_audio_dl1_trace("hw_params", substream,
			       params_buffer_bytes(hw_params), ret);
	/*pr_warn("1 dma_bytes = %zu dma_area = %p dma_addr = 0x%lx\n",
	       substream->runtime->dma_bytes, substream->runtime->dma_area,
	       (long)substream->runtime->dma_addr);*/

	return ret;
}

static int mtk_pcm_I2S0dl1_hw_free(struct snd_pcm_substream *substream)
{
	return 0;
}

static struct snd_pcm_hw_constraint_list constraints_sample_rates = {
	.count = ARRAY_SIZE(soc_high_supported_sample_rates),
	.list = soc_high_supported_sample_rates,
	.mask = 0,
};

static int mtk_pcm_I2S0dl1_open(struct snd_pcm_substream *substream)
{
	int ret = 0;
	struct snd_pcm_runtime *runtime = substream->runtime;

	AfeControlSramLock();
	if (GetSramState() == SRAM_STATE_FREE) {
		mtk_I2S0dl1_hardware.buffer_bytes_max = GetPLaybackSramFullSize();
		mPlaybackSramState = SRAM_STATE_PLAYBACKFULL;
		SetSramState(mPlaybackSramState);
	} else {
		mtk_I2S0dl1_hardware.buffer_bytes_max = GetPLaybackDramSize();
		mPlaybackSramState = SRAM_STATE_PLAYBACKDRAM;
	}
	AfeControlSramUnLock();
	if (mPlaybackSramState == SRAM_STATE_PLAYBACKDRAM)
		AudDrv_Emi_Clk_On();

	/*pr_warn("mtk_I2S0dl1_hardware.buffer_bytes_max = %zu mPlaybackSramState = %d\n",
	       mtk_I2S0dl1_hardware.buffer_bytes_max,
	       mPlaybackSramState);*/
	runtime->hw = mtk_I2S0dl1_hardware;

	AudDrv_Clk_On();
	memcpy((void *)(&(runtime->hw)), (void *)&mtk_I2S0dl1_hardware ,
	       sizeof(struct snd_pcm_hardware));
	pI2S0dl1MemControl = Get_Mem_ControlT(Soc_Aud_Digital_Block_MEM_DL1);
	m2note_audio_copy_trace_count = 0;
	m2note_audio_pointer_trace_count = 0;
	mStartDeferred = false;
	m2note_audio_dl1_trace("open", substream, 0, 0);

	ret = snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_RATE,
					 &constraints_sample_rates);

	if (ret < 0)
		pr_warn("snd_pcm_hw_constraint_integer failed\n");
/*
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		pr_warn("SNDRV_PCM_STREAM_PLAYBACK mtkalsa_I2S0dl1_playback_constraints\n");
	else
		pr_warn("SNDRV_PCM_STREAM_CAPTURE mtkalsa_I2S0dl1_playback_constraints\n");
*/
	if (ret < 0) {
		pr_err("ret < 0 mtk_pcm_I2S0dl1_close\n");
		mtk_pcm_I2S0dl1_close(substream);
		return ret;
	}

	/* pr_warn("mtk_pcm_I2S0dl1_open return\n"); */
	return 0;
}

static int mtk_pcm_I2S0dl1_close(struct snd_pcm_substream *substream)
{
	/*pr_warn("%s\n", __func__);*/
	m2note_audio_dl1_trace("close_entry", substream, 0, 0);

	if (mPrepareDone == true) {
		/* stop DAC output */
#if defined(CONFIG_SND_SOC_FLORIDA)
		SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, false);
		if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC) == false)
		{
		   SetI2SAdcEnable(false);
		   SetI2SDacEnable(false);
		}
#else
        SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, false);
		if (GetI2SDacEnable() == false)
        {
			SetI2SDacEnable(false);
        }
#endif
		/* stop I2S output */
		SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_2, false);

		if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_2) == false)
			Afe_Set_Reg(AFE_I2S_CON3, 0x0, 0x1);

		RemoveMemifSubStream(Soc_Aud_Digital_Block_MEM_DL1, substream);

		EnableAfe(false);

		if (mI2S0dl1_hdoutput_control == true) {
			pr_warn("%s mI2S0dl1_hdoutput_control == %d\n",
				__func__, mI2S0dl1_hdoutput_control);
			DisableALLbySampleRate(substream->runtime->rate);
			EnableI2SDivPower(AUDIO_APLL12_DIV2, false);
		}

		mPrepareDone = false;
	}

	mStartDeferred = false;
	if (irq_user_id == substream) {
		irq_remove_user(irq_user_id, Soc_Aud_IRQ_MCU_MODE_IRQ1_MCU_MODE);
		irq_user_id = NULL;
	}

	if (mPlaybackSramState == SRAM_STATE_PLAYBACKDRAM)
		AudDrv_Emi_Clk_Off();
	AfeControlSramLock();
	ClearSramState(mPlaybackSramState);
	mPlaybackSramState = GetSramState();
	AfeControlSramUnLock();
	m2note_audio_dl1_trace("close_done", substream, 0, 0);
	AudDrv_Clk_Off();
	return 0;
}

static int mtk_pcm_I2S0dl1_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	uint32 MclkDiv3;
	uint32 u32AudioI2S = 0;
	bool mI2SWLen;
	bool requested_32bit;
	bool mem_32bit;
	bool dac_32bit;
	bool requested_hd;
	bool effective_hd;

	if (mPrepareDone == false)
	{
	    printk("%s format = %d samplerate = %d SNDRV_PCM_FORMAT_S32_LE = %d SNDRV_PCM_FORMAT_U32_LE = %d \n", __func__, runtime->format, runtime->rate, SNDRV_PCM_FORMAT_S32_LE, SNDRV_PCM_FORMAT_U32_LE);
			m2note_audio_dl1_trace("prepare_entry", substream, 0, 0);
			SetMemifSubStream(Soc_Aud_Digital_Block_MEM_DL1, substream);

		requested_32bit = (runtime->format == SNDRV_PCM_FORMAT_S32_LE ||
				   runtime->format == SNDRV_PCM_FORMAT_U32_LE);
		mem_32bit = requested_32bit && !m2note_i2s0dl1_force_mem16;
		dac_32bit = requested_32bit && !m2note_i2s0dl1_force_i2s16;
		requested_hd = mI2S0dl1_hdoutput_control;
		effective_hd = requested_hd && !m2note_i2s0dl1_force_no_hd;
		mI2SWLen = dac_32bit ? Soc_Aud_I2S_WLEN_WLEN_32BITS :
					Soc_Aud_I2S_WLEN_WLEN_16BITS;

	        if (mem_32bit)
	        {
	            SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL1, AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
	            SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL2, AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA);
	            SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_VUL, AFE_WLEN_32_BIT_ALIGN_8BIT_0_24BIT_DATA); ////set UL bitwidth

	            SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_24BIT, Soc_Aud_InterConnectionOutput_O03);
	            SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_24BIT, Soc_Aud_InterConnectionOutput_O04);
	            SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_24BIT, Soc_Aud_InterConnectionOutput_O00);
	            SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_24BIT, Soc_Aud_InterConnectionOutput_O01);
	        }
	        else
	        {
	            SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL1, AFE_WLEN_16_BIT);
            SetMemIfFetchFormatPerSample(Soc_Aud_Digital_Block_MEM_DL2, AFE_WLEN_16_BIT);
	            SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT, Soc_Aud_InterConnectionOutput_O03);
	            SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT, Soc_Aud_InterConnectionOutput_O04);
	            SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT, Soc_Aud_InterConnectionOutput_O00);
	            SetoutputConnectionFormat(OUTPUT_DATA_FORMAT_16BIT, Soc_Aud_InterConnectionOutput_O01);
	        }

	        SetSampleRate(Soc_Aud_Digital_Block_MEM_I2S,  runtime->rate);

	        // I2S out Setting
	        u32AudioI2S = SampleRateTransform(runtime->rate) << 8;
	        u32AudioI2S |= Soc_Aud_I2S_FORMAT_I2S << 3; // us3 I2s format
	        u32AudioI2S |= mI2SWLen << 1;

	        if (effective_hd == true) {
	            pr_warn("%s mI2S0dl1_hdoutput_control == %d effective_hd=%d\n",
	                __func__, mI2S0dl1_hdoutput_control, effective_hd);
	            EnableALLbySampleRate(runtime->rate);
	            MclkDiv3 = SetCLkMclk(Soc_Aud_I2S1, runtime->rate);
	            MclkDiv3 = SetCLkMclk(Soc_Aud_I2S3, runtime->rate);
	            SetCLkBclk(MclkDiv3, runtime->rate, runtime->channels,
	                   mI2SWLen);
	            EnableI2SDivPower(AUDIO_APLL12_DIV2, true);
	            u32AudioI2S |= Soc_Aud_LOW_JITTER_CLOCK << 12;
	        } else {
	            u32AudioI2S &= ~(Soc_Aud_LOW_JITTER_CLOCK << 12);
	        }

		pr_warn("M2NOTE_AUDIO_I2S0DL1_SAFE_TRACE force_no_hd=%d force_i2s16=%d force_mem16=%d requested_hd=%d effective_hd=%d runtime_fmt=%u requested_32=%d mem_32=%d dac_32=%d i2s_wlen=%u rate=%u ch=%u i2s3=0x%x\n",
			m2note_i2s0dl1_force_no_hd, m2note_i2s0dl1_force_i2s16,
			m2note_i2s0dl1_force_mem16, requested_hd, effective_hd,
			runtime->format, requested_32bit, mem_32bit, dac_32bit,
			mI2SWLen, runtime->rate, runtime->channels, u32AudioI2S);

	        if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_2) == false)
	        {
	            SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_2, true);
            //Afe_Set_Reg(AFE_I2S_CON, Audio_I2S_Dac | 0x1, MASK_ALL); //6752 TODO: fix fm playback then mp3, i2s_con is misconfigured...
            Afe_Set_Reg(AFE_I2S_CON3, u32AudioI2S | 1, AFE_MASK_ALL);
        }
        else
        {
            SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_2, true);
        }

        // start I2S DAC out
#if defined(CONFIG_SND_SOC_FLORIDA)
		if((GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC) == false))
		{
			Afe_Set_Reg(AUDIO_TOP_CON1, 0x4,  0x4);  // I2S1/I2S2 SOFT_Reset Hi
			Afe_Set_Reg(AUDIO_TOP_CON1, (0x1 << 5)|(0x1 << 6),	(0x1 << 5)|(0x1 << 6)); // Sets to 1 to gated I2S1/2 engine clock
			ConfigAdcI2S(substream);
			SetI2SAdcIn(mAudioDigitalI2S);
			{
				// I2S1 out Setting
				uint32 u32AudioI2S = 0, MclkDiv1 = 0, MclkDiv2 = 0;
	
				u32AudioI2S = SampleRateTransform(substream->runtime->rate) << 8;
				u32AudioI2S |= Soc_Aud_I2S_FORMAT_I2S << 3; // us3 I2s format
				u32AudioI2S |= Soc_Aud_I2S_WLEN_WLEN_32BITS << 1; //32bit  
				u32AudioI2S |= Soc_Aud_LOW_JITTER_CLOCK << 12 ; //Low jitter mode
	
				Afe_Set_Reg(AFE_I2S_CON1, u32AudioI2S, 0xFFFFFFFE);

				MclkDiv1 = SetCLkMclk(Soc_Aud_I2S1, substream->runtime->rate); //select I2S
				SetCLkBclk(MclkDiv1,  substream->runtime->rate, substream->runtime->channels, Soc_Aud_I2S_WLEN_WLEN_32BITS);  
	
				MclkDiv2 = SetCLkMclk(Soc_Aud_I2S2, substream->runtime->rate); //select I2S
				SetCLkBclk(MclkDiv2,  substream->runtime->rate, 2, Soc_Aud_I2S_WLEN_WLEN_32BITS);
			}

			Afe_Set_Reg(AUDIO_TOP_CON1, (0x0 << 5)|(0x0 << 6),	(0x1 << 5)|(0x1 << 6));
			udelay(200);
			Afe_Set_Reg(AUDIO_TOP_CON1, 0x0,  0x4);  // // I2S1/I2S2 SOFT_Reset Lo
	
			SetI2SAdcEnable(true);
			SetI2SDacEnable(true);
		}
		
		SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, true);
#else
	        if (GetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC) == false)
	        {
	            SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, true);
	            SetI2SDacOut(substream->runtime->rate, effective_hd, mI2SWLen);
	            SetI2SDacEnable(true);
	        }
        else
        {
            SetMemoryPathEnable(Soc_Aud_Digital_Block_I2S_OUT_DAC, true);
        }
#endif
        EnableAfe(true);
        mPrepareDone = true;
		m2note_audio_dl1_trace("prepare_done", substream, 0, 0);
    }
	else
		m2note_audio_dl1_trace("prepare_skip_done", substream, 0, 0);
    return 0;
}

static int mtk_pcm_I2S0dl1_start(struct snd_pcm_substream *substream)
{
	int ret;

	printk("%s\n", __func__);
	m2note_audio_dl1_trace("start_entry", substream, 0, 0);

	if (!mtk_pcm_I2S0dl1_has_start_prefill(substream)) {
		mStartDeferred = true;
		m2note_audio_dl1_trace("start_deferred_no_prefill",
				       substream, 0, 0);
		return 0;
	}

	mStartDeferred = false;
	ret = mtk_pcm_I2S0dl1_enable_path(substream, "start_done");
	if (ret)
		return ret;

#ifdef _DEBUG_6328_CLK
	/* Debug 6328 Digital to Analog path clock and data work or not. and read TEST_OUT(0x206) */
	/* Ana_Set_Reg(AFE_MON_DEBUG0, 0x4600 , 0xcf00); // monitor 6328 digitala data sent to analog or not */
	Ana_Set_Reg(AFE_MON_DEBUG0, 0x4200 ,
		    0xcf00); /* monitor 6328 digitala data sent to analog or not */
	Ana_Set_Reg(TEST_CON0, 0x0e00 , 0xffff);
#endif

#if 0 /* test 6328 sgen */
	Ana_Set_Reg(AFE_SGEN_CFG0 , 0x0080 , 0xffff);
	Ana_Set_Reg(AFE_SGEN_CFG1 , 0x0101 , 0xffff);
	Ana_Set_Reg(AFE_AUDIO_TOP_CON0, 0x0000, 0xffff);   /* power on clock */
	/* Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0002, 0x0002);   //UL from sinetable */
	Ana_Set_Reg(PMIC_AFE_TOP_CON0, 0x0001, 0x0001);   /* DL from sinetable */
#endif
	return 0;
}

static int mtk_pcm_I2S0dl1_trigger(struct snd_pcm_substream *substream, int cmd)
{
	/* pr_warn("mtk_pcm_I2S0dl1_trigger cmd = %d\n", cmd); */
	m2note_audio_dl1_trace("trigger", substream, cmd, 0);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		return mtk_pcm_I2S0dl1_start(substream);
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		return mtk_pcm_I2S0dl1_stop(substream);
	}
	return -EINVAL;
}

static int mtk_pcm_I2S0dl1_copy(struct snd_pcm_substream *substream,
				int channel, snd_pcm_uframes_t pos,
				void __user *dst, snd_pcm_uframes_t count)
{
	AFE_BLOCK_T  *Afe_Block = NULL;
	int copy_size = 0, Afe_WriteIdx_tmp;
	unsigned long flags;
	/* struct snd_pcm_runtime *runtime = substream->runtime; */
	char *data_w_ptr = (char *)dst;
	unsigned int request_bytes;
	int ret;
	bool trace_copy;

	PRINTK_AUD_DL1("mtk_pcm_copy pos = %lu count = %lu\n ", pos, count);

#ifdef _DEBUG_6328_CLK
	/* pr_warn("TEST_OUT = 0x%x\n", Ana_Get_Reg(TEST_OUT)); */
	/* pr_warn("AFUNC_AUD_MON0 = 0x%x\n", Ana_Get_Reg(AFUNC_AUD_MON0)); */
	/* pr_warn("AFUNC_AUD_MON1 = 0x%x\n", Ana_Get_Reg(AFUNC_AUD_MON1)); */

#endif

	/* get total bytes to copy */
	count = audio_frame_to_bytes(substream , count);
	request_bytes = count;

	/* check which memif nned to be write */
	Afe_Block = &pI2S0dl1MemControl->rBlock;

	/* handle for buffer management */

	PRINTK_AUD_DL1(" WriteIdx=0x%x, ReadIdx=0x%x, DataRemained=0x%x\n",
		       Afe_Block->u4WriteIdx, Afe_Block->u4DMAReadIdx, Afe_Block->u4DataRemained);

	if (Afe_Block->u4BufferSize == 0) {
		pr_err(" u4BufferSize=0 Error");
		return 0;
	}

	AudDrv_checkDLISRStatus();

	spin_lock_irqsave(&auddrv_I2S0dl1_lock, flags);
	if (Afe_Block->u4DataRemained > Afe_Block->u4BufferSize) {
		pr_warn("M2NOTE_AUDIO_DL1_UNDERRUN_GUARD stage=copy_remained_clamp read=%u write=%u remained=%u buffer=%u\n",
			Afe_Block->u4DMAReadIdx, Afe_Block->u4WriteIdx,
			Afe_Block->u4DataRemained, Afe_Block->u4BufferSize);
		Afe_Block->u4DataRemained = Afe_Block->u4BufferSize;
	}
	copy_size = Afe_Block->u4BufferSize -
		    Afe_Block->u4DataRemained;  /* free space of the buffer */
	spin_unlock_irqrestore(&auddrv_I2S0dl1_lock, flags);

	if (count <=  copy_size) {
		if (copy_size < 0)
			copy_size = 0;
		else
			copy_size = count;
	}

	copy_size = Align64ByteSize(copy_size);
	PRINTK_AUD_DL1("copy_size=0x%x, count=0x%x\n", copy_size, (unsigned int)count);
	trace_copy = m2note_audio_copy_trace_count < M2NOTE_DL1_COPY_TRACE_LIMIT;
	if (trace_copy)
		m2note_audio_dl1_trace("copy_before", substream,
				       request_bytes, copy_size);

	if (copy_size != 0) {
		spin_lock_irqsave(&auddrv_I2S0dl1_lock, flags);
		Afe_WriteIdx_tmp = Afe_Block->u4WriteIdx;
		spin_unlock_irqrestore(&auddrv_I2S0dl1_lock, flags);

		if (Afe_WriteIdx_tmp + copy_size < Afe_Block->u4BufferSize) { /* copy once */
			if (!access_ok(VERIFY_READ, data_w_ptr, copy_size)) {
				PRINTK_AUDDRV("0ptr invalid data_w_ptr=%p, size=%d", data_w_ptr, copy_size);
				PRINTK_AUDDRV(" u4BufferSize=%d, u4DataRemained=%d", Afe_Block->u4BufferSize,
					      Afe_Block->u4DataRemained);
			} else {

				PRINTK_AUD_DL1("memcpy Idx= %p data_w_ptr = %p copy_size = 0x%x\n",
					       Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp, data_w_ptr, copy_size);
				if (copy_from_user((Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp), data_w_ptr,
						   copy_size)) {
					PRINTK_AUDDRV(" Fail copy from user\n");
					return -1;
				}
			}

			spin_lock_irqsave(&auddrv_I2S0dl1_lock, flags);
			Afe_Block->u4DataRemained += copy_size;
			Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + copy_size;
			Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
			spin_unlock_irqrestore(&auddrv_I2S0dl1_lock, flags);
			data_w_ptr += copy_size;
			count -= copy_size;

			PRINTK_AUD_DL1(" finish1, copy_size:%x, WriteIdx:%x, ReadIdx=%x, Remained:%x, count=%x \r\n",
				       copy_size, Afe_Block->u4WriteIdx, Afe_Block->u4DMAReadIdx,
				       Afe_Block->u4DataRemained, (unsigned int)count);

		} else { /* copy twice */
			kal_uint32 size_1 = 0, size_2 = 0;

			size_1 = Align64ByteSize((Afe_Block->u4BufferSize - Afe_WriteIdx_tmp));
			size_2 = Align64ByteSize((copy_size - size_1));

			PRINTK_AUD_DL1("size_1=0x%x, size_2=0x%x\n", size_1, size_2);
			if (!access_ok(VERIFY_READ, data_w_ptr, size_1)) {
				pr_warn(" 1ptr invalid data_w_ptr=%p, size_1=%d", data_w_ptr, size_1);
				pr_warn(" u4BufferSize=%d, u4DataRemained=%d", Afe_Block->u4BufferSize,
				       Afe_Block->u4DataRemained);
			} else {

				PRINTK_AUD_DL1("mcmcpy Idx= %p data_w_ptr = %p size_1 = %x\n",
					       Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp, data_w_ptr, size_1);
				if ((copy_from_user((Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp), data_w_ptr ,
						    size_1))) {
					PRINTK_AUDDRV(" Fail 1 copy from user");
					return -1;
				}
			}
			spin_lock_irqsave(&auddrv_I2S0dl1_lock, flags);
			Afe_Block->u4DataRemained += size_1;
			Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + size_1;
			Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
			Afe_WriteIdx_tmp = Afe_Block->u4WriteIdx;
			spin_unlock_irqrestore(&auddrv_I2S0dl1_lock, flags);

			if (!access_ok(VERIFY_READ, data_w_ptr + size_1, size_2)) {
				PRINTK_AUDDRV("2ptr invalid data_w_ptr=%p, size_1=%d, size_2=%d", data_w_ptr,
					      size_1, size_2);
				PRINTK_AUDDRV("u4BufferSize=%d, u4DataRemained=%d", Afe_Block->u4BufferSize,
					      Afe_Block->u4DataRemained);
			} else {

				PRINTK_AUD_DL1("mcmcpy Idx= %p data_w_ptr+size_1 = %p size_2 = %x\n",
					       Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp,
					       data_w_ptr + size_1, size_2);
				if ((copy_from_user((Afe_Block->pucVirtBufAddr + Afe_WriteIdx_tmp),
						    (data_w_ptr + size_1), size_2))) {
					PRINTK_AUDDRV("AudDrv_write Fail 2  copy from user");
					return -1;
				}
			}
			spin_lock_irqsave(&auddrv_I2S0dl1_lock, flags);

			Afe_Block->u4DataRemained += size_2;
			Afe_Block->u4WriteIdx = Afe_WriteIdx_tmp + size_2;
			Afe_Block->u4WriteIdx %= Afe_Block->u4BufferSize;
			spin_unlock_irqrestore(&auddrv_I2S0dl1_lock, flags);
			count -= copy_size;
			data_w_ptr += copy_size;

			PRINTK_AUD_DL1(" finish2, copy size:%x, WriteIdx:%x,ReadIdx=%x DataRemained:%x \r\n",
				       copy_size, Afe_Block->u4WriteIdx, Afe_Block->u4DMAReadIdx,
				       Afe_Block->u4DataRemained);
		}
	}
	PRINTK_AUD_DL1("pcm_copy return\n");
	if (trace_copy) {
		m2note_audio_dl1_trace("copy_after", substream,
				       request_bytes, copy_size);
		m2note_audio_copy_trace_count++;
	}
	if (mStartDeferred && mtk_pcm_I2S0dl1_has_start_prefill(substream)) {
		mStartDeferred = false;
		ret = mtk_pcm_I2S0dl1_enable_path(substream,
						  "copy_deferred_start_done");
		if (ret)
			return ret;
	}
	return 0;
}

static int mtk_pcm_I2S0dl1_silence(struct snd_pcm_substream *substream,
				   int channel, snd_pcm_uframes_t pos,
				   snd_pcm_uframes_t count)
{
	pr_warn("%s\n", __func__);
	return 0; /* do nothing */
}

static void *dummy_page[2];

static struct page *mtk_I2S0dl1_pcm_page(struct snd_pcm_substream *substream,
					 unsigned long offset)
{
	pr_warn("%s\n", __func__);
	return virt_to_page(dummy_page[substream->stream]); /* the same page */
}

static struct snd_pcm_ops mtk_I2S0dl1_ops = {
	.open =     mtk_pcm_I2S0dl1_open,
	.close =    mtk_pcm_I2S0dl1_close,
	.ioctl =    snd_pcm_lib_ioctl,
	.hw_params =    mtk_pcm_I2S0dl1_hw_params,
	.hw_free =  mtk_pcm_I2S0dl1_hw_free,
	.prepare =  mtk_pcm_I2S0dl1_prepare,
	.trigger =  mtk_pcm_I2S0dl1_trigger,
	.pointer =  mtk_pcm_I2S0dl1_pointer,
	.copy =     mtk_pcm_I2S0dl1_copy,
	.silence =  mtk_pcm_I2S0dl1_silence,
	.page =     mtk_I2S0dl1_pcm_page,
};

static struct snd_soc_platform_driver mtk_I2S0dl1_soc_platform = {
	.ops        = &mtk_I2S0dl1_ops,
	.pcm_new    = mtk_asoc_pcm_I2S0dl1_new,
	.probe      = mtk_afe_I2S0dl1_probe,
};

static int mtk_I2S0dl1_probe(struct platform_device *pdev)
{
	pr_warn("%s\n", __func__);

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_I2S0DL1_PCM);

	pr_warn("%s: dev name %s\n", __func__, dev_name(&pdev->dev));

	mDev = &pdev->dev;

	return snd_soc_register_platform(&pdev->dev,
					 &mtk_I2S0dl1_soc_platform);
}

static int mtk_asoc_pcm_I2S0dl1_new(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;

	pr_warn("%s\n", __func__);
	return ret;
}


static int mtk_afe_I2S0dl1_probe(struct snd_soc_platform *platform)
{
	pr_warn("mtk_afe_I2S0dl1_probe\n");
	snd_soc_add_platform_controls(platform, Audio_snd_I2S0dl1_controls,
				      ARRAY_SIZE(Audio_snd_I2S0dl1_controls));
#if defined(CONFIG_SND_SOC_FLORIDA)
    mAudioDigitalI2S =  kzalloc(sizeof(AudioDigtalI2S), GFP_KERNEL);
#endif
	/* allocate dram */
	AudDrv_Allocate_mem_Buffer(platform->dev, Soc_Aud_Digital_Block_MEM_DL1,
				   Dl1_MAX_BUFFER_SIZE);
	Dl1_Playback_dma_buf =  Get_Mem_Buffer(Soc_Aud_Digital_Block_MEM_DL1);
	return 0;
}

static int mtk_I2S0dl1_remove(struct platform_device *pdev)
{
	pr_warn("%s\n", __func__);
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_pcm_dl1_i2s0Dl1_of_ids[] = {
	{ .compatible = "mediatek,mt_soc_pcm_dl1_i2s0Dl1", },
	{}
};
#endif

static struct platform_driver mtk_I2S0dl1_driver = {
	.driver = {
		.name = MT_SOC_I2S0DL1_PCM,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = mt_soc_pcm_dl1_i2s0Dl1_of_ids,
#endif
	},
	.probe = mtk_I2S0dl1_probe,
	.remove = mtk_I2S0dl1_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtkI2S0dl1_dev;
#endif

static int __init mtk_I2S0dl1_soc_platform_init(void)
{
	int ret;

	pr_warn("%s\n", __func__);
#ifndef CONFIG_OF
	soc_mtkI2S0dl1_dev = platform_device_alloc(MT_SOC_I2S0DL1_PCM, -1);
	if (!soc_mtkI2S0dl1_dev)
		return -ENOMEM;

	ret = platform_device_add(soc_mtkI2S0dl1_dev);
	if (ret != 0) {
		platform_device_put(soc_mtkI2S0dl1_dev);
		return ret;
	}
#endif

	ret = platform_driver_register(&mtk_I2S0dl1_driver);
	return ret;

}
module_init(mtk_I2S0dl1_soc_platform_init);

static void __exit mtk_I2S0dl1_soc_platform_exit(void)
{
	pr_warn("%s\n", __func__);

	platform_driver_unregister(&mtk_I2S0dl1_driver);
}
module_exit(mtk_I2S0dl1_soc_platform_exit);

MODULE_DESCRIPTION("AFE PCM module platform driver");
MODULE_LICENSE("GPL");
