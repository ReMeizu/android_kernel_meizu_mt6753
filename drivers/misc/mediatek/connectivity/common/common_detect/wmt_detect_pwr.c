/*
* Copyright (C) 2011-2014 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/regulator/consumer.h>
#include <mtk_rtc.h>
#include <upmu_common.h>

#ifdef DFT_TAG
#undef DFT_TAG
#endif
#define DFT_TAG         "[WMT-DETECT]"

#include "wmt_detect.h"
#include "wmt_gpio.h"

#define INVALID_PIN_ID (0xFFFFFFFF)

/*copied form WMT module*/
static int wmt_detect_dump_pin_conf(void)
{
	WMT_DETECT_INFO_FUNC("[WMT-DETECT]=>dump wmt pin configuration start<=\n");

	if (INVALID_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_LDO_EN_PIN].gpio_num) {
		WMT_DETECT_INFO_FUNC("LDO(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_LDO_EN_PIN].gpio_num);
	} else
		WMT_DETECT_INFO_FUNC("LDO(not defined)\n");

	if (INVALID_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMU_EN_PIN].gpio_num) {
		WMT_DETECT_INFO_FUNC("PMU(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMU_EN_PIN].gpio_num);
	} else
		WMT_DETECT_INFO_FUNC("PMU(not defined)\n");

	if (INVALID_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMUV28_EN_PIN].gpio_num) {
		WMT_DETECT_INFO_FUNC("PMUV28(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMUV28_EN_PIN].gpio_num);
	} else
		WMT_DETECT_INFO_FUNC("PMUV28(not defined)\n");

	if (INVALID_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_RST_PIN].gpio_num) {
		WMT_DETECT_INFO_FUNC("RST(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_RST_PIN].gpio_num);
	} else
		WMT_DETECT_INFO_FUNC("RST(not defined)\n");

	if (INVALID_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_BGF_EINT_PIN].gpio_num) {
		WMT_DETECT_INFO_FUNC("BGF_EINT(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_BGF_EINT_PIN].gpio_num);
	} else
		WMT_DETECT_INFO_FUNC("BGF_EINT(not defined)\n");

	if (INVALID_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_BGF_EINT_PIN].gpio_num) {
		WMT_DETECT_INFO_FUNC("BGF_EINT_NUM(%d)\n",
				gpio_to_irq(gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_BGF_EINT_PIN].gpio_num));
	} else
		WMT_DETECT_INFO_FUNC("BGF_EINT_NUM(not defined)\n");

	if (INVALID_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_WIFI_EINT_PIN].gpio_num) {
		WMT_DETECT_INFO_FUNC("WIFI_EINT(GPIO%d)\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_WIFI_EINT_PIN].gpio_num);
	} else
		WMT_DETECT_INFO_FUNC("WIFI_EINT(not defined)\n");

	if (INVALID_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_WIFI_EINT_PIN].gpio_num) {
		WMT_DETECT_INFO_FUNC("WIFI_EINT_NUM(%d)\n",
				gpio_to_irq(gpio_ctrl_info.gpio_ctrl_state[GPIO_WIFI_EINT_PIN].gpio_num));
	} else
		WMT_DETECT_INFO_FUNC("WIFI_EINT_NUM(not defined)\n");

	WMT_DETECT_INFO_FUNC("[WMT-PLAT]=>dump wmt pin configuration emds<=\n");

	return 0;
}

int _wmt_detect_output_low(unsigned int id)
{
	if (INVALID_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[id].gpio_num) {
		gpio_direction_output(gpio_ctrl_info.gpio_ctrl_state[id].gpio_num, 0);
		WMT_DETECT_DBG_FUNC("WMT-DETECT: set GPIO%d to output %d\n",
				gpio_ctrl_info.gpio_ctrl_state[id].gpio_num,
				gpio_get_value(gpio_ctrl_info.gpio_ctrl_state[id].gpio_num));
	}

	return 0;
}

int _wmt_detect_output_high(unsigned int id)
{
	if (INVALID_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[id].gpio_num) {
		gpio_direction_output(gpio_ctrl_info.gpio_ctrl_state[id].gpio_num, 1);
		WMT_DETECT_DBG_FUNC("WMT-DETECT: set GPIO%d to output %d\n",
				gpio_ctrl_info.gpio_ctrl_state[id].gpio_num,
				gpio_get_value(gpio_ctrl_info.gpio_ctrl_state[id].gpio_num));
	}

	return 0;
}

