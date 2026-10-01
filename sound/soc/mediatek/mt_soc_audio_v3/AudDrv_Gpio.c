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
 *   AudDrv_Gpio.c
 *
 * Project:
 * --------
 *   MT6735  Audio Driver GPIO
 *
 * Description:
 * ------------
 *   Audio register
 *
 * Author:
 * -------
 * George
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

#if !defined(CONFIG_MTK_LEGACY)
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/pinctrl/consumer.h>
#else
#include <mt-plat/mt_gpio.h>
#endif
#include "AudDrv_Gpio.h"
#include "mt_soc_afe_control.h"

#if !defined(CONFIG_MTK_LEGACY)
struct pinctrl *pinctrlaud;
/*struct pinctrl_state *pins_default;
struct pinctrl_state *audpmic_mode0, *audpmic_mode1, *audi2s1_mode0, *audi2s1_mode1;
struct pinctrl_state *audextamp_high, *audextamp_low, *audextamp2_high, *audextamp2_low;
struct pinctrl_state *audcvspk_high, *audcvspk_low;*/

enum audio_system_gpio_type {
	GPIO_DEFAULT = 0,
	GPIO_PMIC_MODE0,
	GPIO_PMIC_MODE1,
	GPIO_I2S_MODE0,
	GPIO_I2S_MODE1,
	GPIO_EXTAMP_HIGH,
	GPIO_EXTAMP_LOW,
	GPIO_EXTAMP2_HIGH,
	GPIO_EXTAMP2_LOW,
	GPIO_RCVSPK_HIGH,
	GPIO_RCVSPK_LOW,
	GPIO_NUM
};


struct audio_gpio_attr {
	const char *name;
	bool gpio_prepare;
	struct pinctrl_state *gpioctrl;
};

struct audio_gpio_legacy_fallback {
	const char *name;
	const char *prop;
	int gpio_info_type;
	int gpio;
	int pin_mode;
	bool ready;
};

static struct audio_gpio_attr aud_gpios[GPIO_NUM] = {
	[GPIO_DEFAULT] = {"default", false, NULL},
	[GPIO_PMIC_MODE0] = {"audpmicclk-mode0", false, NULL},
	[GPIO_PMIC_MODE1] = {"audpmicclk-mode1", false, NULL},
	[GPIO_I2S_MODE0] = {"audi2s1-mode0", false, NULL},
	[GPIO_I2S_MODE1] = {"audi2s1-mode1", false, NULL},
	[GPIO_EXTAMP_HIGH] = {"extamp-pullhigh", false, NULL},
	[GPIO_EXTAMP_LOW] = {"extamp-pulllow", false, NULL},
	[GPIO_EXTAMP2_HIGH] = {"extamp2-pullhigh", false, NULL},
	[GPIO_EXTAMP2_LOW] = {"extamp2-pulllow", false, NULL},
	[GPIO_RCVSPK_HIGH] = {"rcvspk-pullhigh", false, NULL},
	[GPIO_RCVSPK_LOW] = {"rcvspk-pulllow", false, NULL},
};

static struct device *aud_gpio_dev;

enum audio_gpio_legacy_fallback_type {
	GPIO_LEGACY_EXTAMP = 0,
	GPIO_LEGACY_EXTAMP2,
	GPIO_LEGACY_RCVSPK,
	GPIO_LEGACY_NUM
};

static struct audio_gpio_legacy_fallback aud_gpio_legacy_fallbacks[GPIO_LEGACY_NUM] = {
	[GPIO_LEGACY_EXTAMP] = {"extamp", "extspkamp-gpio", 5, -1, -1, false},
	[GPIO_LEGACY_EXTAMP2] = {"extamp2", "extspkamp_2-gpio", 10, -1, -1, false},
	[GPIO_LEGACY_RCVSPK] = {"rcvspk", "rcvspkswitch-gpio", 11, -1, -1, false},
};

