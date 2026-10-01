/*! \file
    \brief  Declaration of library functions

    Any definitions in this file will be shared among GLUE Layer and internal Driver Stack.
*/




/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
#ifdef DFT_TAG
#undef DFT_TAG
#endif
#define DFT_TAG "[WMT-CMB-HW]"


/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

#include "wmt_plat.h"
#include "wmt_lib.h"
#include "mtk_wcn_cmb_hw.h"
#include "osal_typedef.h"
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/regulator/consumer.h>
#include <upmu_common.h>


/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define DFT_RTC_STABLE_TIME 100
#define DFT_LDO_STABLE_TIME 100
#define DFT_RST_STABLE_TIME 30
#define DFT_OFF_STABLE_TIME 10
#define DFT_ON_STABLE_TIME 30

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/



/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

PWR_SEQ_TIME gPwrSeqTime;
static INT32 gM2noteWcnPmicRailsOn;
static struct regulator *gM2noteWcnVcn18;
static struct regulator *gM2noteWcnVcn28;
static struct regulator *gM2noteWcnVcn33Wifi;

#define M2NOTE_WCN_VCN18_UV		1800000
#define M2NOTE_WCN_VCN28_UV		2800000
#define M2NOTE_WCN_VCN33_WIFI_UV	3300000

static VOID m2note_wcn_pmic_rail_trace(const char *stage, INT32 vcn18, INT32 vcn28, INT32 vcn33_wifi)
{
	WMT_WARN_FUNC("M2NOTE_WCN_PMIC_RAIL_TRACE stage=%s rails_on=%d vcn18=%d vcn28=%d vcn33_wifi=%d\n",
		      stage, gM2noteWcnPmicRailsOn, vcn18, vcn28, vcn33_wifi);
}

static INT32 m2note_wcn_regulator_status(struct regulator *reg)
{
	if (IS_ERR(reg))
		return PTR_ERR(reg);
	if (!reg)
		return -ENODEV;
	return 0;
}

static INT32 m2note_wcn_regulator_enabled(struct regulator *reg)
{
	if (IS_ERR_OR_NULL(reg))
		return m2note_wcn_regulator_status(reg);
	return regulator_is_enabled(reg);
}

static INT32 m2note_wcn_regulator_voltage(struct regulator *reg)
{
	if (IS_ERR_OR_NULL(reg))
		return m2note_wcn_regulator_status(reg);
	return regulator_get_voltage(reg);
}

static VOID m2note_wcn_pmic_rail_state_trace(const char *stage)
{
	WMT_WARN_FUNC("M2NOTE_WCN_PMIC_RAIL_STATE_TRACE stage=%s rails_on=%d "
		      "vcn18_en=%d vcn18_uv=%d vcn28_en=%d vcn28_uv=%d "
		      "vcn33_wifi_en=%d vcn33_wifi_uv=%d on_ctrl18=%u "
		      "on_ctrl28=%u on_ctrl33wifi=%u\n",
		      stage, gM2noteWcnPmicRailsOn,
		      m2note_wcn_regulator_enabled(gM2noteWcnVcn18),
		      m2note_wcn_regulator_voltage(gM2noteWcnVcn18),
		      m2note_wcn_regulator_enabled(gM2noteWcnVcn28),
		      m2note_wcn_regulator_voltage(gM2noteWcnVcn28),
		      m2note_wcn_regulator_enabled(gM2noteWcnVcn33Wifi),
		      m2note_wcn_regulator_voltage(gM2noteWcnVcn33Wifi),
		      pmic_get_register_value(PMIC_RG_VCN18_ON_CTRL),
		      pmic_get_register_value(PMIC_RG_VCN28_ON_CTRL),
		      pmic_get_register_value(PMIC_RG_VCN33_ON_CTRL_WIFI));
}