int _wmt_detect_read_gpio_input(unsigned int id)
{
	int retval = 0;

	if (INVALID_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[id].gpio_num) {
		retval = gpio_get_value(gpio_ctrl_info.gpio_ctrl_state[id].gpio_num);
		WMT_DETECT_DBG_FUNC("WMT-DETECT: get GPIO%d val%d\n",
				gpio_ctrl_info.gpio_ctrl_state[id].gpio_num, retval);
	}

	return retval;
}

static int m2note_wmt_detect_gpio_num(unsigned int id)
{
	return gpio_ctrl_info.gpio_ctrl_state[id].gpio_num;
}

static int m2note_wmt_detect_gpio_value(unsigned int id)
{
	if (INVALID_PIN_ID == gpio_ctrl_info.gpio_ctrl_state[id].gpio_num)
		return -1;
	return gpio_get_value(gpio_ctrl_info.gpio_ctrl_state[id].gpio_num);
}

static void m2note_wmt_detect_pwrseq_trace(const char *stage)
{
	WMT_DETECT_INFO_FUNC("M2NOTE_WCN_PWRSEQ_TRACE source=detect stage=%s pmu_gpio=%d pmu=%d rst_gpio=%d rst=%d wifi_eint_gpio=%d wifi_eint=%d delays=rtc%d/ldo%d/rst%d/off%d/on%d\n",
			     stage,
			     m2note_wmt_detect_gpio_num(GPIO_COMBO_PMU_EN_PIN),
			     m2note_wmt_detect_gpio_value(GPIO_COMBO_PMU_EN_PIN),
			     m2note_wmt_detect_gpio_num(GPIO_COMBO_RST_PIN),
			     m2note_wmt_detect_gpio_value(GPIO_COMBO_RST_PIN),
			     m2note_wmt_detect_gpio_num(GPIO_WIFI_EINT_PIN),
			     m2note_wmt_detect_gpio_value(GPIO_WIFI_EINT_PIN),
			     MAX_RTC_STABLE_TIME,
			     MAX_LDO_STABLE_TIME,
			     MAX_RST_STABLE_TIME,
			     MAX_OFF_STABLE_TIME,
			     MAX_ON_STABLE_TIME);
}

static int gM2noteWmtDetectPmicRailsOn;
static struct regulator *gM2noteWmtDetectVcn18;
static struct regulator *gM2noteWmtDetectVcn28;
static struct regulator *gM2noteWmtDetectVcn33Wifi;

#define M2NOTE_WCN_VCN18_UV		1800000
#define M2NOTE_WCN_VCN28_UV		2800000
#define M2NOTE_WCN_VCN33_WIFI_UV	3300000

static void m2note_wmt_detect_pmic_rail_trace(const char *stage, int vcn18, int vcn28, int vcn33_wifi)
{
	WMT_DETECT_INFO_FUNC("M2NOTE_WCN_DETECT_PMIC_RAIL_TRACE stage=%s rails_on=%d vcn18=%d vcn28=%d vcn33_wifi=%d\n",
			     stage, gM2noteWmtDetectPmicRailsOn, vcn18, vcn28, vcn33_wifi);
}

static int m2note_wmt_detect_regulator_status(struct regulator *reg)
{
	if (IS_ERR(reg))
		return PTR_ERR(reg);
	if (!reg)
		return -ENODEV;
	return 0;
}

static void m2note_wmt_detect_get_global_pmic_rails(void)
{
	if (IS_ERR_OR_NULL(gM2noteWmtDetectVcn18))
		gM2noteWmtDetectVcn18 = regulator_get(NULL, "vcn18");
	if (IS_ERR_OR_NULL(gM2noteWmtDetectVcn28))
		gM2noteWmtDetectVcn28 = regulator_get(NULL, "vcn28");
	if (IS_ERR_OR_NULL(gM2noteWmtDetectVcn33Wifi))
		gM2noteWmtDetectVcn33Wifi = regulator_get(NULL, "vcn33_wifi");
}

