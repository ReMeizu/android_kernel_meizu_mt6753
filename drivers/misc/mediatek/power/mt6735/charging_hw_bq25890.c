/*
 * charging_hw_bq25890.c — TI BQ25890 switch charger HW layer, mt6735/mt6753
 * platform (MT6328 PMIC).
 *
 * Ported for Meizu M5s (M1612) bring-up from the MediaTek mt6755 BSP
 * implementation (kernel-3.18 mt6750-P collection,
 * drivers/misc/mediatek/power/mt6755/charging_hw_bq25890.c).
 *
 * Differences vs the mt6755 original:
 *  - command table follows the mt6735 CHARGING_CTRL_CMD enum (32 commands,
 *    see drivers/misc/mediatek/include/mt-plat/charging.h in this tree);
 *  - PMIC accesses use the MT6328 flag names (PMIC_RGS_CHRDET, ...) as in
 *    charging_hw_bq24296.c / charging_hw_pmic.c here, not MT6351/MT6353;
 *  - no MTK BIF (battery interface) support on this platform: BIF commands
 *    return STATUS_UNSUPPORTED;
 *  - no dual-input (DISO) support: m5s has no DC jack;
 *  - CHARGING_CMD_SET_VINDPM takes a raw REG0D[6:0] code (0x13 = 4.5V),
 *    matching switch_charging.c in this tree ("vindpm * 100 + 2600").
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <mt-plat/charging.h>
#include <mt-plat/upmu_common.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <mt-plat/mt_boot.h>
#include <mt-plat/battery_common.h>
#include <mach/mt_charging.h>
#include <mach/mt_pmic.h>
#include "bq25890.h"

/* ============================================================ // */
/* Define */
/* ============================================================ // */
#define STATUS_OK	0
#define STATUS_FAIL	1
#define STATUS_UNSUPPORTED	-1
#define GETARRAYNUM(array) (sizeof(array)/sizeof(array[0]))

/* ============================================================ // */
/* Global variable */
/* ============================================================ // */

/* BQ25890 REG06 VREG[5:0]: 3.840V + 16mV/step */
const unsigned int VBAT_CV_VTH[] = {
	3840000, 3856000, 3872000, 3888000,
	3904000, 3920000, 3936000, 3952000,
	3968000, 3984000, 4000000, 4016000,
	4032000, 4048000, 4064000, 4080000,
	4096000, 4112000, 4128000, 4144000,
	4160000, 4176000, 4192000, 4208000,
	4224000, 4240000, 4256000, 4272000,
	4288000, 4304000, 4320000, 4336000,
	4352000, 4368000, 4384000, 4400000,
	4416000, 4432000, 4448000, 4464000,
	4480000, 4496000, 4512000, 4528000,
	4544000, 4560000, 4576000, 4592000,
	4608000
};

/* BQ25890 REG04 ICHG[6:0]: 64mA/step, unit here 10uA */
const unsigned int CS_VTH[] = {
	0, 6400, 12800, 19200,
	25600, 32000, 38400, 44800,
	51200, 57600, 64000, 70400,
	76800, 83200, 89600, 96000,
	102400, 108800, 115200, 121600,
	128000, 134400, 140800, 147200,
	153600, 160000, 166400, 172800,
	179200, 185600, 192000, 198400,
	204800, 211200, 217600, 224000,
	230400, 236800, 243200, 249600,
	256000, 262400, 268800, 275200,
	281600, 288000, 294400, 300800,
	307200, 313600, 320000, 326400,
	332800, 339200, 345600, 352000,
	358400, 364800, 371200, 377600,
	384000, 390400, 396800, 403200,
	409600, 416000, 422400, 428800,
	435200, 441600, 448000, 454400,
	460800, 467200, 473600, 480000,
	486400, 492800, 499200, 505600
};