static VOID m2note_wcn_get_global_pmic_rails(VOID)
{
	if (IS_ERR_OR_NULL(gM2noteWcnVcn18))
		gM2noteWcnVcn18 = regulator_get(NULL, "vcn18");
	if (IS_ERR_OR_NULL(gM2noteWcnVcn28))
		gM2noteWcnVcn28 = regulator_get(NULL, "vcn28");
	if (IS_ERR_OR_NULL(gM2noteWcnVcn33Wifi))
		gM2noteWcnVcn33Wifi = regulator_get(NULL, "vcn33_wifi");
}

static INT32 m2note_wcn_get_pmic_rails(VOID)
{
	struct device_node *supply_node;
	struct platform_device *supply_pdev;
	INT32 vcn18 = -ENODEV;
	INT32 vcn28 = -ENODEV;
	INT32 vcn33_wifi = -ENODEV;

	if (!IS_ERR_OR_NULL(gM2noteWcnVcn18) &&
	    !IS_ERR_OR_NULL(gM2noteWcnVcn28) &&
	    !IS_ERR_OR_NULL(gM2noteWcnVcn33Wifi))
		return 0;

	supply_node = of_find_compatible_node(NULL, NULL,
		"mediatek,mt_pmic_regulator_supply");
	if (!supply_node) {
		m2note_wcn_pmic_rail_trace("get_no_supply_node",
			-ENODEV, -ENODEV, -ENODEV);
	} else {
		supply_pdev = of_find_device_by_node(supply_node);
		of_node_put(supply_node);
		if (!supply_pdev) {
			m2note_wcn_pmic_rail_trace("get_no_supply_pdev",
				-ENODEV, -ENODEV, -ENODEV);
		} else {
			if (IS_ERR_OR_NULL(gM2noteWcnVcn18))
				gM2noteWcnVcn18 = regulator_get(&supply_pdev->dev, "vcn18");
			if (IS_ERR_OR_NULL(gM2noteWcnVcn28))
				gM2noteWcnVcn28 = regulator_get(&supply_pdev->dev, "vcn28");
			if (IS_ERR_OR_NULL(gM2noteWcnVcn33Wifi))
				gM2noteWcnVcn33Wifi = regulator_get(&supply_pdev->dev, "vcn33_wifi");

			put_device(&supply_pdev->dev);

			vcn18 = m2note_wcn_regulator_status(gM2noteWcnVcn18);
			vcn28 = m2note_wcn_regulator_status(gM2noteWcnVcn28);
			vcn33_wifi = m2note_wcn_regulator_status(gM2noteWcnVcn33Wifi);
			m2note_wcn_pmic_rail_trace("get", vcn18, vcn28, vcn33_wifi);
		}
	}

	if (vcn18 || vcn28 || vcn33_wifi) {
		m2note_wcn_get_global_pmic_rails();
		vcn18 = m2note_wcn_regulator_status(gM2noteWcnVcn18);
		vcn28 = m2note_wcn_regulator_status(gM2noteWcnVcn28);
		vcn33_wifi = m2note_wcn_regulator_status(gM2noteWcnVcn33Wifi);
		m2note_wcn_pmic_rail_trace("get_global", vcn18, vcn28, vcn33_wifi);
	}

	if (vcn18 || vcn28 || vcn33_wifi)
		return -ENODEV;
	m2note_wcn_pmic_rail_state_trace("get_ready");
	return 0;
}

static INT32 m2note_wcn_regulator_enable(struct regulator *reg, INT32 voltage)
{
	INT32 ret;

	if (IS_ERR_OR_NULL(reg))
		return m2note_wcn_regulator_status(reg);

	ret = regulator_set_voltage(reg, voltage, voltage);
	if (ret)
		WMT_WARN_FUNC("M2NOTE_WCN_PMIC_RAIL_TRACE stage=set_voltage_failed voltage=%d ret=%d\n",
			      voltage, ret);

	return regulator_enable(reg);
}