static int m2note_wmt_detect_get_pmic_rails(void)
{
	struct device_node *supply_node;
	struct platform_device *supply_pdev;
	int vcn18 = -ENODEV;
	int vcn28 = -ENODEV;
	int vcn33_wifi = -ENODEV;

	if (!IS_ERR_OR_NULL(gM2noteWmtDetectVcn18) &&
	    !IS_ERR_OR_NULL(gM2noteWmtDetectVcn28) &&
	    !IS_ERR_OR_NULL(gM2noteWmtDetectVcn33Wifi))
		return 0;

	supply_node = of_find_compatible_node(NULL, NULL,
		"mediatek,mt_pmic_regulator_supply");
	if (!supply_node) {
		m2note_wmt_detect_pmic_rail_trace("get_no_supply_node",
			-ENODEV, -ENODEV, -ENODEV);
	} else {
		supply_pdev = of_find_device_by_node(supply_node);
		of_node_put(supply_node);
		if (!supply_pdev) {
			m2note_wmt_detect_pmic_rail_trace("get_no_supply_pdev",
				-ENODEV, -ENODEV, -ENODEV);
		} else {
			if (IS_ERR_OR_NULL(gM2noteWmtDetectVcn18))
				gM2noteWmtDetectVcn18 = regulator_get(&supply_pdev->dev, "vcn18");
			if (IS_ERR_OR_NULL(gM2noteWmtDetectVcn28))
				gM2noteWmtDetectVcn28 = regulator_get(&supply_pdev->dev, "vcn28");
			if (IS_ERR_OR_NULL(gM2noteWmtDetectVcn33Wifi))
				gM2noteWmtDetectVcn33Wifi = regulator_get(&supply_pdev->dev, "vcn33_wifi");

			put_device(&supply_pdev->dev);

			vcn18 = m2note_wmt_detect_regulator_status(gM2noteWmtDetectVcn18);
			vcn28 = m2note_wmt_detect_regulator_status(gM2noteWmtDetectVcn28);
			vcn33_wifi = m2note_wmt_detect_regulator_status(gM2noteWmtDetectVcn33Wifi);
			m2note_wmt_detect_pmic_rail_trace("get", vcn18, vcn28, vcn33_wifi);
		}
	}

	if (vcn18 || vcn28 || vcn33_wifi) {
		m2note_wmt_detect_get_global_pmic_rails();
		vcn18 = m2note_wmt_detect_regulator_status(gM2noteWmtDetectVcn18);
		vcn28 = m2note_wmt_detect_regulator_status(gM2noteWmtDetectVcn28);
		vcn33_wifi = m2note_wmt_detect_regulator_status(gM2noteWmtDetectVcn33Wifi);
		m2note_wmt_detect_pmic_rail_trace("get_global", vcn18, vcn28, vcn33_wifi);
	}

	if (vcn18 || vcn28 || vcn33_wifi)
		return -ENODEV;
	return 0;
}

static int m2note_wmt_detect_regulator_enable(struct regulator *reg, int voltage)
{
	int ret;

	if (IS_ERR_OR_NULL(reg))
		return m2note_wmt_detect_regulator_status(reg);

	ret = regulator_set_voltage(reg, voltage, voltage);
	if (ret)
		WMT_DETECT_ERR_FUNC("M2NOTE_WCN_DETECT_PMIC_RAIL_TRACE stage=set_voltage_failed voltage=%d ret=%d\n",
				    voltage, ret);

	return regulator_enable(reg);
}

static int m2note_wmt_detect_regulator_disable(struct regulator *reg)
{
	if (IS_ERR_OR_NULL(reg))
		return m2note_wmt_detect_regulator_status(reg);

	return regulator_disable(reg);
}

static int m2note_wmt_detect_pmic_rails_ctrl(int on)
{
	int ret = 0;
	int vcn18 = 0;
	int vcn28 = 0;
	int vcn33_wifi = 0;

	if (on) {
		if (gM2noteWmtDetectPmicRailsOn) {
			m2note_wmt_detect_pmic_rail_trace("on_already", 0, 0, 0);
			return 0;
		}

		ret = m2note_wmt_detect_get_pmic_rails();
		if (ret)
			return ret;

		pmic_set_register_value(PMIC_RG_VCN18_ON_CTRL, 0);
		vcn18 = m2note_wmt_detect_regulator_enable(gM2noteWmtDetectVcn18,
			M2NOTE_WCN_VCN18_UV);
		pmic_set_register_value(PMIC_RG_VCN28_ON_CTRL, 1);
		vcn28 = m2note_wmt_detect_regulator_enable(gM2noteWmtDetectVcn28,
			M2NOTE_WCN_VCN28_UV);
		vcn33_wifi = m2note_wmt_detect_regulator_enable(gM2noteWmtDetectVcn33Wifi,
			M2NOTE_WCN_VCN33_WIFI_UV);
		pmic_set_register_value(PMIC_RG_VCN33_ON_CTRL_WIFI, 1);

		ret = vcn18 + vcn28 + vcn33_wifi;
		gM2noteWmtDetectPmicRailsOn = (0 == ret);
		m2note_wmt_detect_pmic_rail_trace("on", vcn18, vcn28, vcn33_wifi);
		if (ret) {
			if (!vcn33_wifi)
				m2note_wmt_detect_regulator_disable(gM2noteWmtDetectVcn33Wifi);
			if (!vcn28)
				m2note_wmt_detect_regulator_disable(gM2noteWmtDetectVcn28);
			if (!vcn18)
				m2note_wmt_detect_regulator_disable(gM2noteWmtDetectVcn18);
			gM2noteWmtDetectPmicRailsOn = 0;
		}
		return ret;
	}

	if (!gM2noteWmtDetectPmicRailsOn) {
		m2note_wmt_detect_pmic_rail_trace("off_already", 0, 0, 0);
		return 0;
	}

	pmic_set_register_value(PMIC_RG_VCN33_ON_CTRL_WIFI, 0);
	vcn33_wifi = m2note_wmt_detect_regulator_disable(gM2noteWmtDetectVcn33Wifi);
	pmic_set_register_value(PMIC_RG_VCN28_ON_CTRL, 0);
	vcn28 = m2note_wmt_detect_regulator_disable(gM2noteWmtDetectVcn28);
	pmic_set_register_value(PMIC_RG_VCN18_ON_CTRL, 0);
	vcn18 = m2note_wmt_detect_regulator_disable(gM2noteWmtDetectVcn18);

	ret = vcn18 + vcn28 + vcn33_wifi;
	gM2noteWmtDetectPmicRailsOn = 0;
	m2note_wmt_detect_pmic_rail_trace("off", vcn18, vcn28, vcn33_wifi);
	return ret;
}