/* BQ25890 REG00 IINLIM[5:0]: 100mA + 50mA/step, unit here 10uA */
const unsigned int INPUT_CS_VTH[] = {
	10000, 15000, 20000, 25000,
	30000, 35000, 40000, 45000,
	50000, 55000, 60000, 65000,
	70000, 75000, 80000, 85000,
	90000, 95000, 100000, 105000,
	110000, 115000, 120000, 125000,
	130000, 135000, 140000, 145000,
	150000, 155000, 160000, 165000,
	170000, 175000, 180000, 185000,
	190000, 195000, 200000, 200500,
	210000, 215000, 220000, 225000,
	230000, 235000, 240000, 245000,
	250000, 255000, 260000, 265000,
	270000, 275000, 280000, 285000,
	290000, 295000, 300000, 305000,
	310000, 315000, 320000, 325000
};

const unsigned int VCDT_HV_VTH[] = {
	BATTERY_VOLT_04_200000_V, BATTERY_VOLT_04_250000_V, BATTERY_VOLT_04_300000_V,
	    BATTERY_VOLT_04_350000_V,
	BATTERY_VOLT_04_400000_V, BATTERY_VOLT_04_450000_V, BATTERY_VOLT_04_500000_V,
	    BATTERY_VOLT_04_550000_V,
	BATTERY_VOLT_04_600000_V, BATTERY_VOLT_06_000000_V, BATTERY_VOLT_06_500000_V,
	    BATTERY_VOLT_07_000000_V,
	BATTERY_VOLT_07_500000_V, BATTERY_VOLT_08_500000_V, BATTERY_VOLT_09_500000_V,
	    BATTERY_VOLT_10_500000_V
};

/* ============================================================ // */
/* function prototype */
/* ============================================================ // */
static unsigned int charging_error;
static unsigned int charging_get_error_state(void);
static unsigned int charging_set_error_state(void *data);
static unsigned int charging_set_vindpm(void *data);
static unsigned int charging_set_hiz_swchr(void *data);
static unsigned int g_input_current;
static DEFINE_MUTEX(g_input_current_mutex);

/* ============================================================ // */
unsigned int charging_value_to_parameter(const unsigned int *parameter, const unsigned int array_size,
				       const unsigned int val)
{
	unsigned int temp_param;

	if (val < array_size) {
		temp_param = parameter[val];
	} else {
		battery_log(BAT_LOG_CRTI, "Can't find the parameter \r\n");
		temp_param = parameter[0];
	}

	return temp_param;
}

unsigned int charging_parameter_to_value(const unsigned int *parameter, const unsigned int array_size,
				       const unsigned int val)
{
	unsigned int i;

	battery_log(BAT_LOG_FULL, "array_size = %d \r\n", array_size);

	for (i = 0; i < array_size; i++) {
		if (val == *(parameter + i))
			return i;
	}

	battery_log(BAT_LOG_CRTI, "NO register value match. val=%d\r\n", val);

	return 0;
}

static unsigned int bmt_find_closest_level(const unsigned int *pList, unsigned int number,
					 unsigned int level)
{
	unsigned int i, temp_param;
	unsigned int max_value_in_last_element;

	if (pList[0] < pList[1])
		max_value_in_last_element = KAL_TRUE;
	else
		max_value_in_last_element = KAL_FALSE;

	if (max_value_in_last_element == KAL_TRUE) {
		/* max value in the last element */
		for (i = (number - 1); i != 0; i--)	{
			if (pList[i] <= level)
				return pList[i];
		}

		battery_log(BAT_LOG_CRTI, "Can't find closest level, small value first \r\n");
		temp_param = pList[0];
	} else {
		/* max value in the first element */
		for (i = 0; i < number; i++) {
			if (pList[i] <= level)
				return pList[i];
		}

		battery_log(BAT_LOG_CRTI, "Can't find closest level, large value first \r\n");
		temp_param = pList[number - 1];
	}
	return temp_param;
}