static INT32 m2note_wcn_regulator_disable(struct regulator *reg)
{
	if (IS_ERR_OR_NULL(reg))
		return m2note_wcn_regulator_status(reg);

	return regulator_disable(reg);
}

static INT32 m2note_wcn_pmic_rails_ctrl(INT32 on)
{
	INT32 ret = 0;
	INT32 vcn18 = 0;
	INT32 vcn28 = 0;
	INT32 vcn33_wifi = 0;

	if ((0x6630 != mtk_wcn_wmt_chipid_query()) ||
	    (STP_SDIO_IF_TX != wmt_plat_get_comm_if_type()))
		return 0;

	if (on) {
		if (gM2noteWcnPmicRailsOn) {
			m2note_wcn_pmic_rail_trace("on_already", 0, 0, 0);
			m2note_wcn_pmic_rail_state_trace("on_already");
			return 0;
		}

		ret = m2note_wcn_get_pmic_rails();
		if (ret)
			return ret;

		pmic_set_register_value(PMIC_RG_VCN18_ON_CTRL, 0);
		vcn18 = m2note_wcn_regulator_enable(gM2noteWcnVcn18,
			M2NOTE_WCN_VCN18_UV);
		pmic_set_register_value(PMIC_RG_VCN28_ON_CTRL, 1);
		vcn28 = m2note_wcn_regulator_enable(gM2noteWcnVcn28,
			M2NOTE_WCN_VCN28_UV);
		vcn33_wifi = m2note_wcn_regulator_enable(gM2noteWcnVcn33Wifi,
			M2NOTE_WCN_VCN33_WIFI_UV);
		pmic_set_register_value(PMIC_RG_VCN33_ON_CTRL_WIFI, 1);

		ret = vcn18 + vcn28 + vcn33_wifi;
		gM2noteWcnPmicRailsOn = (0 == ret);
		m2note_wcn_pmic_rail_trace("on", vcn18, vcn28, vcn33_wifi);
		m2note_wcn_pmic_rail_state_trace("on");
		if (ret) {
			if (!vcn33_wifi)
				m2note_wcn_regulator_disable(gM2noteWcnVcn33Wifi);
			if (!vcn28)
				m2note_wcn_regulator_disable(gM2noteWcnVcn28);
			if (!vcn18)
				m2note_wcn_regulator_disable(gM2noteWcnVcn18);
			gM2noteWcnPmicRailsOn = 0;
		}
		return ret;
	}

	if (!gM2noteWcnPmicRailsOn) {
		m2note_wcn_pmic_rail_trace("off_already", 0, 0, 0);
		m2note_wcn_pmic_rail_state_trace("off_already");
		return 0;
	}

	pmic_set_register_value(PMIC_RG_VCN33_ON_CTRL_WIFI, 0);
	vcn33_wifi = m2note_wcn_regulator_disable(gM2noteWcnVcn33Wifi);
	pmic_set_register_value(PMIC_RG_VCN28_ON_CTRL, 0);
	vcn28 = m2note_wcn_regulator_disable(gM2noteWcnVcn28);
	pmic_set_register_value(PMIC_RG_VCN18_ON_CTRL, 0);
	vcn18 = m2note_wcn_regulator_disable(gM2noteWcnVcn18);

	ret = vcn18 + vcn28 + vcn33_wifi;
	gM2noteWcnPmicRailsOn = 0;
	m2note_wcn_pmic_rail_trace("off", vcn18, vcn28, vcn33_wifi);
	m2note_wcn_pmic_rail_state_trace("off");
	return ret;
}