static bool auddrv_gpio_read_legacy_fallback(struct audio_gpio_legacy_fallback *fallback,
					     int *pin, int *pin_mode)
{
	struct device_node *node;
	int ret;

	*pin = -1;
	*pin_mode = -1;

	ret = GetGPIO_Info(fallback->gpio_info_type, pin, pin_mode);
	if (!ret && *pin >= 0) {
		*pin &= 0x7fffffff;
		pr_warn("M2NOTE_AUDIO_EXTAMP_TRACE stage=legacy_getinfo name=%s gpio=%d pin_mode=%d\n",
			fallback->name, *pin, *pin_mode);
		return true;
	}

	node = of_find_compatible_node(NULL, NULL, "mediatek,mt-soc-dl1-pcm");
	if (!node) {
		pr_warn("M2NOTE_AUDIO_EXTAMP_TRACE stage=legacy_missing name=%s type=%d ret=%d pin=%d\n",
			fallback->name, fallback->gpio_info_type, ret, *pin);
		return false;
	}

	ret = of_property_read_u32_index(node, fallback->prop, 0, pin);
	if (!ret)
		ret = of_property_read_u32_index(node, fallback->prop, 1, pin_mode);
	of_node_put(node);
	if (ret) {
		pr_warn("M2NOTE_AUDIO_EXTAMP_TRACE stage=legacy_dt_missing name=%s prop=%s ret=%d\n",
			fallback->name, fallback->prop, ret);
		return false;
	}

	pr_warn("M2NOTE_AUDIO_EXTAMP_TRACE stage=legacy_dt name=%s prop=%s gpio=%d pin_mode=%d\n",
		fallback->name, fallback->prop, *pin, *pin_mode);
	return true;
}

static bool auddrv_gpio_prepare_legacy_fallback(struct device *dev,
						enum audio_gpio_legacy_fallback_type type)
{
	struct audio_gpio_legacy_fallback *fallback = &aud_gpio_legacy_fallbacks[type];
	int pin = -1;
	int pin_mode = -1;
	int ret;

	if (fallback->ready)
		return true;

	if (!auddrv_gpio_read_legacy_fallback(fallback, &pin, &pin_mode))
		return false;

	if (!gpio_is_valid(pin)) {
		pr_warn("M2NOTE_AUDIO_EXTAMP_TRACE stage=legacy_invalid name=%s gpio=%d pin_mode=%d\n",
			fallback->name, pin, pin_mode);
		return false;
	}

	if (dev)
		ret = devm_gpio_request_one(dev, pin, GPIOF_OUT_INIT_LOW, fallback->name);
	else
		ret = gpio_request_one(pin, GPIOF_OUT_INIT_LOW, fallback->name);
	if (ret) {
		pr_warn("M2NOTE_AUDIO_EXTAMP_TRACE stage=legacy_request_fail name=%s gpio=%d pin_mode=%d ret=%d\n",
			fallback->name, pin, pin_mode, ret);
		return false;
	}

	fallback->gpio = pin;
	fallback->pin_mode = pin_mode;
	fallback->ready = true;
	pr_warn("M2NOTE_AUDIO_EXTAMP_TRACE stage=legacy_ready name=%s gpio=%d pin_mode=%d\n",
		fallback->name, fallback->gpio, fallback->pin_mode);

	return true;
}

static int auddrv_gpio_select_state_or_legacy(enum audio_system_gpio_type high_state,
					      enum audio_system_gpio_type low_state,
					      enum audio_gpio_legacy_fallback_type type,
					      int bEnable)
{
	enum audio_system_gpio_type state = bEnable ? high_state : low_state;
	struct audio_gpio_legacy_fallback *fallback = &aud_gpio_legacy_fallbacks[type];
	int retval = 0;
	int gpio_ret = 0;

	if (aud_gpios[state].gpio_prepare) {
		retval = pinctrl_select_state(pinctrlaud, aud_gpios[state].gpioctrl);
		if (!retval &&
		    (fallback->ready || auddrv_gpio_prepare_legacy_fallback(aud_gpio_dev, type))) {
			gpio_ret = gpio_direction_output(fallback->gpio, bEnable ? 1 : 0);
			pr_warn("M2NOTE_AUDIO_EXTAMP_TRACE stage=select name=%s mode=pinctrl+legacy-gpio state=%s gpio=%d enable=%d ret=%d gpio_ret=%d\n",
				fallback->name, aud_gpios[state].name, fallback->gpio,
				bEnable, retval, gpio_ret);
			return gpio_ret ? gpio_ret : retval;
		}
		pr_warn("M2NOTE_AUDIO_EXTAMP_TRACE stage=select name=%s mode=pinctrl state=%s enable=%d ret=%d\n",
			fallback->name, aud_gpios[state].name, bEnable, retval);
		return retval;
	}

	if (fallback->ready) {
		retval = gpio_direction_output(fallback->gpio, bEnable ? 1 : 0);
		pr_warn("M2NOTE_AUDIO_EXTAMP_TRACE stage=select name=%s mode=legacy-gpio gpio=%d enable=%d ret=%d\n",
			fallback->name, fallback->gpio, bEnable, retval);
		return retval;
	}

	if (auddrv_gpio_prepare_legacy_fallback(aud_gpio_dev, type)) {
		retval = gpio_direction_output(fallback->gpio, bEnable ? 1 : 0);
		pr_warn("M2NOTE_AUDIO_EXTAMP_TRACE stage=select name=%s mode=legacy-gpio gpio=%d enable=%d ret=%d\n",
			fallback->name, fallback->gpio, bEnable, retval);
		return retval;
	}

	pr_warn("M2NOTE_AUDIO_EXTAMP_TRACE stage=select_missing name=%s state=%s enable=%d\n",
		fallback->name, aud_gpios[state].name, bEnable);
	return -ENODEV;
}