static unsigned int charging_hw_init(void *data)
{
	unsigned int status = STATUS_OK;

	if (chargin_hw_init_done && !is_bq25890_exist()) {
		battery_log(BAT_LOG_CRTI,
			    "charging_hw_init: bq25890 absent, skip charger I2C init\n");
		return status;
	}

	/* VINDPM threshold mode: 1 = absolute (REG0D[7]) */
	bq25890_config_interface(bq25890_COND, 0x1, 0x1, 7);

	return status;
}

static unsigned int charging_sw_init(void *data)
{
	unsigned int status = STATUS_OK;

	if (chargin_hw_init_done && !is_bq25890_exist())
		return status;

	/* Register defaults, mirrored from the mt6755 BSP charging_sw_init()
	 * (same code generation as the stock M1612 kernel, FACT: identical
	 * battery_log format strings in stock_kernel.bin). */
	bq25890_config_interface(bq25890_CON0, 0x01, 0x01, 6);	/* enable ilimit Pin */
	bq25890_config_interface(bq25890_CON1, 0x6, 0xF, 0);	/* Vindpm offset 600mV */
	bq25890_config_interface(bq25890_COND, 0x1, 0x1, 7);	/* vindpm vth absolute */

	/*CC mode */
	bq25890_config_interface(bq25890_CON4, 0x08, 0x7F, 0);	/* ICHG 512mA */
	/*Vbus current limit */
	bq25890_config_interface(bq25890_CON0, 0x3F, 0x3F, 0);	/* IINLIM 3.25A */

	/* absolute VINDPM = 2.6 + code x 0.1 = 4.5V */
	bq25890_config_interface(bq25890_COND, 0x13, 0x7F, 0);

	/*CV mode */
	bq25890_config_interface(bq25890_CON6, 0x20, 0x3F, 2);	/* VREG=CV 4.352V */

	bq25890_config_interface(bq25890_CON2, 0x1, 0x1, 4);	/* enable ico Algorithm */
	bq25890_config_interface(bq25890_CON2, 0x0, 0x1, 3);	/* disable HV DCP */
	bq25890_config_interface(bq25890_CON2, 0x0, 0x1, 2);	/* disable MaxCharge */
	bq25890_config_interface(bq25890_CON2, 0x0, 0x1, 1);	/* disable DPDM detection */

	bq25890_config_interface(bq25890_CON7, 0x1, 0x3, 4);	/* watchdog 40s */
	bq25890_config_interface(bq25890_CON7, 0x1, 0x1, 3);	/* enable charge safety timer */
	bq25890_config_interface(bq25890_CON7, 0x2, 0x3, 1);	/* charge timer 12h */

	bq25890_config_interface(bq25890_CON2, 0x0, 0x1, 5);	/* boost freq 1.5MHz */
	bq25890_config_interface(bq25890_CONA, 0x7, 0xF, 4);	/* boost voltage 4.998V */
	bq25890_config_interface(bq25890_CONA, 0x3, 0x7, 0);	/* boost current limit 1.3A */

	bq25890_config_interface(bq25890_CON8, 0x4, 0x7, 5);	/* ir_comp resistance */
	bq25890_config_interface(bq25890_CON8, 0x6, 0x7, 2);	/* ir_comp vclamp */
	bq25890_config_interface(bq25890_CON8, 0x3, 0x3, 0);	/* thermal reg. 120C */

	bq25890_config_interface(bq25890_CON9, 0x0, 0x1, 4);	/* JEITA_VSET: VREG-200mV */
	bq25890_config_interface(bq25890_CON7, 0x1, 0x1, 0);	/* JEITA_ISET: 20% x ICHG */

	bq25890_config_interface(bq25890_CON3, 0x5, 0x7, 1);	/* SYS_MIN 3.5V */

	/*PreCC mode */
	bq25890_config_interface(bq25890_CON5, 0x1, 0xF, 4);	/* precharge current 128mA */
	bq25890_config_interface(bq25890_CON6, 0x1, 0x1, 1);	/* BATLOWV 3.0V */
	/*CV mode */
	bq25890_config_interface(bq25890_CON6, 0x0, 0x1, 0);	/* VRECHG = CV-100mV */
	bq25890_config_interface(bq25890_CON7, 0x1, 0x1, 7);	/* term. detect */
	bq25890_config_interface(bq25890_CON5, 0x1, 0x7, 0);	/* termination current 128mA */

	return status;
}