static VOID m2note_wcn_pwrseq_trace(const char *stage)
{
	WMT_WARN_FUNC("M2NOTE_WCN_PWRSEQ_TRACE source=cmb stage=%s chip=0x%x if=%d delays=rtc%d/ldo%d/rst%d/off%d/on%d\n",
		      stage,
		      mtk_wcn_wmt_chipid_query(),
		      wmt_plat_get_comm_if_type(),
		      gPwrSeqTime.rtcStableTime,
		      gPwrSeqTime.ldoStableTime,
		      gPwrSeqTime.rstStableTime,
		      gPwrSeqTime.offStableTime,
		      gPwrSeqTime.onStableTime);
	wmt_plat_gpio_ctrl(PIN_PMU, PIN_STA_SHOW);
	wmt_plat_gpio_ctrl(PIN_RST, PIN_STA_SHOW);
	wmt_plat_gpio_ctrl(PIN_WIFI_EINT, PIN_STA_SHOW);
}




/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/



/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

INT32 mtk_wcn_cmb_hw_pwr_off(VOID)
{
	INT32 iRet = 0;

	WMT_INFO_FUNC("CMB-HW, hw_pwr_off start\n");
	m2note_wcn_pwrseq_trace("cmb_pwr_off_start");

	/*1. disable irq --> should be done when do wmt-ic swDeinit period */
	/* TODO:[FixMe][GeorgeKuo] clarify this */

	/*2. set bgf eint/all eint to deinit state, namely input low state */
	if (!((0x6630 == mtk_wcn_wmt_chipid_query())
				&& (STP_SDIO_IF_TX == wmt_plat_get_comm_if_type()))) {
		iRet += wmt_plat_eirq_ctrl(PIN_BGF_EINT, PIN_STA_EINT_DIS);
		WMT_INFO_FUNC("CMB-HW, BGF_EINT IRQ unregistered and disabled\n");
		iRet += wmt_plat_gpio_ctrl(PIN_BGF_EINT, PIN_STA_DEINIT);
	}
	/* 2.1 set ALL_EINT pin to correct state even it is not used currently */
	iRet += wmt_plat_eirq_ctrl(PIN_ALL_EINT, PIN_STA_DEINIT);
	WMT_INFO_FUNC("CMB-HW, ALL_EINT IRQ unregistered and disabled\n");
	iRet += wmt_plat_gpio_ctrl(PIN_ALL_EINT, PIN_STA_DEINIT);
	/* 2.2 deinit gps sync */
	iRet += wmt_plat_gpio_ctrl(PIN_GPS_SYNC, PIN_STA_DEINIT);

	/*3. set audio interface to CMB_STUB_AIF_0, BT PCM OFF, I2S OFF */
	iRet += wmt_plat_audio_ctrl(CMB_STUB_AIF_0, CMB_STUB_AIF_CTRL_DIS);

	/*4. set control gpio into deinit state, namely input low state */
	iRet += wmt_plat_gpio_ctrl(PIN_SDIO_GRP, PIN_STA_DEINIT);
	iRet += wmt_plat_gpio_ctrl(PIN_RST, PIN_STA_OUT_L);
	iRet += wmt_plat_gpio_ctrl(PIN_PMU, PIN_STA_OUT_L);
	m2note_wcn_pwrseq_trace("cmb_pwr_off_rst_pmu_low");

	/*5. set uart tx/rx into deinit state, namely input low state */
	iRet += wmt_plat_gpio_ctrl(PIN_UART_GRP, PIN_STA_DEINIT);

	/* 6. Last, LDO output low */
	iRet += wmt_plat_gpio_ctrl(PIN_LDO, PIN_STA_OUT_L);
	iRet += m2note_wcn_pmic_rails_ctrl(0);

	/*7. deinit gps_lna */
	iRet += wmt_plat_gpio_ctrl(PIN_GPS_LNA, PIN_STA_DEINIT);

	WMT_INFO_FUNC("CMB-HW, hw_pwr_off finish\n");
	m2note_wcn_pwrseq_trace("cmb_pwr_off_finish");
	return iRet;
}