/*This power on sequence must support all combo chip's basic power on sequence
 * 1. LDO control is a must, if external LDO exist
 * 2. PMU control is a must
 * 3. RST control is a must
 * 4. WIFI_EINT pin control is a must, used for GPIO mode for EINT status checkup
 * 5. RTC32k clock control is a must
 * */
static int wmt_detect_chip_pwr_on(void)
{
	int retval = -1;
	int rail_ret;
	/*setting validiation check*/
	if ((INVALID_PIN_ID == gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMU_EN_PIN].gpio_num) ||
		(INVALID_PIN_ID == gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_RST_PIN].gpio_num) ||
		(INVALID_PIN_ID == gpio_ctrl_info.gpio_ctrl_state[GPIO_WIFI_EINT_PIN].gpio_num)) {
		WMT_DETECT_ERR_FUNC("WMT-DETECT: either PMU(%d) or RST(%d) or WIFI_EINT(%d) is not set\n",
				gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMU_EN_PIN].gpio_num,
				gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_RST_PIN].gpio_num,
				gpio_ctrl_info.gpio_ctrl_state[GPIO_WIFI_EINT_PIN].gpio_num);

		return retval;
	}
	m2note_wmt_detect_pwrseq_trace("detect_pwr_on_start");
	/*set LDO/PMU/RST to output 0, no pull*/
	if (INVALID_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_LDO_EN_PIN].gpio_num)
		_wmt_detect_output_low(GPIO_COMBO_LDO_EN_PIN);
	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMU_EN_PIN].gpio_state[GPIO_PULL_DIS]) {
		pinctrl_select_state(gpio_ctrl_info.pinctrl_info,
				gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMU_EN_PIN].gpio_state[GPIO_PULL_DIS]);
		WMT_DETECT_INFO_FUNC("wmt_gpio:set GPIO_COMBO_PMU_EN_PIN to GPIO_PULL_DIS done!\n");
	} else
		WMT_DETECT_ERR_FUNC("wmt_gpio:set GPIO_COMBO_PMU_EN_PIN to GPIO_PULL_DIS fail, is NULL!\n");
	_wmt_detect_output_low(GPIO_COMBO_PMU_EN_PIN);
	if (gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_RST_PIN].gpio_state[GPIO_PULL_DIS]) {
		pinctrl_select_state(gpio_ctrl_info.pinctrl_info,
				gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_RST_PIN].gpio_state[GPIO_PULL_DIS]);
		WMT_DETECT_INFO_FUNC("wmt_gpio:set GPIO_COMBO_RST_PIN to GPIO_PULL_DIS done!\n");
	} else
		WMT_DETECT_ERR_FUNC("wmt_gpio:set GPIO_COMBO_RST_PIN to GPIO_PULL_DIS fail, is NULL!\n");
	_wmt_detect_output_low(GPIO_COMBO_RST_PIN);
	m2note_wmt_detect_pwrseq_trace("detect_rst_pmu_low");

#if 0
	_wmt_detect_output_high(GPIO_WIFI_EINT_PIN);