static unsigned int charging_dump_register(void *data)
{
	if (chargin_hw_init_done && !is_bq25890_exist())
		return STATUS_OK;

	battery_log(BAT_LOG_FULL, "charging_dump_register\r\n");

	bq25890_dump_register();

	return STATUS_OK;
}

static unsigned int charging_enable(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int enable = *(unsigned int *) (data);
	unsigned int bootmode = 0;

	if (chargin_hw_init_done && !is_bq25890_exist())
		return status;

	if (KAL_TRUE == enable) {
		bq25890_set_en_hiz(0x0);
		bq25890_chg_en(enable);
	} else {
		bq25890_chg_en(enable);
		if (charging_get_error_state())
			battery_log(BAT_LOG_CRTI,
				    "[charging_enable] under test mode: disable charging\n");

		bootmode = get_boot_mode();
		if ((bootmode == META_BOOT) || (bootmode == ADVMETA_BOOT))
			bq25890_set_en_hiz(0x1);
	}

	return status;
}

static unsigned int charging_set_cv_voltage(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned short int array_size;
	unsigned int set_cv_voltage;
	unsigned short int register_value;

	array_size = GETARRAYNUM(VBAT_CV_VTH);
	set_cv_voltage = bmt_find_closest_level(VBAT_CV_VTH, array_size, *(unsigned int *) data);
	register_value =
	    charging_parameter_to_value(VBAT_CV_VTH, GETARRAYNUM(VBAT_CV_VTH), set_cv_voltage);
	battery_log(BAT_LOG_CRTI, "charging_set_cv_voltage register_value=0x%x %d %d\n",
		    register_value, *(unsigned int *) data, set_cv_voltage);
	bq25890_set_vreg(register_value);

	return status;
}

static unsigned int charging_get_current(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int array_size;
	unsigned int val;

	/*Get current level */
	array_size = GETARRAYNUM(CS_VTH);
	val = bq25890_get_reg_ichg();
	*(unsigned int *)data = charging_value_to_parameter(CS_VTH, array_size, val);

	return status;
}

static unsigned int charging_set_current(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int set_chr_current;
	unsigned int array_size;
	unsigned int register_value;
	unsigned int current_value = *(unsigned int *) data;

	array_size = GETARRAYNUM(CS_VTH);
	set_chr_current = bmt_find_closest_level(CS_VTH, array_size, current_value);
	register_value = charging_parameter_to_value(CS_VTH, array_size, set_chr_current);
	battery_log(BAT_LOG_FULL, "charging_set_ICHG:%d\n", register_value);

	bq25890_set_ichg(register_value);

	return status;
}

static unsigned int charging_set_input_current(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int current_value = *(unsigned int *) data;
	unsigned int set_chr_current;
	unsigned int array_size;
	unsigned int register_value;

	mutex_lock(&g_input_current_mutex);
	array_size = GETARRAYNUM(INPUT_CS_VTH);
	set_chr_current = bmt_find_closest_level(INPUT_CS_VTH, array_size, current_value);
	g_input_current = set_chr_current;
	register_value = charging_parameter_to_value(INPUT_CS_VTH, array_size, set_chr_current);
	battery_log(BAT_LOG_FULL, "charging_set_input_current:%d\n", register_value);
	bq25890_set_iinlim(register_value);
	mutex_unlock(&g_input_current_mutex);

	return status;
}

static unsigned int charging_get_charging_status(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned char reg_value;

	bq25890_read_interface(bq25890_CONB, &reg_value, 0x3, 3);	/* CHRG_STAT */

	if (reg_value == 0x3)	/* charge done */
		*(unsigned int *) data = KAL_TRUE;
	else
		*(unsigned int *) data = KAL_FALSE;

	return status;
}