INT32 mtk_wcn_cmb_hw_pwr_on(VOID)
{
	static UINT32 _pwr_first_time = 1;
	INT32 iRet = 0;

	WMT_INFO_FUNC("CMB-HW, hw_pwr_on start\n");
	m2note_wcn_pwrseq_trace("cmb_pwr_on_start");

	/* disable interrupt firstly */
	if (!((0x6630 == mtk_wcn_wmt_chipid_query())
	      && (STP_SDIO_IF_TX == wmt_plat_get_comm_if_type())))
		iRet += wmt_plat_eirq_ctrl(PIN_BGF_EINT, PIN_STA_EINT_DIS);
	iRet += wmt_plat_eirq_ctrl(PIN_ALL_EINT, PIN_STA_EINT_DIS);

	/*set all control and eint gpio to init state, namely input low mode */
	iRet += wmt_plat_gpio_ctrl(PIN_LDO, PIN_STA_INIT);
	iRet += wmt_plat_gpio_ctrl(PIN_PMU, PIN_STA_INIT);
	iRet += wmt_plat_gpio_ctrl(PIN_RST, PIN_STA_INIT);
	iRet += wmt_plat_gpio_ctrl(PIN_SDIO_GRP, PIN_STA_INIT);
	if (!((0x6630 == mtk_wcn_wmt_chipid_query())
	      && (STP_SDIO_IF_TX == wmt_plat_get_comm_if_type())))
		iRet += wmt_plat_gpio_ctrl(PIN_BGF_EINT, PIN_STA_INIT);
	iRet += wmt_plat_gpio_ctrl(PIN_ALL_EINT, PIN_STA_INIT);
	iRet += wmt_plat_gpio_ctrl(PIN_GPS_SYNC, PIN_STA_INIT);
	iRet += wmt_plat_gpio_ctrl(PIN_GPS_LNA, PIN_STA_INIT);
	/* wmt_plat_gpio_ctrl(PIN_WIFI_EINT, PIN_STA_INIT); *//* WIFI_EINT is controlled by SDIO host driver */
	/* TODO: [FixMe][George]:WIFI_EINT is used in common SDIO */
	m2note_wcn_pwrseq_trace("cmb_pwr_on_after_gpio_init");

	/*1. pull high LDO to supply power to chip */
	iRet += m2note_wcn_pmic_rails_ctrl(1);
	iRet += wmt_plat_gpio_ctrl(PIN_LDO, PIN_STA_OUT_H);
	osal_sleep_ms(gPwrSeqTime.ldoStableTime);

	/* 2. export RTC clock to chip */
	if (_pwr_first_time) {
		/* rtc clock should be output all the time, so no need to enable output again */
		iRet += wmt_plat_gpio_ctrl(PIN_RTC, PIN_STA_INIT);
		osal_sleep_ms(gPwrSeqTime.rtcStableTime);
		WMT_INFO_FUNC("CMB-HW, rtc clock exported\n");
	}

	/*3. set UART Tx/Rx to UART mode */
	iRet += wmt_plat_gpio_ctrl(PIN_UART_GRP, PIN_STA_INIT);

	if (0x6630 == mtk_wcn_wmt_chipid_query()) {
		switch (wmt_plat_get_comm_if_type()) {
		case STP_UART_IF_TX:
			iRet += wmt_plat_gpio_ctrl(PIN_UART_RX, PIN_STA_OUT_H);
			break;
		case STP_SDIO_IF_TX:
				iRet += wmt_plat_gpio_ctrl(PIN_UART_RX, PIN_STA_IN_L);
			break;
		default:
			WMT_ERR_FUNC("not supported common interface\n");
			break;
		}
	}
	/*4. PMU->output low, RST->output low, sleep off stable time */
	iRet += wmt_plat_gpio_ctrl(PIN_PMU, PIN_STA_OUT_L);
	iRet += wmt_plat_gpio_ctrl(PIN_RST, PIN_STA_OUT_L);
	osal_sleep_ms(gPwrSeqTime.offStableTime);
	m2note_wcn_pwrseq_trace("cmb_pwr_on_rst_pmu_low");

	/*5. PMU->output high, sleep rst stable time */
	iRet += wmt_plat_gpio_ctrl(PIN_PMU, PIN_STA_OUT_H);
	osal_sleep_ms(gPwrSeqTime.rstStableTime);
	m2note_wcn_pwrseq_trace("cmb_pwr_on_pmu_high");

	/*6. RST->output high, sleep on stable time */
	iRet += wmt_plat_gpio_ctrl(PIN_RST, PIN_STA_OUT_H);
	osal_sleep_ms(gPwrSeqTime.onStableTime);
	m2note_wcn_pwrseq_trace("cmb_pwr_on_rst_high");

	/*set UART Tx/Rx to UART mode */
	if (0x6630 == mtk_wcn_wmt_chipid_query())
			iRet += wmt_plat_gpio_ctrl(PIN_UART_RX, PIN_STA_IN_H);


	/*7. set audio interface to CMB_STUB_AIF_1, BT PCM ON, I2S OFF */
	/* BT PCM bus default mode. Real control is done by audio */
	iRet += wmt_plat_audio_ctrl(CMB_STUB_AIF_1, CMB_STUB_AIF_CTRL_DIS);

	/*8. set EINT< -ommited-> move this to WMT-IC module,
	   where common sdio interface will be identified and do proper operation */
	/* TODO: [FixMe][GeorgeKuo] double check if BGF_INT is implemented ok */
	if (!((0x6630 == mtk_wcn_wmt_chipid_query())
	      && (STP_SDIO_IF_TX == wmt_plat_get_comm_if_type()))) {
		iRet += wmt_plat_gpio_ctrl(PIN_BGF_EINT, PIN_STA_MUX);
		iRet += wmt_plat_eirq_ctrl(PIN_BGF_EINT, PIN_STA_INIT);
		WMT_INFO_FUNC("CMB-HW, BGF_EINT IRQ registered and disabled\n");
	} else {
		WMT_INFO_FUNC("CMB-HW, no need to register BGF_EINT for MT6630 SDIO mode\n");
	}

	/* 8.1 set ALL_EINT pin to correct state even it is not used currently */
	iRet += wmt_plat_gpio_ctrl(PIN_ALL_EINT, PIN_STA_MUX);
	iRet += wmt_plat_eirq_ctrl(PIN_ALL_EINT, PIN_STA_INIT);
	WMT_INFO_FUNC("CMB-HW, hw_pwr_on finish (%d)\n", iRet);
	m2note_wcn_pwrseq_trace("cmb_pwr_on_finish");

	_pwr_first_time = 0;
	return iRet;

}