void AudDrv_GPIO_probe(void *dev)
{
	int ret;
	int i = 0;

	pr_warn("%s\n", __func__);
	aud_gpio_dev = dev;

	pinctrlaud = devm_pinctrl_get(dev);
	if (IS_ERR(pinctrlaud)) {
		ret = PTR_ERR(pinctrlaud);
		pr_err("Cannot find pinctrlaud!\n");
		return;
	}

	for (i = 0; i < ARRAY_SIZE(aud_gpios); i++) {
		aud_gpios[i].gpioctrl = pinctrl_lookup_state(pinctrlaud, aud_gpios[i].name);
		if (IS_ERR(aud_gpios[i].gpioctrl)) {
			ret = PTR_ERR(aud_gpios[i].gpioctrl);
			pr_err("%s pinctrl_lookup_state %s fail %d\n", __func__, aud_gpios[i].name,
			       ret);
		} else {
			aud_gpios[i].gpio_prepare = true;
		}
	}

	auddrv_gpio_prepare_legacy_fallback(aud_gpio_dev, GPIO_LEGACY_EXTAMP);
	auddrv_gpio_prepare_legacy_fallback(aud_gpio_dev, GPIO_LEGACY_EXTAMP2);
	auddrv_gpio_prepare_legacy_fallback(aud_gpio_dev, GPIO_LEGACY_RCVSPK);
#if 0
	pins_default = pinctrl_lookup_state(pinctrlaud, "default");
	if (IS_ERR(pins_default)) {
		ret = PTR_ERR(pins_default);
		dev_err(&pdev->dev, "Cannot find aud pinctrl default!\n");
		return;
	}

	audpmic_mode0 = pinctrl_lookup_state(pinctrlaud, "audpmicclk-mode0");
	if (IS_ERR(audpmic_mode0)) {
		ret = PTR_ERR(audpmic_mode0);
		dev_err(&pdev->dev, "Cannot find pinctrl audpmic_mode0!\n");
		return;
	}

	audpmic_mode1 = pinctrl_lookup_state(pinctrlaud, "audpmicclk-mode1");
	if (IS_ERR(audpmic_mode1)) {
		ret = PTR_ERR(audpmic_mode1);
		dev_err(&pdev->dev, "Cannot find pinctrl audpmic_mode1!\n");
		return;
	}

	audi2s1_mode0 = pinctrl_lookup_state(pinctrlaud, "audi2s1-mode0");
	if (IS_ERR(audi2s1_mode0)) {
		ret = PTR_ERR(audi2s1_mode0);
		dev_err(&pdev->dev, "Cannot find pinctrl audi2s1_mode0!\n");
		return;
	}

	audi2s1_mode1 = pinctrl_lookup_state(pinctrlaud, "audi2s1-mode1");
	if (IS_ERR(audi2s1_mode1)) {
		ret = PTR_ERR(audi2s1_mode1);
		dev_err(&pdev->dev, "Cannot find pinctrl audi2s1_mode1!\n");
		return;
	}


	audextamp_high = pinctrl_lookup_state(pinctrlaud, "extamp-pullhigh");
	if (IS_ERR(audextamp_high)) {
		ret = PTR_ERR(audextamp_high);
		dev_err(&pdev->dev, "Cannot find pinctrl audextamp_high!\n");
		return;
	}


	audextamp_low = pinctrl_lookup_state(pinctrlaud, "extamp-pulllow");
	if (IS_ERR(audextamp_low)) {
		ret = PTR_ERR(audextamp_low);
		dev_err(&pdev->dev, "Cannot find pinctrl audextamp_low!\n");
		return;
	}

	audextamp2_high = pinctrl_lookup_state(pinctrlaud, "extamp2-pullhigh");
	if (IS_ERR(audextamp2_high)) {
		ret = PTR_ERR(audextamp2_high);
		dev_err(&pdev->dev, "Cannot find pinctrl audextamp2_high!\n");
		return;
	}

	audextamp2_low = pinctrl_lookup_state(pinctrlaud, "extamp2-pulllow");
	if (IS_ERR(audextamp2_low)) {
		ret = PTR_ERR(audextamp2_low);
		dev_err(&pdev->dev, "Cannot find pinctrl audextamp2_low!\n");
		return;
	}

	audcvspk_high = pinctrl_lookup_state(pinctrlaud, "rcvspk-pullhigh");
	if (IS_ERR(audcvspk_high)) {
		ret = PTR_ERR(audcvspk_high);
		dev_err(&pdev->dev, "Cannot find pinctrl audcvspk_high!\n");
		return;
	}

	audcvspk_low = pinctrl_lookup_state(pinctrlaud, "rcvspk-pulllow");
	if (IS_ERR(audcvspk_low)) {
		ret = PTR_ERR(audcvspk_low);
		dev_err(&pdev->dev, "Cannot find pinctrl audcvspk_low!\n");
		return;
	}
#endif

}