static unsigned int charging_reset_watch_dog_timer(void *data)
{
	unsigned int status = STATUS_OK;

	if (chargin_hw_init_done && !is_bq25890_exist())
		return status;

	battery_log(BAT_LOG_FULL, "charging_reset_watch_dog_timer\r\n");

	bq25890_config_interface(bq25890_CON3, 0x1, 0x1, 6);	/* WD_RST */

	return status;
}

static unsigned int charging_set_hv_threshold(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int set_hv_voltage;
	unsigned int array_size;
	unsigned short register_value;
	unsigned int voltage = *(unsigned int *) (data);

	array_size = GETARRAYNUM(VCDT_HV_VTH);
	set_hv_voltage = bmt_find_closest_level(VCDT_HV_VTH, array_size, voltage);
	register_value = charging_parameter_to_value(VCDT_HV_VTH, array_size, set_hv_voltage);
	pmic_set_register_value(PMIC_RG_VCDT_HV_VTH, register_value);

	return status;
}

static unsigned int charging_get_hv_status(void *data)
{
	unsigned int status = STATUS_OK;

#if defined(CONFIG_POWER_EXT) || defined(CONFIG_MTK_FPGA)
	*(kal_bool *) (data) = 0;
	pr_notice("[charging_get_hv_status] charger ok for bring up.\n");
#else
	*(kal_bool *) (data) = pmic_get_register_value(PMIC_RGS_VCDT_HV_DET);
#endif

	return status;
}

static unsigned int charging_get_battery_status(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int val = 0;

#if defined(CONFIG_POWER_EXT) || defined(CONFIG_MTK_FPGA)
	*(kal_bool *) (data) = 0;	/* battery exist */
	battery_log(BAT_LOG_CRTI, "[charging_get_battery_status] battery exist for bring up.\n");
#else
	val = pmic_get_register_value(PMIC_BATON_TDET_EN);
	battery_log(BAT_LOG_FULL, "[charging_get_battery_status] BATON_TDET_EN = %d\n", val);
	if (val) {
		pmic_set_register_value(PMIC_BATON_TDET_EN, 1);
		pmic_set_register_value(PMIC_RG_BATON_EN, 1);
		*(kal_bool *) (data) = pmic_get_register_value(PMIC_RGS_BATON_UNDET);
	} else {
		*(kal_bool *) (data) = KAL_FALSE;
	}
#endif

	return status;
}

static unsigned int charging_get_charger_det_status(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int val = 0;

#if defined(CONFIG_MTK_FPGA)
	val = 1;
	battery_log(BAT_LOG_CRTI, "[charging_get_charger_det_status] chr exist for fpga.\n");
#else
	val = pmic_get_register_value(PMIC_RGS_CHRDET);
#endif

	*(kal_bool *) (data) = val;

	return status;
}

static unsigned int charging_get_charger_type(void *data)
{
	unsigned int status = STATUS_OK;

#if defined(CONFIG_POWER_EXT) || defined(CONFIG_MTK_FPGA)
	*(CHARGER_TYPE *) (data) = STANDARD_HOST;
#else
	*(CHARGER_TYPE *) (data) = hw_charging_get_charger_type();
#endif

	return status;
}

static unsigned int charging_get_is_pcm_timer_trigger(void *data)
{
	unsigned int status = STATUS_OK;
	/* as charging_hw_pmic.c / charging_hw_bq24296.c in this tree:
	 * slp_get_wake_reason() is not wired up here */
	return status;
}

static unsigned int charging_set_platform_reset(void *data)
{
	unsigned int status = STATUS_OK;

#if defined(CONFIG_POWER_EXT) || defined(CONFIG_MTK_FPGA)
#else
	battery_log(BAT_LOG_CRTI, "charging_set_platform_reset\n");

	kernel_restart("battery service reboot system");
#endif
	return status;
}