INT32 mtk_wcn_cmb_hw_rst(VOID)
{
	INT32 iRet = 0;

	WMT_INFO_FUNC("CMB-HW, hw_rst start, eirq should be disabled before this step\n");
	if (0x6630 == mtk_wcn_wmt_chipid_query()) {
		switch (wmt_plat_get_comm_if_type()) {
		case STP_UART_IF_TX:
			iRet += wmt_plat_gpio_ctrl(PIN_UART_RX, PIN_STA_OUT_H);
			break;
		case STP_SDIO_IF_TX:
				iRet += wmt_plat_gpio_ctrl(PIN_UART_RX, PIN_STA_IN_L);
			break;
		default:
			WMT_ERR_FUNC("not supported common interface\n");
			break;
		}
	}

	/*1. PMU->output low, RST->output low, sleep off stable time */
	iRet += wmt_plat_gpio_ctrl(PIN_PMU, PIN_STA_OUT_L);
	iRet += wmt_plat_gpio_ctrl(PIN_RST, PIN_STA_OUT_L);
	osal_sleep_ms(gPwrSeqTime.offStableTime);

	/*2. PMU->output high, sleep rst stable time */
	iRet += wmt_plat_gpio_ctrl(PIN_PMU, PIN_STA_OUT_H);
	osal_sleep_ms(gPwrSeqTime.rstStableTime);

	/*3. RST->output high, sleep on stable time */
	iRet += wmt_plat_gpio_ctrl(PIN_RST, PIN_STA_OUT_H);
	osal_sleep_ms(gPwrSeqTime.onStableTime);

	/*set UART Tx/Rx to UART mode */
	if (0x6630 == mtk_wcn_wmt_chipid_query())
			iRet += wmt_plat_gpio_ctrl(PIN_UART_RX, PIN_STA_IN_H);

	WMT_INFO_FUNC("CMB-HW, hw_rst finish, eirq should be enabled after this step\n");
	return 0;
}