#endif

	rail_ret = m2note_wmt_detect_pmic_rails_ctrl(1);
	if (rail_ret)
		WMT_DETECT_ERR_FUNC("M2NOTE_WCN_DETECT_PMIC_RAIL_TRACE stage=on_failed ret=%d\n", rail_ret);

	/*pull high LDO*/
	if (INVALID_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_LDO_EN_PIN].gpio_num)
		_wmt_detect_output_high(GPIO_COMBO_LDO_EN_PIN);
	/*sleep for LDO stable time*/
	msleep(MAX_LDO_STABLE_TIME);

	/*export RTC clock, sleep for RTC stable time*/
	rtc_gpio_enable_32k(RTC_GPIO_USER_GPS);
	msleep(MAX_RTC_STABLE_TIME);
	/*PMU output low, RST output low, to make chip power off completely*/
	/*always done*/
	/*sleep for power off stable time*/
	msleep(MAX_OFF_STABLE_TIME);
	/*PMU output high, and sleep for reset stable time*/
	_wmt_detect_output_high(GPIO_COMBO_PMU_EN_PIN);
	m2note_wmt_detect_pwrseq_trace("detect_pmu_high");
#ifdef CONFIG_MTK_COMBO_COMM_NPWR
	if ((gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_I2S_DAT_PIN].gpio_num != INVALID_PIN_ID) &&
		(gpio_ctrl_info.gpio_ctrl_state[GPIO_PCM_DAISYNC_PIN].gpio_num != INVALID_PIN_ID)) {
		msleep(20);
		_wmt_detect_output_high(GPIO_PCM_DAISYNC_PIN);

		msleep(20);
		_wmt_detect_output_high(GPIO_COMBO_I2S_DAT_PIN);

		msleep(20);
		_wmt_detect_output_low(GPIO_COMBO_I2S_DAT_PIN);

		msleep(20);
		_wmt_detect_output_low(GPIO_PCM_DAISYNC_PIN);

		msleep(20);
	}
#endif
	msleep(MAX_RST_STABLE_TIME);
	/*RST output high, and sleep for power on stable time */
	_wmt_detect_output_high(GPIO_COMBO_RST_PIN);
	msleep(MAX_ON_STABLE_TIME);
	m2note_wmt_detect_pwrseq_trace("detect_rst_high");

	retval = 0;
	return retval;
}

static int wmt_detect_chip_pwr_off(void)
{

	/*set RST pin to input low status*/
	if (INVALID_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_LDO_EN_PIN].gpio_num)
		_wmt_detect_output_low(GPIO_COMBO_LDO_EN_PIN);
	/*set RST pin to input low status*/
	if (INVALID_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_RST_PIN].gpio_num)
		_wmt_detect_output_low(GPIO_COMBO_RST_PIN);
	/*set PMU pin to input low status*/
	if (INVALID_PIN_ID != gpio_ctrl_info.gpio_ctrl_state[GPIO_COMBO_PMU_EN_PIN].gpio_num)
		_wmt_detect_output_low(GPIO_COMBO_PMU_EN_PIN);
	m2note_wmt_detect_pmic_rails_ctrl(0);
	m2note_wmt_detect_pwrseq_trace("detect_pwr_off_finish");
	return 0;
}

int wmt_detect_read_ext_cmb_status(void)
{
	int retval = 0;
	/*read WIFI_EINT pin status*/
	if (INVALID_PIN_ID == gpio_ctrl_info.gpio_ctrl_state[GPIO_WIFI_EINT_PIN].gpio_num) {
		retval = 0;
		WMT_DETECT_ERR_FUNC("WMT-DETECT: no WIFI_EINT pin set\n");
	} else {
		retval = _wmt_detect_read_gpio_input(GPIO_WIFI_EINT_PIN);
		WMT_DETECT_ERR_FUNC("WMT-DETECT: WIFI_EINT input status:%d\n", retval);
	}
	return retval;
}

int wmt_detect_chip_pwr_ctrl(int on)
{
	int retval = -1;

	if (0 == on) {
		/*power off combo chip */
		retval = wmt_detect_chip_pwr_off();
	} else {
		wmt_detect_dump_pin_conf();
		/*power on combo chip */
		retval = wmt_detect_chip_pwr_on();
	}
	return retval;
}

int wmt_detect_sdio_pwr_ctrl(int on)
{
	int retval = -1;
#ifdef MTK_WCN_COMBO_CHIP_SUPPORT
	if (0 == on) {
		/*power off SDIO slot */
		retval = board_sdio_ctrl(1, 0);
	} else {
		/*power on SDIO slot */
		retval = board_sdio_ctrl(1, 1);
	}
#else
	WMT_DETECT_WARN_FUNC("WMT-DETECT: MTK_WCN_COMBO_CHIP_SUPPORT is not set\n");
#endif
	return retval;
}