static unsigned int charging_get_platform_boot_mode(void *data)
{
	unsigned int status = STATUS_OK;
#if defined(CONFIG_POWER_EXT) || defined(CONFIG_MTK_FPGA)
#else
	*(unsigned int *) (data) = get_boot_mode();

	battery_log(BAT_LOG_CRTI, "get_boot_mode=%d\n", get_boot_mode());
#endif
	return status;
}

static unsigned int charging_set_power_off(void *data)
{
	unsigned int status = STATUS_OK;

#if defined(CONFIG_POWER_EXT) || defined(CONFIG_MTK_FPGA)
#else
	battery_log(BAT_LOG_CRTI, "charging_set_power_off\n");
	kernel_power_off();
#endif

	return status;
}

static unsigned int charging_get_power_source(void *data)
{
	unsigned int status = STATUS_OK;

	*(kal_bool *) data = KAL_FALSE;

	return status;
}

static unsigned int charging_get_csdac_full_flag(void *data)
{
	return STATUS_UNSUPPORTED;
}

static unsigned int charging_set_ta_current_pattern(void *data)
{
	unsigned int increase = *(unsigned int *) (data);

	/* BQ25890 has native PumpExpress support (REG09 PUMPX_UP/DN) */
	if (increase == KAL_TRUE) {
		bq25890_pumpx_up(1);
		battery_log(BAT_LOG_FULL, "Pumping up adaptor...\n");
	} else {
		bq25890_pumpx_up(0);
		battery_log(BAT_LOG_FULL, "Pumping down adaptor...\n");
	}

	return STATUS_OK;
}

static unsigned int charging_set_error_state(void *data)
{
	unsigned int status = STATUS_OK;

	charging_error = *(unsigned int *) (data);
	charging_set_hiz_swchr(&charging_error);

	return status;
}

static unsigned int charging_diso_init(void *data)
{
	/* no dual-input (DC jack) hardware on m5s */
	return STATUS_OK;
}

static unsigned int charging_get_diso_state(void *data)
{
	return STATUS_OK;
}

static unsigned int charging_get_error_state(void)
{
	return charging_error;
}

/* data = raw REG0D[6:0] code: Vth = 2.6V + code * 100mV (absolute mode).
 * switch_charging.c passes SWITCH_CHR_VINDPM_5V (0x13) etc. */
static unsigned int charging_set_vindpm(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int v = *(unsigned int *) data;

	battery_log(BAT_LOG_CRTI, "charging_set_vindpm:%d 0x%x\n", v, v);
	bq25890_set_vindpm(v);

	return status;
}

static unsigned int charging_set_vbus_ovp_en(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int e = *(unsigned int *) data;

	pmic_set_register_value(PMIC_RG_VCDT_HV_EN, e);

	return status;
}

static unsigned int charging_get_bif_vbat(void *data)
{
	/* no MTK BIF support on mt6735 platform */
	return STATUS_UNSUPPORTED;
}

static unsigned int charging_set_chrind_ck_pdn(void *data)
{
	/* MT6328 has no RG_DRV_CHRIND_CK_PDN; only called for MT6351-based
	 * configs (see battery_common.c, CONFIG_MTK_BQ25896_SUPPORT block) */
	return STATUS_UNSUPPORTED;
}

static unsigned int charging_enable_safetytimer(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int en;

	en = *(unsigned int *) data;
	bq25890_en_chg_timer(en);

	return status;
}

static unsigned int charging_set_hiz_swchr(void *data)
{
	unsigned int status = STATUS_OK;
	unsigned int en;
	unsigned int vindpm;

	en = *(unsigned int *) data;
	if (en == 1)
		vindpm = 0x7F;	/* VINDPM max => input power path off */
	else
		vindpm = 0x13;	/* 4.5V */

	bq25890_set_vindpm(vindpm);

	return status;
}