static VOID mtk_wcn_cmb_hw_dmp_seq(VOID)
{
	PUINT32 pTimeSlot = (PUINT32) &gPwrSeqTime;

	WMT_INFO_FUNC
	    ("combo chip power on sequence time, RTC (%d), LDO (%d), RST(%d), OFF(%d), ON(%d)\n",
	     pTimeSlot[0],
		      /**pTimeSlot++,*/
	     pTimeSlot[1], pTimeSlot[2], pTimeSlot[3], pTimeSlot[4]
	    );
}

INT32 mtk_wcn_cmb_hw_state_show(VOID)
{
	wmt_plat_gpio_ctrl(PIN_PMU, PIN_STA_SHOW);
	wmt_plat_gpio_ctrl(PIN_RST, PIN_STA_SHOW);
	wmt_plat_gpio_ctrl(PIN_RTC, PIN_STA_SHOW);
	return 0;
}



INT32 mtk_wcn_cmb_hw_init(P_PWR_SEQ_TIME pPwrSeqTime)
{
	if (NULL != pPwrSeqTime &&
	    pPwrSeqTime->ldoStableTime > 0 &&
	    pPwrSeqTime->rtcStableTime > 0 &&
	    pPwrSeqTime->offStableTime > DFT_OFF_STABLE_TIME &&
	    pPwrSeqTime->onStableTime > DFT_ON_STABLE_TIME &&
	    pPwrSeqTime->rstStableTime > DFT_RST_STABLE_TIME) {
		/*memcpy may be more performance */
		WMT_DBG_FUNC("setting hw init sequence parameters\n");
		osal_memcpy(&gPwrSeqTime, pPwrSeqTime, osal_sizeof(gPwrSeqTime));
	} else {
		WMT_WARN_FUNC
		    ("invalid pPwrSeqTime parameter, use default hw init sequence parameters\n");
		gPwrSeqTime.ldoStableTime = DFT_LDO_STABLE_TIME;
		gPwrSeqTime.offStableTime = DFT_OFF_STABLE_TIME;
		gPwrSeqTime.onStableTime = DFT_ON_STABLE_TIME;
		gPwrSeqTime.rstStableTime = DFT_RST_STABLE_TIME;
		gPwrSeqTime.rtcStableTime = DFT_RTC_STABLE_TIME;
	}
	mtk_wcn_cmb_hw_dmp_seq();
	return 0;
}

INT32 mtk_wcn_cmb_hw_deinit(VOID)
{

	WMT_WARN_FUNC("mtk_wcn_cmb_hw_deinit start, set to default hw init sequence parameters\n");
	gPwrSeqTime.ldoStableTime = DFT_LDO_STABLE_TIME;
	gPwrSeqTime.offStableTime = DFT_OFF_STABLE_TIME;
	gPwrSeqTime.onStableTime = DFT_ON_STABLE_TIME;
	gPwrSeqTime.rstStableTime = DFT_RST_STABLE_TIME;
	gPwrSeqTime.rtcStableTime = DFT_RTC_STABLE_TIME;
	WMT_WARN_FUNC("mtk_wcn_cmb_hw_deinit finish\n");
	return 0;
}