int AudDrv_GPIO_PMIC_Select(int bEnable)
{
	int retval = 0;
	enum audio_system_gpio_type state = bEnable ? GPIO_PMIC_MODE1 : GPIO_PMIC_MODE0;

	if (bEnable == 1) {
		if (aud_gpios[state].gpio_prepare) {
			retval =
			    pinctrl_select_state(pinctrlaud, aud_gpios[state].gpioctrl);
			if (retval)
				pr_err("could not set aud_gpios[GPIO_PMIC_MODE1] pins\n");
		}
	} else {
		if (aud_gpios[state].gpio_prepare) {
			retval =
			    pinctrl_select_state(pinctrlaud, aud_gpios[state].gpioctrl);
			if (retval)
				pr_err("could not set aud_gpios[GPIO_PMIC_MODE0] pins\n");
		}

	}

	pr_warn("M2NOTE_AUDIO_PMIC_GPIO_TRACE stage=select enable=%d state=%s ready=%d ret=%d\n",
		bEnable, aud_gpios[state].name, aud_gpios[state].gpio_prepare, retval);
	return retval;
}

int AudDrv_GPIO_I2S_Select(int bEnable)
{
	int retval = 0;

	if (bEnable == 1) {
		if (aud_gpios[GPIO_I2S_MODE1].gpio_prepare) {
			retval =
			    pinctrl_select_state(pinctrlaud, aud_gpios[GPIO_I2S_MODE1].gpioctrl);
			if (retval)
				pr_err("could not set aud_gpios[GPIO_I2S_MODE1] pins\n");
		}
	} else {
		if (aud_gpios[GPIO_I2S_MODE0].gpio_prepare) {
			retval =
			    pinctrl_select_state(pinctrlaud, aud_gpios[GPIO_I2S_MODE0].gpioctrl);
			if (retval)
				pr_err("could not set aud_gpios[GPIO_I2S_MODE0] pins\n");
		}

	}
	return retval;
}

int AudDrv_GPIO_EXTAMP_Select(int bEnable)
{
	return auddrv_gpio_select_state_or_legacy(GPIO_EXTAMP_HIGH, GPIO_EXTAMP_LOW,
						  GPIO_LEGACY_EXTAMP, bEnable);
}

int AudDrv_GPIO_EXTAMP2_Select(int bEnable)
{
	return auddrv_gpio_select_state_or_legacy(GPIO_EXTAMP2_HIGH, GPIO_EXTAMP2_LOW,
						  GPIO_LEGACY_EXTAMP2, bEnable);
}

int AudDrv_GPIO_RCVSPK_Select(int bEnable)
{
	return auddrv_gpio_select_state_or_legacy(GPIO_RCVSPK_HIGH, GPIO_RCVSPK_LOW,
						  GPIO_LEGACY_RCVSPK, bEnable);
}

#endif