static unsigned int charging_get_bif_tbat(void *data)
{
	return STATUS_UNSUPPORTED;
}

static unsigned int (*const charging_func[CHARGING_CMD_NUMBER]) (void *data) = {
	charging_hw_init,		/* CHARGING_CMD_INIT */
	charging_dump_register,		/* CHARGING_CMD_DUMP_REGISTER */
	charging_enable,		/* CHARGING_CMD_ENABLE */
	charging_set_cv_voltage,	/* CHARGING_CMD_SET_CV_VOLTAGE */
	charging_get_current,		/* CHARGING_CMD_GET_CURRENT */
	charging_set_current,		/* CHARGING_CMD_SET_CURRENT */
	charging_set_input_current,	/* CHARGING_CMD_SET_INPUT_CURRENT */
	charging_get_charging_status,	/* CHARGING_CMD_GET_CHARGING_STATUS */
	charging_reset_watch_dog_timer,	/* CHARGING_CMD_RESET_WATCH_DOG_TIMER */
	charging_set_hv_threshold,	/* CHARGING_CMD_SET_HV_THRESHOLD */
	charging_get_hv_status,		/* CHARGING_CMD_GET_HV_STATUS */
	charging_get_battery_status,	/* CHARGING_CMD_GET_BATTERY_STATUS */
	charging_get_charger_det_status, /* CHARGING_CMD_GET_CHARGER_DET_STATUS */
	charging_get_charger_type,	/* CHARGING_CMD_GET_CHARGER_TYPE */
	charging_get_is_pcm_timer_trigger, /* CHARGING_CMD_GET_IS_PCM_TIMER_TRIGGER */
	charging_set_platform_reset,	/* CHARGING_CMD_SET_PLATFORM_RESET */
	charging_get_platform_boot_mode, /* CHARGING_CMD_GET_PLATFORM_BOOT_MODE */
	charging_set_power_off,		/* CHARGING_CMD_SET_POWER_OFF */
	charging_get_power_source,	/* CHARGING_CMD_GET_POWER_SOURCE */
	charging_get_csdac_full_flag,	/* CHARGING_CMD_GET_CSDAC_FALL_FLAG */
	charging_set_ta_current_pattern, /* CHARGING_CMD_SET_TA_CURRENT_PATTERN */
	charging_set_error_state,	/* CHARGING_CMD_SET_ERROR_STATE */
	charging_diso_init,		/* CHARGING_CMD_DISO_INIT */
	charging_get_diso_state,	/* CHARGING_CMD_GET_DISO_STATE */
	charging_set_vindpm,		/* CHARGING_CMD_SET_VINDPM */
	charging_set_vbus_ovp_en,	/* CHARGING_CMD_SET_VBUS_OVP_EN */
	charging_get_bif_vbat,		/* CHARGING_CMD_GET_BIF_VBAT */
	charging_set_chrind_ck_pdn,	/* CHARGING_CMD_SET_CHRIND_CK_PDN */
	charging_sw_init,		/* CHARGING_CMD_SW_INIT */
	charging_enable_safetytimer,	/* CHARGING_CMD_ENABLE_SAFETY_TIMER */
	charging_set_hiz_swchr,		/* CHARGING_CMD_SET_HIZ_SWCHR */
	charging_get_bif_tbat		/* CHARGING_CMD_GET_BIF_TBAT */
};

/*
* FUNCTION
*		Internal_chr_control_handler
*
* DESCRIPTION
*		 This function is called to set the charger hw
*
* CALLS
*
* PARAMETERS
*		None
*
* RETURNS
*
*
* GLOBALS AFFECTED
*	   None
*/
signed int chr_control_interface(CHARGING_CTRL_CMD cmd, void *data)
{
	signed int status;

	if (cmd < CHARGING_CMD_NUMBER && charging_func[cmd] != NULL)
		status = charging_func[cmd] (data);
	else
		return STATUS_UNSUPPORTED;

	return status;
}
