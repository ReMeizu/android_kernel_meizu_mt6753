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
#define DFT_TAG         "[WMT-CORE]"

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "osal_typedef.h"

#include "wmt_lib.h"
#include "wmt_core.h"
#include "wmt_ctrl.h"
#include "wmt_ic.h"
#include "wmt_conf.h"

#include "wmt_func.h"
#include "stp_core.h"
#include "psm_core.h"


P_WMT_FUNC_OPS gpWmtFuncOps[4] = {
#if CFG_FUNC_BT_SUPPORT
	[0] = &wmt_func_bt_ops,
#else
	[0] = NULL,
#endif

#if CFG_FUNC_FM_SUPPORT
	[1] = &wmt_func_fm_ops,
#else
	[1] = NULL,
#endif

#if CFG_FUNC_GPS_SUPPORT
	[2] = &wmt_func_gps_ops,
#else
	[2] = NULL,
#endif

#if CFG_FUNC_WIFI_SUPPORT
	[3] = &wmt_func_wifi_ops,
#else
	[3] = NULL,
#endif

};

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/* TODO:[FixMe][GeorgeKuo]: is it an MT6620 only or general general setting?
*move to wmt_ic_6620 temporarily.
*/
/* BT Port 2 Feature. */
/* #define CFG_WMT_BT_PORT2 (1) */

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

static WMT_CTX gMtkWmtCtx;
static UINT8 gLpbkBuf[1024] = { 0 };

#if defined(CONFIG_MTK_COMBO_CHIP_CONSYS_6735)
static const UINT32 m2note_wmt_snapshot_regs[] = {
	0x80030000,
	0x80030004,
	0x80030010,
	0x80000000,
	0x80000004,
	0x80000008,
	0x80010000,
	0x00008a00,
	0x00008a04,
	0x00008a10,
	0x00008a18,
	0x80050050,
	0x80050074,
	0x80050078,
	0x80050110,
	0x80050130,
	0x80050140,
	0x80050150,
	0x80050300,
};

#define M2NOTE_WMT_STAGE_REG_COUNT 19
#define M2NOTE_WMT_STAGE_CACHE_MAX 32

struct m2note_wmt_stage_reg_cache {
	UINT32 reg;
	INT32 ret;
	UINT32 val;
};

struct m2note_wmt_stage_cache_entry {
	const char *stage;
	UINT32 seq;
	UINT32 slot;
	UINT32 reg_count;
	MTK_WCN_BOOL read_adie;
	INT32 adie_2400;
	INT32 adie_2404;
	struct m2note_wmt_stage_reg_cache regs[M2NOTE_WMT_STAGE_REG_COUNT];
};

#define M2NOTE_WMT_RFCAL_TRACE_BYTES 96

static UINT8 m2note_wmt_last_rfcal_evt[M2NOTE_WMT_RFCAL_TRACE_BYTES];
static UINT32 m2note_wmt_last_rfcal_len;
static UINT32 m2note_wmt_last_rfcal_seq;
static INT32 m2note_wmt_last_rfcal_idx = -1;
static INT32 m2note_wmt_last_rfcal_ret;
static UINT32 m2note_wmt_last_rfcal_got;
static UINT32 m2note_wmt_last_rfcal_declared_len;
static UINT8 m2note_wmt_last_rfcal_status = 0xff;
static UINT8 m2note_wmt_last_rfcal_subop = 0xff;
static UINT8 m2note_wmt_last_rfcal_exp_tail;
static UINT32 m2note_wmt_last_rfcal_sum;
static UINT32 m2note_wmt_last_rfcal_xor;
static INT32 m2note_wmt_last_rfcal_3780_le = -1;
static INT32 m2note_wmt_last_rfcal_3780_be = -1;
static MTK_WCN_BOOL m2note_wmt_last_rfcal_valid = MTK_WCN_BOOL_FALSE;

#define M2NOTE_WMT_RFCAL_EVT_BUF_SIZE 768

#define M2NOTE_WMT_INIT_SCRIPT_CACHE_MAX 64
#define M2NOTE_WMT_INIT_SCRIPT_CACHE_BYTES 8
#define M2NOTE_WMT_PATCH_CACHE_MAX 4
#define M2NOTE_WMT_PATCH_NAME_BYTES 64
#define M2NOTE_WMT_PATCH_BUILD_BYTES 16
#define M2NOTE_WMT_PATCH_PLATFORM_BYTES 5

struct m2note_wmt_init_script_cache_entry {
	const char *name;
	const char *status;
	INT32 idx;
	INT32 ret;
	UINT32 got;
	UINT32 cmd_sz;
	UINT32 evt_sz;
	UINT8 cmd[M2NOTE_WMT_INIT_SCRIPT_CACHE_BYTES];
	UINT8 evt[M2NOTE_WMT_INIT_SCRIPT_CACHE_BYTES];
	UINT8 exp[M2NOTE_WMT_INIT_SCRIPT_CACHE_BYTES];
};

struct m2note_wmt_patch_cache_entry {
	UINT32 index;
	INT32 ret;
	UINT32 raw_size;
	UINT32 body_size;
	UINT32 frag_num;
	UINT32 frag_seq;
	UINT16 last_frag_size;
	UINT16 hw_ver;
	UINT16 sw_ver;
	UINT16 patch_ver;
	UINT8 address[4];
	UINT8 name[M2NOTE_WMT_PATCH_NAME_BYTES];
	UINT8 build[M2NOTE_WMT_PATCH_BUILD_BYTES];
	UINT8 platform[M2NOTE_WMT_PATCH_PLATFORM_BYTES];
};

static struct m2note_wmt_init_script_cache_entry m2note_wmt_init_script_cache[M2NOTE_WMT_INIT_SCRIPT_CACHE_MAX];
static UINT32 m2note_wmt_init_script_cache_seq;
static UINT32 m2note_wmt_init_script_cache_dump_seq;
static UINT32 m2note_wmt_init_script_cache_count;
static UINT32 m2note_wmt_init_script_cache_dropped;
static UINT32 m2note_wmt_init_script_cache_hif;
static UINT32 m2note_wmt_init_script_cache_hw;

static struct m2note_wmt_stage_cache_entry m2note_wmt_stage_cache[M2NOTE_WMT_STAGE_CACHE_MAX];
static UINT32 m2note_wmt_stage_cache_seq;
static UINT32 m2note_wmt_stage_cache_dump_seq;
static UINT32 m2note_wmt_stage_cache_count;
static UINT32 m2note_wmt_stage_cache_dropped;

static struct m2note_wmt_patch_cache_entry m2note_wmt_patch_cache[M2NOTE_WMT_PATCH_CACHE_MAX];
static UINT32 m2note_wmt_patch_cache_seq;
static UINT32 m2note_wmt_patch_cache_dump_seq;
static UINT32 m2note_wmt_patch_cache_count;
static UINT32 m2note_wmt_patch_cache_dropped;

static MTK_WCN_BOOL m2note_wmt_last_coex_valid = MTK_WCN_BOOL_FALSE;
static UINT32 m2note_wmt_last_coex_seq;
static UINT32 m2note_wmt_last_coex_cfg_exist;
static UINT32 m2note_wmt_last_coex_ant;
static UINT32 m2note_wmt_last_coex_filter;
static UINT32 m2note_wmt_last_coex_co_clock;
static UINT32 m2note_wmt_last_coex_gps_lna_pin;
static UINT32 m2note_wmt_last_coex_gps_lna_enable;
static UINT32 m2note_wmt_last_coex_sdio;
static UINT32 m2note_wmt_last_coex_cmd5;
static INT32 m2note_wmt_last_coex_ret;

#endif

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

static INT32 opfunc_hif_conf(P_WMT_OP pWmtOp);
static INT32 opfunc_pwr_on(P_WMT_OP pWmtOp);
static INT32 opfunc_pwr_off(P_WMT_OP pWmtOp);
static INT32 opfunc_func_on(P_WMT_OP pWmtOp);
static INT32 opfunc_func_off(P_WMT_OP pWmtOp);
static INT32 opfunc_reg_rw(P_WMT_OP pWmtOp);
static INT32 opfunc_exit(P_WMT_OP pWmtOp);
static INT32 opfunc_pwr_sv(P_WMT_OP pWmtOp);
static INT32 opfunc_dsns(P_WMT_OP pWmtOp);
static INT32 opfunc_lpbk(P_WMT_OP pWmtOp);
static INT32 opfunc_cmd_test(P_WMT_OP pWmtOp);
static INT32 opfunc_hw_rst(P_WMT_OP pWmtOp);
static INT32 opfunc_sw_rst(P_WMT_OP pWmtOp);
static INT32 opfunc_stp_rst(P_WMT_OP pWmtOp);
static INT32 opfunc_therm_ctrl(P_WMT_OP pWmtOp);
static INT32 opfunc_efuse_rw(P_WMT_OP pWmtOp);
static INT32 opfunc_therm_ctrl(P_WMT_OP pWmtOp);
static INT32 opfunc_gpio_ctrl(P_WMT_OP pWmtOp);
static INT32 opfunc_pin_state(P_WMT_OP pWmtOp);
static INT32 opfunc_bgw_ds(P_WMT_OP pWmtOp);
static INT32 opfunc_set_mcu_clk(P_WMT_OP pWmtOp);
static INT32 opfunc_adie_lpbk_test(P_WMT_OP pWmtOp);
INT32 mtk_wcn_m2note_adie_reg_read(UINT32 reg);
INT32 mtk_wcn_m2note_reg08_read_trace(const char *stage, UINT32 offset,
				      PUINT32 pVal);
#if CFG_WMT_LTE_COEX_HANDLING
static INT32 opfunc_idc_msg_handling(P_WMT_OP pWmtOp);
#endif
static VOID wmt_core_dump_func_state(PINT8 pSource);
static INT32 wmt_core_stp_init(VOID);
static INT32 wmt_core_stp_deinit(VOID);
static INT32 wmt_core_hw_check(VOID);

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

static const UINT8 WMT_SLEEP_CMD[] = { 0x01, 0x03, 0x01, 0x00, 0x01 };
static const UINT8 WMT_SLEEP_EVT[] = { 0x02, 0x03, 0x02, 0x00, 0x00, 0x01 };

static const UINT8 WMT_HOST_AWAKE_CMD[] = { 0x01, 0x03, 0x01, 0x00, 0x02 };
static const UINT8 WMT_HOST_AWAKE_EVT[] = { 0x02, 0x03, 0x02, 0x00, 0x00, 0x02 };

static const UINT8 WMT_WAKEUP_CMD[] = { 0xFF };
static const UINT8 WMT_WAKEUP_EVT[] = { 0x02, 0x03, 0x02, 0x00, 0x00, 0x03 };

static UINT8 WMT_THERM_CMD[] = { 0x01, 0x11, 0x01, 0x00,
	0x00			/*thermal sensor operation */
};
static UINT8 WMT_THERM_CTRL_EVT[] = { 0x02, 0x11, 0x01, 0x00, 0x00 };
static UINT8 WMT_THERM_READ_EVT[] = { 0x02, 0x11, 0x02, 0x00, 0x00, 0x00 };

static UINT8 WMT_EFUSE_CMD[] = { 0x01, 0x0D, 0x08, 0x00,
	0x01,			/*[4]operation, 0:init, 1:write 2:read */
	0x01,			/*[5]Number of register setting */
	0xAA, 0xAA,		/*[6-7]Address */
	0xBB, 0xBB, 0xBB, 0xBB	/*[8-11] Value */
};

static UINT8 WMT_EFUSE_EVT[] = { 0x02, 0x0D, 0x08, 0x00,
	0xAA,			/*[4]operation, 0:init, 1:write 2:read */
	0xBB,			/*[5]Number of register setting */
	0xCC, 0xCC,		/*[6-7]Address */
	0xDD, 0xDD, 0xDD, 0xDD	/*[8-11] Value */
};

static UINT8 WMT_DSNS_CMD[] = { 0x01, 0x0E, 0x02, 0x00, 0x01,
	0x00			/*desnse type */
};
static UINT8 WMT_DSNS_EVT[] = { 0x02, 0x0E, 0x01, 0x00, 0x00 };

/* TODO:[NewFeature][GeorgeKuo] Update register group in ONE CMD/EVT */
static UINT8 WMT_SET_REG_CMD[] = { 0x01, 0x08, 0x10, 0x00	/*length */
	    , 0x00		/*op: w(1) & r(2) */
	    , 0x01		/*type: reg */
	    , 0x00		/*res */
	    , 0x01		/*1 register */
	    , 0x00, 0x00, 0x00, 0x00	/* addr */
	    , 0x00, 0x00, 0x00, 0x00	/* value */
	    , 0xFF, 0xFF, 0xFF, 0xFF	/*mask */
};

static UINT8 WMT_SET_REG_WR_EVT[] = { 0x02, 0x08, 0x04, 0x00	/*length */
	    , 0x00		/*S: 0 */
	    , 0x00		/*type: reg */
	    , 0x00		/*rev */
	    , 0x01		/*1 register */
	    /* , 0x00, 0x00, 0x00, 0x00  */		/* addr */
	    /* , 0x00, 0x00, 0x00, 0x00  */		/* value */
};

static UINT8 WMT_SET_REG_RD_EVT[] = { 0x02, 0x08, 0x04, 0x00	/*length */
	    , 0x00		/*S: 0 */
	    , 0x00		/*type: reg */
	    , 0x00		/*rev */
	    , 0x01		/*1 register */
	    , 0x00, 0x00, 0x00, 0x00	/* addr */
	    , 0x00, 0x00, 0x00, 0x00	/* value */
};

/* GeorgeKuo: Use designated initializers described in
 * http://gcc.gnu.org/onlinedocs/gcc-4.0.4/gcc/Designated-Inits.html
 */

static const WMT_OPID_FUNC wmt_core_opfunc[] = {
	[WMT_OPID_HIF_CONF] = opfunc_hif_conf,
	[WMT_OPID_PWR_ON] = opfunc_pwr_on,
	[WMT_OPID_PWR_OFF] = opfunc_pwr_off,
	[WMT_OPID_FUNC_ON] = opfunc_func_on,
	[WMT_OPID_FUNC_OFF] = opfunc_func_off,
	[WMT_OPID_REG_RW] = opfunc_reg_rw,	/* TODO:[ChangeFeature][George] is this OP obsoleted? */
	[WMT_OPID_EXIT] = opfunc_exit,
	[WMT_OPID_PWR_SV] = opfunc_pwr_sv,
	[WMT_OPID_DSNS] = opfunc_dsns,
	[WMT_OPID_LPBK] = opfunc_lpbk,
	[WMT_OPID_CMD_TEST] = opfunc_cmd_test,
	[WMT_OPID_HW_RST] = opfunc_hw_rst,
	[WMT_OPID_SW_RST] = opfunc_sw_rst,
	[WMT_OPID_STP_RST] = opfunc_stp_rst,
	[WMT_OPID_THERM_CTRL] = opfunc_therm_ctrl,
	[WMT_OPID_EFUSE_RW] = opfunc_efuse_rw,
	[WMT_OPID_GPIO_CTRL] = opfunc_gpio_ctrl,
	[WMT_OPID_GPIO_STATE] = opfunc_pin_state,
	[WMT_OPID_BGW_DS] = opfunc_bgw_ds,
	[WMT_OPID_SET_MCU_CLK] = opfunc_set_mcu_clk,
	[WMT_OPID_ADIE_LPBK_TEST] = opfunc_adie_lpbk_test,
#if CFG_WMT_LTE_COEX_HANDLING
	[WMT_OPID_IDC_MSG_HANDLING] = opfunc_idc_msg_handling,
#endif
};

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
INT32 wmt_core_init(VOID)
{
	INT32 i = 0;

	osal_memset(&gMtkWmtCtx, 0, osal_sizeof(gMtkWmtCtx));
	/* gMtkWmtCtx.p_ops is cleared to NULL */

	/* default FUNC_OFF state */
	for (i = 0; i < WMTDRV_TYPE_MAX; ++i) {
		/* WinMo is default to DRV_STS_UNREG; */
		gMtkWmtCtx.eDrvStatus[i] = DRV_STS_POWER_OFF;
	}

	return 0;
}

INT32 wmt_core_deinit(VOID)
{
	/* return to init state */
	osal_memset(&gMtkWmtCtx, 0, osal_sizeof(gMtkWmtCtx));
	/* gMtkWmtCtx.p_ops is cleared to NULL */
	return 0;
}

/* TODO: [ChangeFeature][George] Is wmt_ctrl a good interface? maybe not...... */
/* parameters shall be copied in/from ctrl buffer, which is also a size-wasting buffer. */
INT32 wmt_core_tx(const PUINT8 pData, const UINT32 size, PUINT32 writtenSize, const MTK_WCN_BOOL bRawFlag)
{
	INT32 iRet;
#if 0				/* Test using direct function call instead of wmt_ctrl() interface */
	WMT_CTRL_DATA ctrlData;

	ctrlData.ctrlId = WMT_CTRL_TX;
	ctrlData.au4CtrlData[0] = (UINT32) pData;
	ctrlData.au4CtrlData[1] = size;
	ctrlData.au4CtrlData[2] = (UINT32) writtenSize;
	ctrlData.au4CtrlData[3] = bRawFlag;

	iRet = wmt_ctrl(&ctrlData);
	if (iRet) {
		/* ERROR */
		WMT_ERR_FUNC("WMT-CORE: wmt_core_ctrl failed: WMT_CTRL_TX, iRet:%d\n", iRet);
		/* (*sys_dbg_assert)(0, __FILE__, __LINE__); */
		osal_assert(0);
	}
#endif
	iRet = wmt_ctrl_tx_ex(pData, size, writtenSize, bRawFlag);
	if (0 == *writtenSize) {
		INT32 retry_times = 0;
		INT32 max_retry_times = 3;
		INT32 retry_delay_ms = 360;

		WMT_WARN_FUNC("WMT-CORE: wmt_ctrl_tx_ex failed and written ret:%d, maybe no winspace in STP layer\n",
			      *writtenSize);
		while ((0 == *writtenSize) && (retry_times < max_retry_times)) {
			WMT_ERR_FUNC("WMT-CORE: retrying, wait for %d ms\n", retry_delay_ms);
			osal_sleep_ms(retry_delay_ms);

			iRet = wmt_ctrl_tx_ex(pData, size, writtenSize, bRawFlag);
			retry_times++;
		}
	}
	return iRet;
}

INT32 wmt_core_rx(PUINT8 pBuf, UINT32 bufLen, UINT32 *readSize)
{
	INT32 iRet;
	WMT_CTRL_DATA ctrlData;

	ctrlData.ctrlId = WMT_CTRL_RX;
	ctrlData.au4CtrlData[0] = (SIZE_T) pBuf;
	ctrlData.au4CtrlData[1] = bufLen;
	ctrlData.au4CtrlData[2] = (SIZE_T) readSize;

	iRet = wmt_ctrl(&ctrlData);
	if (iRet) {
		/* ERROR */
		WMT_ERR_FUNC("WMT-CORE: wmt_core_ctrl failed: WMT_CTRL_RX, iRet:%d\n", iRet);
		mtk_wcn_stp_dbg_dump_package();
		osal_assert(0);
	}
	return iRet;
}

INT32 wmt_core_rx_flush(UINT32 type)
{
	INT32 iRet;
	WMT_CTRL_DATA ctrlData;

	ctrlData.ctrlId = WMT_CTRL_RX_FLUSH;
	ctrlData.au4CtrlData[0] = (UINT32) type;

	iRet = wmt_ctrl(&ctrlData);
	if (iRet) {
		/* ERROR */
		WMT_ERR_FUNC("WMT-CORE: wmt_core_ctrl failed: WMT_CTRL_RX_FLUSH, iRet:%d\n", iRet);
		osal_assert(0);
	}
	return iRet;
}

INT32 wmt_core_func_ctrl_cmd(ENUM_WMTDRV_TYPE_T type, MTK_WCN_BOOL fgEn)
{
	INT32 iRet = 0;
	UINT32 u4WmtCmdPduLen;
	UINT32 u4WmtEventPduLen;
	UINT32 u4ReadSize;
	UINT32 u4WrittenSize;
	WMT_PKT rWmtPktCmd;
	WMT_PKT rWmtPktEvent;
	MTK_WCN_BOOL fgFail;

	/* TODO:[ChangeFeature][George] remove WMT_PKT. replace it with hardcoded arrays. */
	/* Using this struct relies on compiler's implementation and pack() settings */
	osal_memset(&rWmtPktCmd, 0, osal_sizeof(rWmtPktCmd));
	osal_memset(&rWmtPktEvent, 0, osal_sizeof(rWmtPktEvent));

	rWmtPktCmd.eType = (UINT8) PKT_TYPE_CMD;
	rWmtPktCmd.eOpCode = (UINT8) OPCODE_FUNC_CTRL;

	/* Flag field: driver type */
	rWmtPktCmd.aucParam[0] = (UINT8) type;
	/* Parameter field: ON/OFF */
	rWmtPktCmd.aucParam[1] = (fgEn == WMT_FUNC_CTRL_ON) ? 1 : 0;
	rWmtPktCmd.u2SduLen = WMT_FLAG_LEN + WMT_FUNC_CTRL_PARAM_LEN;	/* (2) */

	/* WMT Header + WMT SDU */
	u4WmtCmdPduLen = WMT_HDR_LEN + rWmtPktCmd.u2SduLen;	/* (6) */
	u4WmtEventPduLen = WMT_HDR_LEN + WMT_STS_LEN;	/* (5) */

	do {
		fgFail = MTK_WCN_BOOL_TRUE;
/* iRet = (*kal_stp_tx)((PUINT8)&rWmtPktCmd, u4WmtCmdPduLen, &u4WrittenSize); */
		iRet = wmt_core_tx((PUINT8) &rWmtPktCmd, u4WmtCmdPduLen, &u4WrittenSize, MTK_WCN_BOOL_FALSE);
		if (iRet) {
			WMT_ERR_FUNC("WMT-CORE: wmt_func_ctrl_cmd kal_stp_tx failed\n");
			break;
		}

		iRet = wmt_core_rx((PUINT8) &rWmtPktEvent, u4WmtEventPduLen, &u4ReadSize);
		if (iRet) {
			WMT_ERR_FUNC("WMT-CORE: wmt_func_ctrl_cmd kal_stp_rx failed\n");
			break;
		}

		/* Error Checking */
		if (PKT_TYPE_EVENT != rWmtPktEvent.eType) {
			WMT_ERR_FUNC("WMT-CORE: wmt_func_ctrl_cmd PKT_TYPE_EVENT != rWmtPktEvent.eType %d\n",
				     rWmtPktEvent.eType);
			break;
		}

		if (rWmtPktCmd.eOpCode != rWmtPktEvent.eOpCode) {
			WMT_ERR_FUNC
			    ("WMT-CORE: wmt_func_ctrl_cmd rWmtPktCmd.eOpCode(0x%x) != rWmtPktEvent.eType(0x%x)\n",
			     rWmtPktCmd.eOpCode, rWmtPktEvent.eOpCode);
			break;
		}

		if (u4WmtEventPduLen != (rWmtPktEvent.u2SduLen + WMT_HDR_LEN)) {
			WMT_ERR_FUNC
			    ("WMT-CORE: wmt_func_ctrl_cmd u4WmtEventPduLen(0x%x) != rWmtPktEvent.u2SduLen(0x%x)+4\n",
			     u4WmtEventPduLen, rWmtPktEvent.u2SduLen);
			break;
		}
		/* Status field of event check */
		if (0 != rWmtPktEvent.aucParam[0]) {
			WMT_ERR_FUNC("WMT-CORE: wmt_func_ctrl_cmd, 0 != status(%d)\n", rWmtPktEvent.aucParam[0]);
			break;
		}

		fgFail = MTK_WCN_BOOL_FALSE;
	} while (0);

	if (MTK_WCN_BOOL_FALSE == fgFail) {
		/* WMT_INFO_FUNC("WMT-CORE: wmt_func_ctrl_cmd OK!\n"); */
		return 0;
	}
	WMT_ERR_FUNC("WMT-CORE: wmt_func_ctrl_cmd 0x%x FAIL\n", rWmtPktCmd.aucParam[0]);
	return -3;
}

INT32 wmt_core_opid_handler(P_WMT_OP pWmtOp)
{
	UINT32 opId;
	INT32 ret;

	opId = pWmtOp->opId;

	if (wmt_core_opfunc[opId]) {
		ret = (*(wmt_core_opfunc[opId])) (pWmtOp);	/*wmtCoreOpidHandlerPack[].opHandler */
		return ret;
	}
	WMT_ERR_FUNC("WMT-CORE: null handler (%d)\n", pWmtOp->opId);
	return -2;

}

INT32 wmt_core_opid(P_WMT_OP pWmtOp)
{

	/*sanity check */
	if (NULL == pWmtOp) {
		WMT_ERR_FUNC("null pWmtOP\n");
		/*print some message with error info */
		return -1;
	}

	if (WMT_OPID_MAX <= pWmtOp->opId) {
		WMT_ERR_FUNC("WMT-CORE: invalid OPID(%d)\n", pWmtOp->opId);
		return -2;
	}
	/* TODO: [FixMe][GeorgeKuo] do sanity check to const function table when init and skip checking here */
	return wmt_core_opid_handler(pWmtOp);
}

INT32 wmt_core_ctrl(ENUM_WMT_CTRL_T ctrId, unsigned long *pPa1, unsigned long *pPa2)
{
	INT32 iRet = -1;
	WMT_CTRL_DATA ctrlData;
	SIZE_T val1 = (pPa1) ? *pPa1 : 0;
	SIZE_T val2 = (pPa2) ? *pPa2 : 0;

	ctrlData.ctrlId = (SIZE_T) ctrId;
	ctrlData.au4CtrlData[0] = val1;
	ctrlData.au4CtrlData[1] = val2;

	iRet = wmt_ctrl(&ctrlData);
	if (iRet) {
		/* ERROR */
		WMT_ERR_FUNC("WMT-CORE: wmt_core_ctrl failed: id(%d), type(%d), value(%d) iRet:(%d)\n", ctrId, val1,
			     val2, iRet);
		osal_assert(0);
	} else {
		if (pPa1)
			*pPa1 = ctrlData.au4CtrlData[0];
		if (pPa2)
			*pPa2 = ctrlData.au4CtrlData[1];
	}
	return iRet;
}

VOID wmt_core_dump_data(PUINT8 pData, PUINT8 pTitle, UINT32 len)
{
	PUINT8 ptr = pData;
	INT32 k = 0;

	WMT_INFO_FUNC("%s len=%d\n", pTitle, len);
	for (k = 0; k < len; k++) {
		if (k % 16 == 0)
			WMT_INFO_FUNC("\n");
		WMT_INFO_FUNC("0x%02x ", *ptr);
		ptr++;
	}
	WMT_INFO_FUNC("--end\n");
}

/*!
 * \brief An WMT-CORE function to support read, write, and read after write to
 * an internal register.
 *
 * Detailed description.
 *
 * \param isWrite 1 for write, 0 for read
 * \param offset of register to be written or read
 * \param pVal a pointer to the 32-bit value to be writtern or read
 * \param mask a 32-bit mask to be applied for the read or write operation
 *
 * \retval 0 operation success
 * \retval -1 invalid parameters
 * \retval -2 tx cmd fail
 * \retval -3 rx event fail
 * \retval -4 read check error
 */
INT32 wmt_core_reg_rw_raw(UINT32 isWrite, UINT32 offset, PUINT32 pVal, UINT32 mask)
{
	INT32 iRet;
	UINT32 u4Res;
	UINT32 evtLen;
	UINT8 evtBuf[16] = { 0 };

	WMT_SET_REG_CMD[4] = (isWrite) ? 0x1 : 0x2;	/* w:1, r:2 */
	osal_memcpy(&WMT_SET_REG_CMD[8], &offset, 4);	/* offset */
	osal_memcpy(&WMT_SET_REG_CMD[12], pVal, 4);	/* [2] is var addr */
	osal_memcpy(&WMT_SET_REG_CMD[16], &mask, 4);	/* mask */

	/* send command */
	iRet = wmt_core_tx(WMT_SET_REG_CMD, sizeof(WMT_SET_REG_CMD), &u4Res, MTK_WCN_BOOL_FALSE);
	if ((iRet) || (u4Res != sizeof(WMT_SET_REG_CMD))) {
		WMT_ERR_FUNC("Tx REG_CMD fail!(%d) len (%d, %d)\n", iRet, u4Res, sizeof(WMT_SET_REG_CMD));
		return -2;
	}

	/* receive event */
	evtLen = (isWrite) ? sizeof(WMT_SET_REG_WR_EVT) : sizeof(WMT_SET_REG_RD_EVT);
	iRet = wmt_core_rx(evtBuf, evtLen, &u4Res);
	if ((iRet) || (u4Res != evtLen)) {
		WMT_ERR_FUNC("Rx REG_EVT fail!(%d) len(%d, %d)\n", iRet, u4Res, evtLen);
		return -3;
	}

	if (!isWrite) {
		UINT32 rxEvtAddr;
		UINT32 txCmdAddr;

		osal_memcpy(&txCmdAddr, &WMT_SET_REG_CMD[8], 4);
		osal_memcpy(&rxEvtAddr, &evtBuf[8], 4);

		/* check read result */
		if (txCmdAddr != rxEvtAddr) {
			WMT_ERR_FUNC("Check read addr fail (0x%08x, 0x%08x)\n", rxEvtAddr, txCmdAddr);
			return -4;
		}
		WMT_DBG_FUNC("Check read addr(0x%08x) ok\n", rxEvtAddr);

		osal_memcpy(pVal, &evtBuf[12], 4);
	}

	/* no error here just return 0 */
	return 0;
}

#if defined(CONFIG_MTK_COMBO_CHIP_CONSYS_6735)
static VOID m2note_wmt_cache_rfcal_evt(INT32 idx, UINT8 *evt, UINT32 got,
				       INT32 ret, UINT8 exp_tail)
{
	UINT32 len = got;
	UINT32 i;
	UINT32 sum = 0;
	UINT32 xors = 0;
	INT32 off_le = -1;
	INT32 off_be = -1;

	if (!evt)
		return;

	for (i = 0; i < got; i++) {
		sum += evt[i];
		xors ^= ((UINT32)evt[i] << ((i & 3) * 8));
		if (i + 1 < got) {
			UINT16 le = (UINT16)evt[i] | ((UINT16)evt[i + 1] << 8);
			UINT16 be = ((UINT16)evt[i] << 8) | (UINT16)evt[i + 1];

			if (off_le < 0 && le == 0x3780)
				off_le = i;
			if (off_be < 0 && be == 0x3780)
				off_be = i;
		}
	}

	if (len > osal_sizeof(m2note_wmt_last_rfcal_evt))
		len = osal_sizeof(m2note_wmt_last_rfcal_evt);

	osal_memset(m2note_wmt_last_rfcal_evt, 0,
		    osal_sizeof(m2note_wmt_last_rfcal_evt));
	osal_memcpy(m2note_wmt_last_rfcal_evt, evt, len);
	m2note_wmt_last_rfcal_len = len;
	m2note_wmt_last_rfcal_got = got;
	m2note_wmt_last_rfcal_ret = ret;
	m2note_wmt_last_rfcal_idx = idx;
	m2note_wmt_last_rfcal_exp_tail = exp_tail;
	m2note_wmt_last_rfcal_declared_len =
		(got >= 4) ? ((UINT32)evt[2] | ((UINT32)evt[3] << 8)) : 0;
	m2note_wmt_last_rfcal_status = (got >= 5) ? evt[4] : 0xff;
	m2note_wmt_last_rfcal_subop = (got >= 6) ? evt[5] : 0xff;
	m2note_wmt_last_rfcal_valid = MTK_WCN_BOOL_TRUE;
	m2note_wmt_last_rfcal_sum = sum;
	m2note_wmt_last_rfcal_xor = xors;
	m2note_wmt_last_rfcal_3780_le = off_le;
	m2note_wmt_last_rfcal_3780_be = off_be;
	m2note_wmt_last_rfcal_seq++;
}

static UINT16 m2note_wmt_be16(UINT16 val)
{
	return ((val & 0x00ff) << 8) | ((val & 0xff00) >> 8);
}

static UINT16 m2note_wmt_patch_ver_hi(UINT32 val)
{
	return ((val & 0xff000000) >> 24) |
	       ((val & 0x00ff0000) >> 16);
}

static VOID m2note_wmt_copy_str(UINT8 *dst, UINT32 dst_len, const UINT8 *src)
{
	UINT32 i;

	if (!dst || !dst_len)
		return;

	osal_memset(dst, 0, dst_len);
	if (!src)
		return;

	for (i = 0; i + 1 < dst_len && src[i]; i++)
		dst[i] = src[i];
}

static VOID m2note_wmt_copy_fixed_str(UINT8 *dst, UINT32 dst_len,
				      const UINT8 *src, UINT32 src_len)
{
	UINT32 len = src_len;

	if (!dst || !dst_len)
		return;

	osal_memset(dst, 0, dst_len);
	if (!src)
		return;

	if (len + 1 > dst_len)
		len = dst_len - 1;
	if (len)
		osal_memcpy(dst, src, len);
}

VOID m2note_wmt_note_patch_result(UINT32 index, const UINT8 *name,
				  const UINT8 *address, const UINT8 *build,
				  const UINT8 *platform, UINT16 hw_ver,
				  UINT16 sw_ver, UINT32 patch_ver,
				  UINT32 raw_size, UINT32 body_size,
				  UINT32 frag_num, UINT32 frag_seq,
				  UINT16 last_frag_size, INT32 ret)
{
	struct m2note_wmt_patch_cache_entry *entry;

	if (m2note_wmt_patch_cache_count >= M2NOTE_WMT_PATCH_CACHE_MAX) {
		m2note_wmt_patch_cache_dropped++;
		return;
	}

	entry = &m2note_wmt_patch_cache[m2note_wmt_patch_cache_count++];
	entry->index = index;
	entry->ret = ret;
	entry->raw_size = raw_size;
	entry->body_size = body_size;
	entry->frag_num = frag_num;
	entry->frag_seq = frag_seq;
	entry->last_frag_size = last_frag_size;
	entry->hw_ver = m2note_wmt_be16(hw_ver);
	entry->sw_ver = m2note_wmt_be16(sw_ver);
	entry->patch_ver = m2note_wmt_patch_ver_hi(patch_ver);
	if (address)
		osal_memcpy(entry->address, address, osal_sizeof(entry->address));
	m2note_wmt_copy_str(entry->name, osal_sizeof(entry->name), name);
	m2note_wmt_copy_str(entry->build, osal_sizeof(entry->build), build);
	m2note_wmt_copy_fixed_str(entry->platform, osal_sizeof(entry->platform),
				  platform, 4);

	WMT_WARN_FUNC("M2NOTE_WCN_PATCH_RESULT_TRACE seq=%u index=%u status=%s ret=%d name=%s raw=%u body=%u frag_num=%u frag_seq=%u last_frag=%u hw=0x%04x sw=0x%04x ph=0x%04x addr=[%02X %02X %02X %02X] build=%s platform=%s\n",
		      m2note_wmt_patch_cache_seq, entry->index,
		      (!ret && frag_seq == frag_num) ? "ok" : "fail",
		      entry->ret, entry->name, entry->raw_size,
		      entry->body_size, entry->frag_num, entry->frag_seq,
		      entry->last_frag_size, entry->hw_ver, entry->sw_ver,
		      entry->patch_ver, entry->address[0], entry->address[1],
		      entry->address[2], entry->address[3], entry->build,
		      entry->platform);
}

VOID m2note_wmt_note_sw_init_start(UINT32 hif_type, UINT32 hw_ver)
{
#if defined(CONFIG_MTK_COMBO_CHIP_CONSYS_6735)
	osal_memset(m2note_wmt_init_script_cache, 0,
		    osal_sizeof(m2note_wmt_init_script_cache));
	osal_memset(m2note_wmt_stage_cache, 0,
		    osal_sizeof(m2note_wmt_stage_cache));
	osal_memset(m2note_wmt_patch_cache, 0,
		    osal_sizeof(m2note_wmt_patch_cache));
	osal_memset(m2note_wmt_last_rfcal_evt, 0,
		    osal_sizeof(m2note_wmt_last_rfcal_evt));
	m2note_wmt_init_script_cache_seq++;
	m2note_wmt_stage_cache_seq = m2note_wmt_init_script_cache_seq;
	m2note_wmt_patch_cache_seq = m2note_wmt_init_script_cache_seq;
	m2note_wmt_init_script_cache_count = 0;
	m2note_wmt_init_script_cache_dropped = 0;
	m2note_wmt_stage_cache_count = 0;
	m2note_wmt_stage_cache_dropped = 0;
	m2note_wmt_patch_cache_count = 0;
	m2note_wmt_patch_cache_dropped = 0;
	m2note_wmt_patch_cache_dump_seq = 0;
	m2note_wmt_init_script_cache_hif = hif_type;
	m2note_wmt_init_script_cache_hw = hw_ver;
	m2note_wmt_last_rfcal_valid = MTK_WCN_BOOL_FALSE;
	m2note_wmt_last_rfcal_len = 0;
	m2note_wmt_last_rfcal_got = 0;
	m2note_wmt_last_rfcal_ret = 0;
	m2note_wmt_last_rfcal_idx = -1;
	m2note_wmt_last_rfcal_declared_len = 0;
	m2note_wmt_last_rfcal_status = 0xff;
	m2note_wmt_last_rfcal_subop = 0xff;
	m2note_wmt_last_rfcal_exp_tail = 0;
	m2note_wmt_last_rfcal_sum = 0;
	m2note_wmt_last_rfcal_xor = 0;
	m2note_wmt_last_rfcal_3780_le = -1;
	m2note_wmt_last_rfcal_3780_be = -1;
	m2note_wmt_last_coex_valid = MTK_WCN_BOOL_FALSE;
	m2note_wmt_last_coex_ret = 0;
	WMT_WARN_FUNC("M2NOTE_WCN_SW_INIT_TRACE stage=start seq=%u hif=%u hw=0x%08x\n",
		      m2note_wmt_init_script_cache_seq, hif_type, hw_ver);
#endif
}

VOID m2note_wmt_get_last_rfcal(UINT32 *got, UINT8 *status)
{
#if defined(CONFIG_MTK_COMBO_CHIP_CONSYS_6735)
	if (got)
		*got = m2note_wmt_last_rfcal_valid ?
			m2note_wmt_last_rfcal_got : 0;
	if (status)
		*status = m2note_wmt_last_rfcal_valid ?
			m2note_wmt_last_rfcal_status : 0xff;
#else
	if (got)
		*got = 0;
	if (status)
		*status = 0xff;
#endif
}

VOID m2note_wmt_note_coex_result(UINT32 cfg_exist, UINT32 ant, UINT32 filter,
				 UINT32 co_clock, UINT32 gps_lna_pin,
				 UINT32 gps_lna_enable, UINT32 sdio,
				 UINT32 cmd5, INT32 ret)
{
#if defined(CONFIG_MTK_COMBO_CHIP_CONSYS_6735)
	m2note_wmt_last_coex_valid = MTK_WCN_BOOL_TRUE;
	m2note_wmt_last_coex_seq = m2note_wmt_init_script_cache_seq;
	m2note_wmt_last_coex_cfg_exist = cfg_exist;
	m2note_wmt_last_coex_ant = ant;
	m2note_wmt_last_coex_filter = filter;
	m2note_wmt_last_coex_co_clock = co_clock;
	m2note_wmt_last_coex_gps_lna_pin = gps_lna_pin;
	m2note_wmt_last_coex_gps_lna_enable = gps_lna_enable;
	m2note_wmt_last_coex_sdio = sdio;
	m2note_wmt_last_coex_cmd5 = cmd5;
	m2note_wmt_last_coex_ret = ret;
	WMT_WARN_FUNC("M2NOTE_WCN_WMT_COEX_RESULT_TRACE seq=%u cfgExist=%u ant=%u filter=%u cmd5=0x%02x co_clock=%u gps_lna_pin=%u gps_lna_en=%u sdio=0x%x ret=%d\n",
		      m2note_wmt_last_coex_seq, cfg_exist, ant, filter, cmd5,
		      co_clock, gps_lna_pin, gps_lna_enable, sdio, ret);
#endif
}

VOID m2note_wmt_dump_adie_rx_state(const char *stage)
{
#if defined(CONFIG_MTK_COMBO_CHIP_CONSYS_6735)
	const char *trace_stage = stage ? stage : "null";
	INT32 adie_2400 = mtk_wcn_m2note_adie_reg_read(0x2400);
	INT32 adie_2404 = mtk_wcn_m2note_adie_reg_read(0x2404);
	UINT32 raw08_2400 = 0;
	UINT32 raw08_2404 = 0;
	UINT32 raw08_2408 = 0;
	UINT32 raw08_240c = 0;
	INT32 raw08_2400_ret = mtk_wcn_m2note_reg08_read_trace(trace_stage,
							       0x00002400,
							       &raw08_2400);
	INT32 raw08_2404_ret = mtk_wcn_m2note_reg08_read_trace(trace_stage,
							       0x00002404,
							       &raw08_2404);
	INT32 raw08_2408_ret = mtk_wcn_m2note_reg08_read_trace(trace_stage,
							       0x00002408,
							       &raw08_2408);
	INT32 raw08_240c_ret = mtk_wcn_m2note_reg08_read_trace(trace_stage,
							       0x0000240c,
							       &raw08_240c);

	WMT_WARN_FUNC("M2NOTE_WCN_ADIE_RX_TRACE stage=%s reg2400_retval=0x%04x reg2404_retval=0x%04x raw08_2400_ret=%d raw08_2400=0x%08x raw08_2400_tail16=0x%04x raw08_2404_ret=%d raw08_2404=0x%08x raw08_2404_tail16=0x%04x raw08_2408_ret=%d raw08_2408=0x%08x raw08_2408_tail16=0x%04x raw08_240c_ret=%d raw08_240c=0x%08x raw08_240c_tail16=0x%04x\n",
		      trace_stage, adie_2400 & 0xffff, adie_2404 & 0xffff,
		      raw08_2400_ret, raw08_2400,
		      (raw08_2400 >> 16) & 0xffff,
		      raw08_2404_ret, raw08_2404,
		      (raw08_2404 >> 16) & 0xffff,
		      raw08_2408_ret, raw08_2408,
		      (raw08_2408 >> 16) & 0xffff,
		      raw08_240c_ret, raw08_240c,
		      (raw08_240c >> 16) & 0xffff);
#endif
}

static VOID m2note_wmt_cache_init_script_event(INT32 idx, UINT8 *name,
					       UINT8 *cmd, UINT32 cmd_sz,
					       UINT8 *evt, UINT8 *exp,
					       UINT32 evt_sz, INT32 ret,
					       UINT32 got, const char *status)
{
	struct m2note_wmt_init_script_cache_entry *entry;
	UINT32 len;

	if (m2note_wmt_init_script_cache_count >= M2NOTE_WMT_INIT_SCRIPT_CACHE_MAX) {
		m2note_wmt_init_script_cache_dropped++;
		return;
	}

	entry = &m2note_wmt_init_script_cache[m2note_wmt_init_script_cache_count++];
	entry->name = (const char *)name;
	entry->status = status;
	entry->idx = idx;
	entry->ret = ret;
	entry->got = got;
	entry->cmd_sz = cmd_sz;
	entry->evt_sz = evt_sz;

	len = cmd_sz;
	if (len > M2NOTE_WMT_INIT_SCRIPT_CACHE_BYTES)
		len = M2NOTE_WMT_INIT_SCRIPT_CACHE_BYTES;
	if (cmd && len)
		osal_memcpy(entry->cmd, cmd, len);

	len = got;
	if (len > M2NOTE_WMT_INIT_SCRIPT_CACHE_BYTES)
		len = M2NOTE_WMT_INIT_SCRIPT_CACHE_BYTES;
	if (evt && len)
		osal_memcpy(entry->evt, evt, len);

	len = evt_sz;
	if (len > M2NOTE_WMT_INIT_SCRIPT_CACHE_BYTES)
		len = M2NOTE_WMT_INIT_SCRIPT_CACHE_BYTES;
	if (exp && len)
		osal_memcpy(entry->exp, exp, len);
}

static MTK_WCN_BOOL m2note_wmt_stage_dumps_init_cache(const char *stage)
{
	if (!stage)
		return MTK_WCN_BOOL_FALSE;

	if (osal_strstr((char *)stage, "wifi_func_before_probe") ||
	    osal_strstr((char *)stage, "after_5g_support") ||
	    osal_strstr((char *)stage, "before_psm_gate") ||
	    osal_strstr((char *)stage, "debug_snapshot"))
		return MTK_WCN_BOOL_TRUE;

	return MTK_WCN_BOOL_FALSE;
}

static VOID m2note_wmt_dump_init_script_cache(const char *stage)
{
	UINT32 i;

	if (!m2note_wmt_stage_dumps_init_cache(stage))
		return;

	if (m2note_wmt_init_script_cache_dump_seq == m2note_wmt_init_script_cache_seq)
		return;

	m2note_wmt_init_script_cache_dump_seq = m2note_wmt_init_script_cache_seq;
	WMT_WARN_FUNC("M2NOTE_WCN_INIT_SCRIPT_CACHE_SUMMARY stage=%s seq=%u hif=%u hw=0x%08x entries=%u dropped=%u\n",
		      stage, m2note_wmt_init_script_cache_seq,
		      m2note_wmt_init_script_cache_hif,
		      m2note_wmt_init_script_cache_hw,
		      m2note_wmt_init_script_cache_count,
		      m2note_wmt_init_script_cache_dropped);

	for (i = 0; i < m2note_wmt_init_script_cache_count; i++) {
		struct m2note_wmt_init_script_cache_entry *entry =
			&m2note_wmt_init_script_cache[i];

		WMT_WARN_FUNC("M2NOTE_WCN_INIT_SCRIPT_CACHE_TRACE stage=%s seq=%u slot=%u idx=%d status=%s op=%s ret=%d got=%u cmdsz=%u evtsz=%u cmd=[%02X %02X %02X %02X %02X %02X %02X %02X] evt=[%02X %02X %02X %02X %02X %02X %02X %02X] exp=[%02X %02X %02X %02X %02X %02X %02X %02X]\n",
			      stage, m2note_wmt_init_script_cache_seq, i,
			      entry->idx,
			      entry->status ? entry->status : "null",
			      entry->name ? entry->name : "null",
			      entry->ret, entry->got, entry->cmd_sz,
			      entry->evt_sz,
			      entry->cmd[0], entry->cmd[1], entry->cmd[2],
			      entry->cmd[3], entry->cmd[4], entry->cmd[5],
			      entry->cmd[6], entry->cmd[7],
			      entry->evt[0], entry->evt[1], entry->evt[2],
			      entry->evt[3], entry->evt[4], entry->evt[5],
			      entry->evt[6], entry->evt[7],
			      entry->exp[0], entry->exp[1], entry->exp[2],
			      entry->exp[3], entry->exp[4], entry->exp[5],
			      entry->exp[6], entry->exp[7]);
	}
}

static VOID m2note_wmt_dump_patch_cache(const char *stage)
{
	UINT32 i;

	if (!m2note_wmt_stage_dumps_init_cache(stage))
		return;

	if (m2note_wmt_patch_cache_dump_seq == m2note_wmt_patch_cache_seq)
		return;

	m2note_wmt_patch_cache_dump_seq = m2note_wmt_patch_cache_seq;
	WMT_WARN_FUNC("M2NOTE_WCN_PATCH_CACHE_SUMMARY stage=%s seq=%u entries=%u dropped=%u\n",
		      stage, m2note_wmt_patch_cache_seq,
		      m2note_wmt_patch_cache_count,
		      m2note_wmt_patch_cache_dropped);

	for (i = 0; i < m2note_wmt_patch_cache_count; i++) {
		struct m2note_wmt_patch_cache_entry *entry =
			&m2note_wmt_patch_cache[i];

		WMT_WARN_FUNC("M2NOTE_WCN_PATCH_CACHE_TRACE stage=%s seq=%u slot=%u index=%u status=%s ret=%d name=%s raw=%u body=%u frag_num=%u frag_seq=%u last_frag=%u hw=0x%04x sw=0x%04x ph=0x%04x addr=[%02X %02X %02X %02X] build=%s platform=%s\n",
			      stage, m2note_wmt_patch_cache_seq, i,
			      entry->index,
			      (!entry->ret && entry->frag_seq == entry->frag_num) ?
			      "ok" : "fail",
			      entry->ret, entry->name, entry->raw_size,
			      entry->body_size, entry->frag_num,
			      entry->frag_seq, entry->last_frag_size,
			      entry->hw_ver, entry->sw_ver, entry->patch_ver,
			      entry->address[0], entry->address[1],
			      entry->address[2], entry->address[3],
			      entry->build, entry->platform);
	}
}

static VOID m2note_wmt_dump_rfcal_cache(const char *stage)
{
	if (!m2note_wmt_last_rfcal_valid) {
		WMT_WARN_FUNC("M2NOTE_WCN_RFCAL_CACHE_TRACE stage=%s valid=0\n",
			      stage);
		return;
	}

	WMT_WARN_FUNC("M2NOTE_WCN_RFCAL_CACHE_TRACE stage=%s valid=1 seq=%u idx=%d ret=%d got=%u saved=%u declared_len=%u status=%02X subop=%02X exp_tail=%02X truncated=%u sum=0x%08x xor=0x%08x marker3780_le_off=%d marker3780_be_off=%d trace_bytes=%u evt=%*phN\n",
		      stage, m2note_wmt_last_rfcal_seq, m2note_wmt_last_rfcal_idx,
		      m2note_wmt_last_rfcal_ret, m2note_wmt_last_rfcal_got,
		      m2note_wmt_last_rfcal_len,
		      m2note_wmt_last_rfcal_declared_len,
		      m2note_wmt_last_rfcal_status,
		      m2note_wmt_last_rfcal_subop,
		      m2note_wmt_last_rfcal_exp_tail,
		      m2note_wmt_last_rfcal_declared_len >
		      m2note_wmt_last_rfcal_got ? 1 : 0,
		      m2note_wmt_last_rfcal_sum,
		      m2note_wmt_last_rfcal_xor,
		      m2note_wmt_last_rfcal_3780_le,
		      m2note_wmt_last_rfcal_3780_be,
		      M2NOTE_WMT_RFCAL_TRACE_BYTES,
		      M2NOTE_WMT_RFCAL_TRACE_BYTES,
		      m2note_wmt_last_rfcal_evt);
}

static struct m2note_wmt_stage_cache_entry *m2note_wmt_alloc_stage_cache(const char *stage)
{
	struct m2note_wmt_stage_cache_entry *entry;

	if (m2note_wmt_stage_cache_count >= M2NOTE_WMT_STAGE_CACHE_MAX) {
		m2note_wmt_stage_cache_dropped++;
		return NULL;
	}

	entry = &m2note_wmt_stage_cache[m2note_wmt_stage_cache_count];
	entry->stage = stage;
	entry->seq = m2note_wmt_stage_cache_seq;
	entry->slot = m2note_wmt_stage_cache_count;
	entry->reg_count = osal_array_size(m2note_wmt_snapshot_regs);
	m2note_wmt_stage_cache_count++;
	return entry;
}

static VOID m2note_wmt_dump_stage_cache(const char *stage)
{
	UINT32 i;

	if (!m2note_wmt_stage_dumps_init_cache(stage))
		return;

	if (m2note_wmt_stage_cache_dump_seq == m2note_wmt_stage_cache_seq)
		return;

	m2note_wmt_stage_cache_dump_seq = m2note_wmt_stage_cache_seq;
	WMT_WARN_FUNC("M2NOTE_WCN_STAGE_CACHE_SUMMARY stage=%s seq=%u entries=%u dropped=%u\n",
		      stage, m2note_wmt_stage_cache_seq,
		      m2note_wmt_stage_cache_count,
		      m2note_wmt_stage_cache_dropped);

	for (i = 0; i < m2note_wmt_stage_cache_count; i++) {
		struct m2note_wmt_stage_cache_entry *entry =
			&m2note_wmt_stage_cache[i];

		WMT_WARN_FUNC("M2NOTE_WCN_STAGE_CACHE_TRACE dump_stage=%s seq=%u slot=%u src_stage=%s part=top r80030000_ret=%d r80030000=0x%08x r80030004_ret=%d r80030004=0x%08x r80030010_ret=%d r80030010=0x%08x r80000000_ret=%d r80000000=0x%08x r80000004_ret=%d r80000004=0x%08x r80000008_ret=%d r80000008=0x%08x r80010000_ret=%d r80010000=0x%08x\n",
			      stage, entry->seq, entry->slot,
			      entry->stage ? entry->stage : "null",
			      entry->regs[0].ret, entry->regs[0].val,
			      entry->regs[1].ret, entry->regs[1].val,
			      entry->regs[2].ret, entry->regs[2].val,
			      entry->regs[3].ret, entry->regs[3].val,
			      entry->regs[4].ret, entry->regs[4].val,
			      entry->regs[5].ret, entry->regs[5].val,
			      entry->regs[6].ret, entry->regs[6].val);
		WMT_WARN_FUNC("M2NOTE_WCN_STAGE_CACHE_TRACE dump_stage=%s seq=%u slot=%u src_stage=%s part=rf r00008a00_ret=%d r00008a00=0x%08x r00008a04_ret=%d r00008a04=0x%08x r00008a10_ret=%d r00008a10=0x%08x r00008a18_ret=%d r00008a18=0x%08x read_adie=%u adie2400=0x%04x adie2404=0x%04x\n",
			      stage, entry->seq, entry->slot,
			      entry->stage ? entry->stage : "null",
			      entry->regs[7].ret, entry->regs[7].val,
			      entry->regs[8].ret, entry->regs[8].val,
			      entry->regs[9].ret, entry->regs[9].val,
			      entry->regs[10].ret, entry->regs[10].val,
			      entry->read_adie ? 1 : 0,
			      entry->adie_2400 & 0xffff,
			      entry->adie_2404 & 0xffff);
			WMT_WARN_FUNC("M2NOTE_WCN_STAGE_CACHE_TRACE dump_stage=%s seq=%u slot=%u src_stage=%s part=cfg r80050050_ret=%d r80050050=0x%08x r80050074_ret=%d r80050074=0x%08x r80050078_ret=%d r80050078=0x%08x r80050110_ret=%d r80050110=0x%08x r80050130_ret=%d r80050130=0x%08x r80050140_ret=%d r80050140=0x%08x r80050150_ret=%d r80050150=0x%08x r80050300_ret=%d r80050300=0x%08x\n",
			      stage, entry->seq, entry->slot,
			      entry->stage ? entry->stage : "null",
			      entry->regs[11].ret, entry->regs[11].val,
			      entry->regs[12].ret, entry->regs[12].val,
			      entry->regs[13].ret, entry->regs[13].val,
			      entry->regs[14].ret, entry->regs[14].val,
			      entry->regs[15].ret, entry->regs[15].val,
			      entry->regs[16].ret, entry->regs[16].val,
			      entry->regs[17].ret, entry->regs[17].val,
			      entry->regs[18].ret, entry->regs[18].val);
	}
}

static VOID m2note_wmt_dump_init_summary(const char *stage)
{
	WMT_WARN_FUNC("M2NOTE_WCN_INIT_SUMMARY_TRACE stage=%s part=cache seq=%u hif=%u hw=0x%08x init_entries=%u init_dropped=%u patch_entries=%u patch_dropped=%u stage_entries=%u stage_dropped=%u\n",
		      stage, m2note_wmt_init_script_cache_seq,
		      m2note_wmt_init_script_cache_hif,
		      m2note_wmt_init_script_cache_hw,
		      m2note_wmt_init_script_cache_count,
		      m2note_wmt_init_script_cache_dropped,
		      m2note_wmt_patch_cache_count,
		      m2note_wmt_patch_cache_dropped,
		      m2note_wmt_stage_cache_count,
		      m2note_wmt_stage_cache_dropped);
	WMT_WARN_FUNC("M2NOTE_WCN_INIT_SUMMARY_TRACE stage=%s part=rf valid=%u seq=%u idx=%d ret=%d got=%u saved=%u declared_len=%u status=%02X subop=%02X sum=0x%08x xor=0x%08x marker3780_le_off=%d marker3780_be_off=%d\n",
		      stage,
		      m2note_wmt_last_rfcal_valid ? 1 : 0,
		      m2note_wmt_last_rfcal_seq,
		      m2note_wmt_last_rfcal_idx,
		      m2note_wmt_last_rfcal_ret,
		      m2note_wmt_last_rfcal_got,
		      m2note_wmt_last_rfcal_len,
		      m2note_wmt_last_rfcal_declared_len,
		      m2note_wmt_last_rfcal_status,
		      m2note_wmt_last_rfcal_subop,
		      m2note_wmt_last_rfcal_sum,
		      m2note_wmt_last_rfcal_xor,
		      m2note_wmt_last_rfcal_3780_le,
		      m2note_wmt_last_rfcal_3780_be);
	WMT_WARN_FUNC("M2NOTE_WCN_INIT_SUMMARY_TRACE stage=%s part=coex valid=%u seq=%u cfg=%u ant=%u filter=%u cmd5=0x%02x co_clock=%u gps_lna_pin=%u gps_lna_en=%u sdio=0x%x ret=%d\n",
		      stage,
		      m2note_wmt_last_coex_valid ? 1 : 0,
		      m2note_wmt_last_coex_seq,
		      m2note_wmt_last_coex_cfg_exist,
		      m2note_wmt_last_coex_ant,
		      m2note_wmt_last_coex_filter,
		      m2note_wmt_last_coex_cmd5,
		      m2note_wmt_last_coex_co_clock,
		      m2note_wmt_last_coex_gps_lna_pin,
		      m2note_wmt_last_coex_gps_lna_enable,
		      m2note_wmt_last_coex_sdio,
		      m2note_wmt_last_coex_ret);
}

static MTK_WCN_BOOL m2note_wmt_stage_reads_adie(const char *stage)
{
	if (!stage)
		return MTK_WCN_BOOL_FALSE;

	if (osal_strstr((char *)stage, "after_power_on_dlm") ||
	    osal_strstr((char *)stage, "after_set_mcuclk") ||
	    osal_strstr((char *)stage, "after_patch") ||
	    osal_strstr((char *)stage, "after_wmt_reset") ||
	    osal_strstr((char *)stage, "before_lte") ||
	    osal_strstr((char *)stage, "after_lte") ||
	    osal_strstr((char *)stage, "before_rfcal") ||
	    osal_strstr((char *)stage, "after_rfcal") ||
	    osal_strstr((char *)stage, "after_init_coex") ||
	    osal_strstr((char *)stage, "after_crystal") ||
	    osal_strstr((char *)stage, "after_sdio") ||
	    osal_strstr((char *)stage, "after_fm") ||
	    osal_strstr((char *)stage, "after_set_opt") ||
	    osal_strstr((char *)stage, "after_5g") ||
	    osal_strstr((char *)stage, "before_psm") ||
	    osal_strstr((char *)stage, "wifi_func"))
		return MTK_WCN_BOOL_TRUE;

	return MTK_WCN_BOOL_FALSE;
}

static VOID m2note_wmt_dump_reg_list(const char *stage, const char *bank,
				     const UINT32 *regs, INT32 count,
				     struct m2note_wmt_stage_cache_entry *cache_entry)
{
	INT32 i;

	for (i = 0; i < count; i++) {
		UINT32 val = 0;
		INT32 ret = wmt_core_reg_rw_raw(0, regs[i], &val, 0xffffffff);

		if (cache_entry && i < M2NOTE_WMT_STAGE_REG_COUNT) {
			cache_entry->regs[i].reg = regs[i];
			cache_entry->regs[i].ret = ret;
			cache_entry->regs[i].val = val;
		}

		WMT_WARN_FUNC("M2NOTE_WCN_STAGE_REG_TRACE stage=%s reg=0x%08x ret=%d val=0x%08x bank=%s\n",
			      stage, regs[i], ret, val, bank);
	}
}
#endif

VOID m2note_wmt_dump_combo_regs(const char *stage)
{
#if defined(CONFIG_MTK_COMBO_CHIP_CONSYS_6735)
	const char *trace_stage = stage ? stage : "null";
	struct m2note_wmt_stage_cache_entry *stage_entry;

	stage_entry = m2note_wmt_alloc_stage_cache(trace_stage);
	m2note_wmt_dump_reg_list(trace_stage, "core",
				 m2note_wmt_snapshot_regs,
				 osal_array_size(m2note_wmt_snapshot_regs),
				 stage_entry);

	if (m2note_wmt_stage_reads_adie(stage)) {
		INT32 adie_2400 = mtk_wcn_m2note_adie_reg_read(0x2400);
		INT32 adie_2404 = mtk_wcn_m2note_adie_reg_read(0x2404);
		UINT32 reg08_2400 = 0;
		UINT32 reg08_2404 = 0;
		UINT32 reg08_2408 = 0;
		UINT32 reg08_240c = 0;
		INT32 reg08_2400_ret =
			mtk_wcn_m2note_reg08_read_trace(trace_stage,
							0x00002400,
							&reg08_2400);
		INT32 reg08_2404_ret =
			mtk_wcn_m2note_reg08_read_trace(trace_stage,
							0x00002404,
							&reg08_2404);
		INT32 reg08_2408_ret =
			mtk_wcn_m2note_reg08_read_trace(trace_stage,
							0x00002408,
							&reg08_2408);
		INT32 reg08_240c_ret =
			mtk_wcn_m2note_reg08_read_trace(trace_stage,
							0x0000240c,
							&reg08_240c);

		if (stage_entry) {
			stage_entry->read_adie = MTK_WCN_BOOL_TRUE;
			stage_entry->adie_2400 = adie_2400;
			stage_entry->adie_2404 = adie_2404;
		}
		WMT_WARN_FUNC("M2NOTE_WCN_STAGE_ADIE_TRACE stage=%s adie2400=0x%04x adie2404=0x%04x\n",
			      trace_stage, adie_2400 & 0xffff, adie_2404 & 0xffff);
			WMT_WARN_FUNC("M2NOTE_WCN_STAGE_ADIE_REG08_TRACE stage=%s reg2400_ret=%d reg2400=0x%08x reg2400_tail16=0x%04x reg2404_ret=%d reg2404=0x%08x reg2404_tail16=0x%04x legacy_3_10_adie_marker_suspect=0x3780\n",
			      trace_stage, reg08_2400_ret, reg08_2400,
			      (reg08_2400 >> 16) & 0xffff,
			      reg08_2404_ret, reg08_2404,
			      (reg08_2404 >> 16) & 0xffff);
		WMT_WARN_FUNC("M2NOTE_WCN_STAGE_REG08_EXTRA_TRACE stage=%s reg2408_ret=%d reg2408=0x%08x reg2408_tail16=0x%04x reg240c_ret=%d reg240c=0x%08x reg240c_tail16=0x%04x\n",
			      trace_stage, reg08_2408_ret, reg08_2408,
			      (reg08_2408 >> 16) & 0xffff,
			      reg08_240c_ret, reg08_240c,
			      (reg08_240c >> 16) & 0xffff);
		m2note_wmt_dump_rfcal_cache(trace_stage);
	} else {
		WMT_WARN_FUNC("M2NOTE_WCN_STAGE_ADIE_TRACE stage=%s action=skip_pre_rfcal\n",
			      trace_stage);
	}

	m2note_wmt_dump_init_script_cache(trace_stage);
	m2note_wmt_dump_patch_cache(trace_stage);
	m2note_wmt_dump_stage_cache(trace_stage);
	m2note_wmt_dump_init_summary(trace_stage);
#endif
}

VOID m2note_wmt_dump_debug_snapshot(const char *stage)
{
#if defined(CONFIG_MTK_COMBO_CHIP_CONSYS_6735)
	const char *trace_stage = stage ? stage : "debug_snapshot";

	m2note_wmt_init_script_cache_dump_seq = 0;
	m2note_wmt_stage_cache_dump_seq = 0;
	m2note_wmt_patch_cache_dump_seq = 0;
	WMT_WARN_FUNC("M2NOTE_WCN_DEBUG_SNAPSHOT stage=%s action=start seq=%u stages=%u init=%u patches=%u\n",
		      trace_stage, m2note_wmt_stage_cache_seq,
		      m2note_wmt_stage_cache_count,
		      m2note_wmt_init_script_cache_count,
		      m2note_wmt_patch_cache_count);
	wmt_core_dump_func_state("m2note_dbg_snapshot");
	m2note_wmt_dump_adie_rx_state(trace_stage);
	m2note_wmt_dump_combo_regs(trace_stage);
	WMT_WARN_FUNC("M2NOTE_WCN_DEBUG_SNAPSHOT stage=%s action=done\n",
		      trace_stage);
#endif
}

static MTK_WCN_BOOL m2note_wmt_trace_init_script(UINT8 *name)
{
	if (!name)
		return MTK_WCN_BOOL_FALSE;

	if (osal_strstr((char *)name, "coex") ||
	    osal_strstr((char *)name, "lte") ||
	    osal_strstr((char *)name, "LTE") ||
	    osal_strstr((char *)name, "tdm") ||
	    osal_strstr((char *)name, "TDM") ||
	    osal_strstr((char *)name, "SDIO") ||
	    osal_strstr((char *)name, "sdio") ||
	    osal_strstr((char *)name, "FM") ||
	    osal_strstr((char *)name, "fm") ||
	    osal_strstr((char *)name, "strap") ||
	    osal_strstr((char *)name, "register") ||
	    osal_strstr((char *)name, "core dump") ||
	    osal_strstr((char *)name, "RF") ||
	    osal_strstr((char *)name, "rf") ||
	    osal_strstr((char *)name, "calibration") ||
	    osal_strstr((char *)name, "efuse") ||
	    osal_strstr((char *)name, "Efuse") ||
	    osal_strstr((char *)name, "crystal") ||
	    osal_strstr((char *)name, "Crystal"))
		return MTK_WCN_BOOL_TRUE;

	return MTK_WCN_BOOL_FALSE;
}

INT32 wmt_core_init_script(struct init_script *script, INT32 count)
{
	UINT8 evtBuf[M2NOTE_WMT_RFCAL_EVT_BUF_SIZE];
	UINT32 u4Res;
	UINT32 rxLen;
	INT32 i = 0;
	INT32 iRet;
	MTK_WCN_BOOL m2noteTrace;
	MTK_WCN_BOOL m2noteRfcal;
	MTK_WCN_BOOL m2noteRxOk;

	for (i = 0; i < count; i++) {
		m2noteTrace = m2note_wmt_trace_init_script(script[i].str);
		m2noteRfcal = (script[i].cmdSz > 1 && script[i].cmd[1] == 0x14) ?
			MTK_WCN_BOOL_TRUE : MTK_WCN_BOOL_FALSE;
		if (m2noteTrace) {
			WMT_WARN_FUNC("M2NOTE_WCN_INIT_SCRIPT_TRACE stage=start idx=%d op=%s cmdsz=%u evtsz=%u cmd=[%02X %02X %02X %02X %02X %02X] exp=[%02X %02X %02X %02X %02X %02X]\n",
				      i, (char *)script[i].str, script[i].cmdSz, script[i].evtSz,
				      script[i].cmdSz > 0 ? script[i].cmd[0] : 0,
				      script[i].cmdSz > 1 ? script[i].cmd[1] : 0,
				      script[i].cmdSz > 2 ? script[i].cmd[2] : 0,
				      script[i].cmdSz > 3 ? script[i].cmd[3] : 0,
				      script[i].cmdSz > 4 ? script[i].cmd[4] : 0,
				      script[i].cmdSz > 5 ? script[i].cmd[5] : 0,
				      script[i].evtSz > 0 ? script[i].evt[0] : 0,
				      script[i].evtSz > 1 ? script[i].evt[1] : 0,
				      script[i].evtSz > 2 ? script[i].evt[2] : 0,
				      script[i].evtSz > 3 ? script[i].evt[3] : 0,
				      script[i].evtSz > 4 ? script[i].evt[4] : 0,
				      script[i].evtSz > 5 ? script[i].evt[5] : 0);
		}
		WMT_DBG_FUNC("WMT-CORE: init_script operation %s start\n", script[i].str);
		/* CMD */
		/* iRet = (*kal_stp_tx)(script[i].cmd, script[i].cmdSz, &u4Res); */
		iRet = wmt_core_tx(script[i].cmd, script[i].cmdSz, &u4Res, MTK_WCN_BOOL_FALSE);
		if (iRet || (u4Res != script[i].cmdSz)) {
			WMT_ERR_FUNC("WMT-CORE: write (%s) iRet(%d) cmd len err(%d, %d)\n", script[i].str, iRet, u4Res,
				     script[i].cmdSz);
			if (m2noteTrace) {
				WMT_WARN_FUNC("M2NOTE_WCN_INIT_SCRIPT_TRACE stage=tx_fail idx=%d op=%s ret=%d wrote=%u exp=%u\n",
					      i, (char *)script[i].str, iRet, u4Res, script[i].cmdSz);
			}
#if defined(CONFIG_MTK_COMBO_CHIP_CONSYS_6735)
			m2note_wmt_cache_init_script_event(i, script[i].str,
							   script[i].cmd, script[i].cmdSz,
							   NULL, script[i].evt, script[i].evtSz,
							   iRet, u4Res, "tx_fail");
#endif
			break;
		}
		/* EVENT BUF */
		osal_memset(evtBuf, 0, sizeof(evtBuf));
		rxLen = m2noteRfcal ? sizeof(evtBuf) : script[i].evtSz;
		iRet = wmt_core_rx(evtBuf, rxLen, &u4Res);
		if (m2noteRfcal) {
			m2noteRxOk = (!iRet &&
				      u4Res >= script[i].evtSz &&
				      script[i].evtSz >= 2 &&
				      evtBuf[0] == script[i].evt[0] &&
				      evtBuf[1] == script[i].evt[1]);
			if (m2noteRxOk && script[i].evtSz >= 6)
				m2noteRxOk = evtBuf[5] == script[i].evt[5];
		} else {
			m2noteRxOk = (!iRet && u4Res == script[i].evtSz);
		}
		if (!m2noteRxOk) {
			WMT_ERR_FUNC("WMT-CORE: read (%s) iRet(%d) evt len err(rx:%d, exp:%d)\n", script[i].str, iRet,
				     u4Res, script[i].evtSz);
			if (m2noteTrace) {
				WMT_WARN_FUNC("M2NOTE_WCN_INIT_SCRIPT_TRACE stage=rx_fail idx=%d op=%s ret=%d got=%u exp=%u rx_req=%u rfcal=%u evt=[%02X %02X %02X %02X %02X %02X]\n",
					      i, (char *)script[i].str, iRet, u4Res, script[i].evtSz,
					      rxLen, m2noteRfcal,
					      evtBuf[0], evtBuf[1], evtBuf[2], evtBuf[3], evtBuf[4], evtBuf[5]);
			}
#if defined(CONFIG_MTK_COMBO_CHIP_CONSYS_6735)
			m2note_wmt_cache_init_script_event(i, script[i].str,
							   script[i].cmd, script[i].cmdSz,
							   evtBuf, script[i].evt, script[i].evtSz,
							   iRet, u4Res, "rx_fail");
#endif
			mtk_wcn_stp_dbg_dump_package();
			break;
		}
		if (m2noteTrace) {
			WMT_WARN_FUNC("M2NOTE_WCN_INIT_SCRIPT_TRACE stage=rx idx=%d op=%s ret=%d got=%u rx_req=%u rfcal=%u evt=[%02X %02X %02X %02X %02X %02X]\n",
				      i, (char *)script[i].str, iRet, u4Res,
				      rxLen, m2noteRfcal,
				      evtBuf[0], evtBuf[1], evtBuf[2], evtBuf[3], evtBuf[4], evtBuf[5]);
		}
		/* RESULT */
		if (0x14 == evtBuf[1]) {	/* RF calibration data EVT — dump it (was silently ignored) */
#if defined(CONFIG_MTK_COMBO_CHIP_CONSYS_6735)
			m2note_wmt_cache_rfcal_evt(i, evtBuf, u4Res, iRet,
						   script[i].evtSz >= 6 ? script[i].evt[5] : 0xff);
#endif
			WMT_WARN_FUNC("M2NOTE_WCN_RFCAL_EVT_TRACE op=%s sz=%d declared_len=%u status=%02X subop=%02X truncated=%u trace_bytes=%u evt=%*phN exp_last=%02X\n",
				      script[i].str, u4Res,
				      (u4Res >= 4) ? ((UINT32)evtBuf[2] | ((UINT32)evtBuf[3] << 8)) : 0,
				      (u4Res >= 5) ? evtBuf[4] : 0xff,
				      (u4Res >= 6) ? evtBuf[5] : 0xff,
				      ((u4Res >= 4) && (((UINT32)evtBuf[2] | ((UINT32)evtBuf[3] << 8)) > u4Res)) ? 1 : 0,
				      M2NOTE_WMT_RFCAL_TRACE_BYTES,
				      M2NOTE_WMT_RFCAL_TRACE_BYTES, evtBuf,
				      script[i].evtSz >= 6 ? script[i].evt[5] : 0xff);
		}
		if (0x14 != evtBuf[1]) {	/* workaround RF calibration data EVT,do not care this EVT */
			if (osal_memcmp(evtBuf, script[i].evt, script[i].evtSz) != 0) {
				WMT_ERR_FUNC("WMT-CORE:compare %s result error\n", script[i].str);
				WMT_ERR_FUNC
				    ("WMT-CORE:rx(%d):[%02X,%02X,%02X,%02X,%02X] exp(%d):[%02X,%02X,%02X,%02X,%02X]\n",
				     u4Res, evtBuf[0], evtBuf[1], evtBuf[2], evtBuf[3], evtBuf[4], script[i].evtSz,
				     script[i].evt[0], script[i].evt[1], script[i].evt[2], script[i].evt[3],
				     script[i].evt[4]);
				if (m2noteTrace) {
					WMT_WARN_FUNC("M2NOTE_WCN_INIT_SCRIPT_TRACE stage=cmp_fail idx=%d op=%s got=%u evt=[%02X %02X %02X %02X %02X %02X] exp=[%02X %02X %02X %02X %02X %02X]\n",
						      i, (char *)script[i].str, u4Res,
						      evtBuf[0], evtBuf[1], evtBuf[2], evtBuf[3], evtBuf[4], evtBuf[5],
						      script[i].evtSz > 0 ? script[i].evt[0] : 0,
						      script[i].evtSz > 1 ? script[i].evt[1] : 0,
						      script[i].evtSz > 2 ? script[i].evt[2] : 0,
						      script[i].evtSz > 3 ? script[i].evt[3] : 0,
						      script[i].evtSz > 4 ? script[i].evt[4] : 0,
						      script[i].evtSz > 5 ? script[i].evt[5] : 0);
				}
#if defined(CONFIG_MTK_COMBO_CHIP_CONSYS_6735)
				m2note_wmt_cache_init_script_event(i, script[i].str,
								   script[i].cmd, script[i].cmdSz,
								   evtBuf, script[i].evt, script[i].evtSz,
								   iRet, u4Res, "cmp_fail");
#endif
				mtk_wcn_stp_dbg_dump_package();
				break;
			}
		}
#if defined(CONFIG_MTK_COMBO_CHIP_CONSYS_6735)
		m2note_wmt_cache_init_script_event(i, script[i].str,
						   script[i].cmd, script[i].cmdSz,
						   evtBuf, script[i].evt, script[i].evtSz,
						   iRet, u4Res, "ok");
#endif
		if (m2noteTrace) {
			WMT_WARN_FUNC("M2NOTE_WCN_INIT_SCRIPT_TRACE stage=ok idx=%d op=%s\n",
				      i, (char *)script[i].str);
		}
		WMT_DBG_FUNC("init_script operation %s ok\n", script[i].str);
	}

	return (i == count) ? 0 : -1;
}

static INT32 wmt_core_stp_init(VOID)
{
	INT32 iRet = -1;
	unsigned long ctrlPa1;
	unsigned long ctrlPa2;
	UINT8 co_clock_type;
	P_WMT_CTX pctx = &gMtkWmtCtx;
	P_WMT_GEN_CONF pWmtGenConf = NULL;

	wmt_conf_read_file();
	pWmtGenConf = wmt_conf_get_cfg();
	if (!(pctx->wmtInfoBit & WMT_OP_HIF_BIT)) {
		WMT_ERR_FUNC("WMT-CORE: no hif info!\n");
		osal_assert(0);
		return -1;
	}
	/* 4 <1> open stp */
	ctrlPa1 = 0;
	ctrlPa2 = 0;
	iRet = wmt_core_ctrl(WMT_CTRL_STP_OPEN, &ctrlPa1, &ctrlPa2);
	if (iRet) {
		WMT_ERR_FUNC("WMT-CORE: wmt open stp\n");
		return -2;
	}
	/* 4 <1.5> disable and un-ready stp */
	ctrlPa1 = WMT_STP_CONF_EN;
	ctrlPa2 = 0;
	iRet += wmt_core_ctrl(WMT_CTRL_STP_CONF, &ctrlPa1, &ctrlPa2);
	ctrlPa1 = WMT_STP_CONF_RDY;
	ctrlPa2 = 0;
	iRet += wmt_core_ctrl(WMT_CTRL_STP_CONF, &ctrlPa1, &ctrlPa2);

	/* 4 <2> set mode and enable */
	if (WMT_HIF_BTIF == pctx->wmtHifConf.hifType) {
		ctrlPa1 = WMT_STP_CONF_MODE;
#if defined(CONFIG_MTK_COMBO_CHIP_CONSYS_6735)
		WMT_WARN_FUNC("M2NOTE_WCN_BTIF_MAND_PRE_HW_CHECK hifType=%d\n",
			      pctx->wmtHifConf.hifType);
		ctrlPa2 = MTKSTP_BTIF_MAND_MODE;
#else
		ctrlPa2 = MTKSTP_BTIF_MAND_MODE;
#endif
		iRet += wmt_core_ctrl(WMT_CTRL_STP_CONF, &ctrlPa1, &ctrlPa2);
	}

	ctrlPa1 = WMT_STP_CONF_EN;
	ctrlPa2 = 1;
	iRet += wmt_core_ctrl(WMT_CTRL_STP_CONF, &ctrlPa1, &ctrlPa2);
	if (iRet) {
		WMT_ERR_FUNC("WMT-CORE: stp_init <1><2> fail:%d\n", iRet);
		return -3;
	}
	/* TODO: [ChangeFeature][GeorgeKuo] can we apply raise UART baud rate firstly for ALL supported chips??? */

	iRet = wmt_core_hw_check();
	if (iRet) {
		WMT_ERR_FUNC("hw_check fail:%d\n", iRet);
		return -4;
	}
	/* mtkWmtCtx.p_ic_ops is identified and checked ok */
	if ((NULL != pctx->p_ic_ops->co_clock_ctrl) && (pWmtGenConf != NULL)) {
		co_clock_type = (pWmtGenConf->co_clock_flag & 0x0f);
		(*(pctx->p_ic_ops->co_clock_ctrl)) (co_clock_type == 0 ? WMT_CO_CLOCK_DIS : WMT_CO_CLOCK_EN);
	} else {
		WMT_WARN_FUNC("pctx->p_ic_ops->co_clock_ctrl(0x%x), pWmtGenConf(0x%x)\n", pctx->p_ic_ops->co_clock_ctrl,
			      pWmtGenConf);
	}
	osal_assert(NULL != pctx->p_ic_ops->sw_init);
	if (NULL != pctx->p_ic_ops->sw_init) {
		iRet = (*(pctx->p_ic_ops->sw_init)) (&pctx->wmtHifConf);
	} else {
		WMT_ERR_FUNC("gMtkWmtCtx.p_ic_ops->sw_init is NULL\n");
		return -5;
	}
	if (iRet) {
		WMT_ERR_FUNC("gMtkWmtCtx.p_ic_ops->sw_init fail:%d\n", iRet);
		return -6;
	}
	/* 4 <10> set stp ready */
	ctrlPa1 = WMT_STP_CONF_RDY;
	ctrlPa2 = 1;
	iRet = wmt_core_ctrl(WMT_CTRL_STP_CONF, &ctrlPa1, &ctrlPa2);

	return iRet;
}

static INT32 wmt_core_stp_deinit(VOID)
{
	INT32 iRet;
	unsigned long ctrlPa1;
	unsigned long ctrlPa2;

	WMT_DBG_FUNC(" start\n");

	if (NULL == gMtkWmtCtx.p_ic_ops) {
		WMT_WARN_FUNC("gMtkWmtCtx.p_ic_ops is NULL\n");
		goto deinit_ic_ops_done;
	}
	if (NULL != gMtkWmtCtx.p_ic_ops->sw_deinit) {
		iRet = (*(gMtkWmtCtx.p_ic_ops->sw_deinit)) (&gMtkWmtCtx.wmtHifConf);
		/* unbind WMT-IC */
		gMtkWmtCtx.p_ic_ops = NULL;
	} else {
		WMT_ERR_FUNC("gMtkWmtCtx.p_ic_ops->sw_init is NULL\n");
	}

deinit_ic_ops_done:

	/* 4 <1> un-ready, disable, and close stp. */
	ctrlPa1 = WMT_STP_CONF_RDY;
	ctrlPa2 = 0;
	iRet = wmt_core_ctrl(WMT_CTRL_STP_CONF, &ctrlPa1, &ctrlPa2);
	ctrlPa1 = WMT_STP_CONF_EN;
	ctrlPa2 = 0;
	iRet += wmt_core_ctrl(WMT_CTRL_STP_CONF, &ctrlPa1, &ctrlPa2);
	ctrlPa1 = 0;
	ctrlPa2 = 0;
	iRet += wmt_core_ctrl(WMT_CTRL_STP_CLOSE, &ctrlPa1, &ctrlPa2);

	if (iRet)
		WMT_WARN_FUNC("end with fail:%d\n", iRet);

	return iRet;
}

static VOID wmt_core_dump_func_state(PINT8 pSource)
{
	WMT_WARN_FUNC("[%s]status(b:%d f:%d g:%d w:%d lpbk:%d coredump:%d wmt:%d stp:%d)\n",
		      (pSource == NULL ? (PINT8) "CORE" : pSource),
		      gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_BT],
		      gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_FM],
		      gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_GPS],
		      gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WIFI],
		      gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_LPBK],
		      gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_COREDUMP],
		      gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT], gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_STP]
	    );
	return;

}

static VOID m2note_wmt_trace_state(const char *stage, UINT32 drvType, INT32 ret)
{
	P_WMT_GEN_CONF conf = wmt_conf_get_cfg();

	WMT_WARN_FUNC("M2NOTE_WCN_WMT_STATE_TRACE stage=%s drv=%u ret=%d bt=%u fm=%u gps=%u wifi=%u lpbk=%u coredump=%u wmt=%u stp=%u\n",
		      stage,
		      drvType,
		      ret,
		      gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_BT],
		      gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_FM],
		      gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_GPS],
		      gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WIFI],
		      gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_LPBK],
		      gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_COREDUMP],
		      gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT],
		      gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_STP]);
	WMT_WARN_FUNC("M2NOTE_WCN_WMT_CFG_LIVE stage=%s cfg_ptr=%p cfgExist=%u ant=%u filter=%u co_clock=%u gps_lna_pin=%u gps_lna_en=%u sdio=0x%x\n",
		      stage,
		      conf,
		      conf ? conf->cfgExist : 0,
		      conf ? conf->coex_wmt_ant_mode : 0,
		      conf ? conf->coex_wmt_filter_mode : 0,
		      conf ? conf->co_clock_flag : 0,
		      conf ? conf->wmt_gps_lna_pin : 0,
		      conf ? conf->wmt_gps_lna_enable : 0,
		      conf ? conf->sdio_driving_cfg : 0);
}

MTK_WCN_BOOL wmt_core_patch_check(UINT32 u4PatchVer, UINT32 u4HwVer)
{
	if (MAJORNUM(u4HwVer) != MAJORNUM(u4PatchVer)) {
		/*major no. does not match */
		WMT_ERR_FUNC("WMT-CORE: chip version(0x%d) does not match patch version(0x%d)\n", u4HwVer, u4PatchVer);
		return MTK_WCN_BOOL_FALSE;
	}
	return MTK_WCN_BOOL_TRUE;
}

static INT32 wmt_core_hw_check(VOID)
{
	UINT32 chipid;
	P_WMT_IC_OPS p_ops;
	INT32 iret;

	/* 1. get chip id */
	chipid = 0;
	WMT_LOUD_FUNC("before read hwcode (chip id)\n");
	iret = wmt_core_reg_rw_raw(0, GEN_HCR, &chipid, GEN_HCR_MASK);	/* read 0x80000008 */
	if (iret) {
		WMT_ERR_FUNC("get hwcode (chip id) fail (%d)\n", iret);
		return -2;
	}
	WMT_WARN_FUNC("get hwcode (chip id) (0x%x)\n", chipid);

	/* TODO:[ChangeFeature][George]: use a better way to select a correct ops table based on chip id */
	switch (chipid) {
#if CFG_CORE_MT6620_SUPPORT
	case 0x6620:
		p_ops = &wmt_ic_ops_mt6620;
		break;
#endif
#if CFG_CORE_MT6628_SUPPORT
	case 0x6628:
		p_ops = &wmt_ic_ops_mt6628;
		break;
#endif
#if CFG_CORE_SOC_SUPPORT
	case 0x6572:
	case 0x6582:
	case 0x6592:
	case 0x8127:
	case 0x6571:
	case 0x6752:
	case 0x0279:
	case 0x0326:
	case 0x0321:
	case 0x0335:
	case 0x0337:
	case 0x8163:
	case 0x6580:
		p_ops = &wmt_ic_ops_soc;
		break;
#endif
	default:
		p_ops = (P_WMT_IC_OPS) NULL;
		break;
	}

	if (NULL == p_ops) {
		WMT_ERR_FUNC("unsupported chip id (hw_code): 0x%x\n", chipid);
		return -3;
	} else if (MTK_WCN_BOOL_FALSE == wmt_core_ic_ops_check(p_ops)) {
		WMT_ERR_FUNC
		    ("chip id(0x%x) with null operation fp: init(0x%p), deinit(0x%p), pin_ctrl(0x%p), ver_chk(0x%p)\n",
		     chipid, p_ops->sw_init, p_ops->sw_deinit, p_ops->ic_pin_ctrl, p_ops->ic_ver_check);
		return -4;
	}
	WMT_DBG_FUNC("chip id(0x%x) fp: init(0x%p), deinit(0x%p), pin_ctrl(0x%p), ver_chk(0x%p)\n",
		     chipid, p_ops->sw_init, p_ops->sw_deinit, p_ops->ic_pin_ctrl, p_ops->ic_ver_check);

	wmt_ic_ops_soc.icId = chipid;
	WMT_DBG_FUNC("wmt_ic_ops_soc.icId(0x%x)\n", wmt_ic_ops_soc.icId);
	iret = p_ops->ic_ver_check();
	if (iret) {
		WMT_ERR_FUNC("chip id(0x%x) ver_check error:%d\n", chipid, iret);
		return -5;
	}

	WMT_DBG_FUNC("chip id(0x%x) ver_check ok\n", chipid);
	gMtkWmtCtx.p_ic_ops = p_ops;
	return 0;
}

static INT32 opfunc_hif_conf(P_WMT_OP pWmtOp)
{
	if (!(pWmtOp->u4InfoBit & WMT_OP_HIF_BIT)) {
		WMT_ERR_FUNC("WMT-CORE: no HIF_BIT in WMT_OP!\n");
		return -1;
	}

	if (gMtkWmtCtx.wmtInfoBit & WMT_OP_HIF_BIT) {
		WMT_ERR_FUNC("WMT-CORE: WMT HIF already exist. overwrite! old (%d), new(%d))\n",
			     gMtkWmtCtx.wmtHifConf.hifType, pWmtOp->au4OpData[0]);
	} else {
		gMtkWmtCtx.wmtInfoBit |= WMT_OP_HIF_BIT;
		WMT_ERR_FUNC("WMT-CORE: WMT HIF info added\n");
	}

	osal_memcpy(&gMtkWmtCtx.wmtHifConf, &pWmtOp->au4OpData[0], osal_sizeof(gMtkWmtCtx.wmtHifConf));
	return 0;

}

static INT32 opfunc_pwr_on(P_WMT_OP pWmtOp)
{

	INT32 iRet;
	unsigned long ctrlPa1;
	unsigned long ctrlPa2;
	INT32 retry = WMT_PWRON_RTY_DFT;

	if (DRV_STS_POWER_OFF != gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT]) {
		WMT_ERR_FUNC("WMT-CORE: already powered on, WMT DRV_STS_[0x%x]\n",
			     gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT]);
		osal_assert(0);
		return -1;
	}
	/* TODO: [FixMe][GeorgeKuo]: clarify the following is reqiured or not! */
	if (pWmtOp->u4InfoBit & WMT_OP_HIF_BIT)
		opfunc_hif_conf(pWmtOp);

pwr_on_rty:
	/* power on control */
	ctrlPa1 = 0;
	ctrlPa2 = 0;
	iRet = wmt_core_ctrl(WMT_CTRL_HW_PWR_ON, &ctrlPa1, &ctrlPa2);
	if (iRet) {
		WMT_ERR_FUNC("WMT-CORE: WMT_CTRL_HW_PWR_ON fail iRet(%d)\n", iRet);
		if (0 == retry--) {
			WMT_INFO_FUNC("WMT-CORE: retry (%d)\n", retry);
			goto pwr_on_rty;
		}
		return -1;
	}
	gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT] = DRV_STS_POWER_ON;

	/* init stp */
	iRet = wmt_core_stp_init();
	if (iRet) {
		WMT_ERR_FUNC("WMT-CORE: wmt_core_stp_init fail (%d)\n", iRet);
		osal_assert(0);

		/* deinit stp */
		iRet = wmt_core_stp_deinit();
		iRet = opfunc_pwr_off(pWmtOp);
		if (iRet)
			WMT_ERR_FUNC("WMT-CORE: opfunc_pwr_off fail during pwr_on retry\n");

		if (0 < retry--) {
			WMT_INFO_FUNC("WMT-CORE: retry (%d)\n", retry);
			goto pwr_on_rty;
		}
		iRet = -2;
		return iRet;
	}

	WMT_DBG_FUNC("WMT-CORE: WMT [FUNC_ON]\n");
	gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT] = DRV_STS_FUNC_ON;

	/* What to do when state is changed from POWER_OFF to POWER_ON?
	 * 1. STP driver does s/w reset
	 * 2. UART does 0xFF wake up
	 * 3. SDIO does re-init command(changed to trigger by host)
	 */
	return iRet;

}

static INT32 opfunc_pwr_off(P_WMT_OP pWmtOp)
{

	INT32 iRet;
	unsigned long ctrlPa1;
	unsigned long ctrlPa2;

	if (DRV_STS_POWER_OFF == gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT]) {
		WMT_WARN_FUNC("WMT-CORE: WMT already off, WMT DRV_STS_[0x%x]\n",
			      gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT]);
		osal_assert(0);
		return -1;
	}
	if (MTK_WCN_BOOL_FALSE == g_pwr_off_flag) {
		WMT_WARN_FUNC("CONNSYS power off be disabled, maybe need trigger core dump!\n");
		osal_assert(0);
		return -2;
	}

	/* wmt and stp are initialized successfully */
	if (DRV_STS_FUNC_ON == gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT]) {
		iRet = wmt_core_stp_deinit();
		if (iRet) {
			WMT_WARN_FUNC("wmt_core_stp_deinit fail (%d)\n", iRet);
			/*should let run to power down chip */
		}
	}
	gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT] = DRV_STS_POWER_ON;

	/* power off control */
	ctrlPa1 = 0;
	ctrlPa2 = 0;
	iRet = wmt_core_ctrl(WMT_CTRL_HW_PWR_OFF, &ctrlPa1, &ctrlPa2);
	if (iRet)
		WMT_WARN_FUNC("HW_PWR_OFF fail (%d)\n", iRet);
	WMT_WARN_FUNC("HW_PWR_OFF ok\n");

	/*anyway, set to POWER_OFF state */
	gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT] = DRV_STS_POWER_OFF;
	return iRet;

}

static INT32 opfunc_func_on(P_WMT_OP pWmtOp)
{
	INT32 iRet = -1;
	INT32 iPwrOffRet = -1;
	UINT32 drvType;

	drvType = pWmtOp->au4OpData[0];
	m2note_wmt_trace_state("func_on_entry", drvType, 0);

	/* Check abnormal type */
	if (WMTDRV_TYPE_COREDUMP < drvType) {
		WMT_ERR_FUNC("abnormal Fun(%d)\n", drvType);
		osal_assert(0);
		return -1;
	}

	/* Check abnormal state */
	if ((DRV_STS_POWER_OFF > gMtkWmtCtx.eDrvStatus[drvType])
	    || (DRV_STS_MAX <= gMtkWmtCtx.eDrvStatus[drvType])) {
		WMT_ERR_FUNC("func(%d) status[0x%x] abnormal\n", drvType, gMtkWmtCtx.eDrvStatus[drvType]);
		osal_assert(0);
		return -2;
	}

	/* check if func already on */
	if (DRV_STS_FUNC_ON == gMtkWmtCtx.eDrvStatus[drvType]) {
		WMT_WARN_FUNC("func(%d) already on\n", drvType);
		return 0;
	}
	/*enable power off flag, if flag=0, power off connsys will not be executed */
	mtk_wcn_set_connsys_power_off_flag(MTK_WCN_BOOL_TRUE);
	/* check if chip power on is needed */
	if (DRV_STS_FUNC_ON != gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT]) {
		iRet = opfunc_pwr_on(pWmtOp);

			if (iRet) {
				WMT_ERR_FUNC("func(%d) pwr_on fail(%d)\n", drvType, iRet);
				osal_assert(0);
				m2note_wmt_trace_state("func_on_pwr_on_fail", drvType, iRet);

				/* check all sub-func and do power off */
				return -3;
		}
	}

	if (WMTDRV_TYPE_WMT > drvType) {
		if (NULL != gpWmtFuncOps[drvType] && NULL != gpWmtFuncOps[drvType]->func_on) {
			m2note_wmt_trace_state("func_on_before_cb", drvType, 0);
			iRet = (*(gpWmtFuncOps[drvType]->func_on)) (gMtkWmtCtx.p_ic_ops, wmt_conf_get_cfg());
			m2note_wmt_trace_state("func_on_after_cb", drvType, iRet);
			if (0 != iRet)
				gMtkWmtCtx.eDrvStatus[drvType] = DRV_STS_POWER_OFF;
			else
				gMtkWmtCtx.eDrvStatus[drvType] = DRV_STS_FUNC_ON;
		} else {
			WMT_WARN_FUNC("WMT-CORE: ops for type(%d) not found\n", drvType);
			iRet = -5;
		}
	} else {
		if (WMTDRV_TYPE_LPBK == drvType)
			gMtkWmtCtx.eDrvStatus[drvType] = DRV_STS_FUNC_ON;
		else if (WMTDRV_TYPE_COREDUMP == drvType)
			gMtkWmtCtx.eDrvStatus[drvType] = DRV_STS_FUNC_ON;
		iRet = 0;
	}

	if (iRet) {
		WMT_ERR_FUNC("WMT-CORE:type(0x%x) function on failed, ret(%d)\n", drvType, iRet);
		osal_assert(0);
		/* FIX-ME:[Chaozhong Liang], Error handling? check subsystem state and do pwr off if necessary? */
		/* check all sub-func and do power off */
		if ((DRV_STS_POWER_OFF == gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_BT]) &&
		    (DRV_STS_POWER_OFF == gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_GPS]) &&
		    (DRV_STS_POWER_OFF == gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_FM]) &&
		    (DRV_STS_POWER_OFF == gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WIFI]) &&
		    (DRV_STS_POWER_OFF == gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_LPBK]) &&
		    (DRV_STS_POWER_OFF == gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_COREDUMP])) {
			WMT_INFO_FUNC("WMT-CORE:Fun(%d) [POWER_OFF] and power down chip\n", drvType);
			mtk_wcn_wmt_system_state_reset();

			iPwrOffRet = opfunc_pwr_off(pWmtOp);
			if (iPwrOffRet) {
				WMT_ERR_FUNC("WMT-CORE: wmt_pwr_off fail(%d) when turn off func(%d)\n", iPwrOffRet,
					     drvType);
				osal_assert(0);
				}
			}
			m2note_wmt_trace_state("func_on_fail", drvType, iRet);
			return iRet;
		}

		wmt_core_dump_func_state("AF FUNC ON");
		m2note_wmt_trace_state("func_on_exit", drvType, iRet);

		return 0;
}

static INT32 opfunc_func_off(P_WMT_OP pWmtOp)
{

	INT32 iRet = -1;
	UINT32 drvType;

	drvType = pWmtOp->au4OpData[0];
	m2note_wmt_trace_state("func_off_entry", drvType, 0);
	/* Check abnormal type */
	if (WMTDRV_TYPE_COREDUMP < drvType) {
		WMT_ERR_FUNC("WMT-CORE: abnormal Fun(%d) in wmt_func_off\n", drvType);
		osal_assert(0);
		return -1;
	}

	/* Check abnormal state */
	if (DRV_STS_MAX <= gMtkWmtCtx.eDrvStatus[drvType]) {
		WMT_ERR_FUNC("WMT-CORE: Fun(%d) DRV_STS_[0x%x] abnormal in wmt_func_off\n",
			     drvType, gMtkWmtCtx.eDrvStatus[drvType]);
		osal_assert(0);
		return -2;
	}

	if (DRV_STS_FUNC_ON != gMtkWmtCtx.eDrvStatus[drvType]) {
		WMT_WARN_FUNC("WMT-CORE: Fun(%d) DRV_STS_[0x%x] already non-FUN_ON in wmt_func_off\n",
			      drvType, gMtkWmtCtx.eDrvStatus[drvType]);
		/* needs to check 4 subsystem's state? */
		m2note_wmt_trace_state("func_off_already_off", drvType, 0);
		return 0;
	} else if (WMTDRV_TYPE_WMT > drvType) {
		if (NULL != gpWmtFuncOps[drvType] && NULL != gpWmtFuncOps[drvType]->func_off) {
			m2note_wmt_trace_state("func_off_before_cb", drvType, 0);
			iRet = (*(gpWmtFuncOps[drvType]->func_off)) (gMtkWmtCtx.p_ic_ops, wmt_conf_get_cfg());
			m2note_wmt_trace_state("func_off_after_cb", drvType, iRet);
		} else {
			WMT_WARN_FUNC("WMT-CORE: ops for type(%d) not found\n", drvType);
			iRet = -3;
		}
	} else {
		if (WMTDRV_TYPE_LPBK == drvType)
			gMtkWmtCtx.eDrvStatus[drvType] = DRV_STS_POWER_OFF;
		else if (WMTDRV_TYPE_COREDUMP == drvType)
			gMtkWmtCtx.eDrvStatus[drvType] = DRV_STS_POWER_OFF;
		iRet = 0;
	}

	/* shall we put device state to POWER_OFF state when fail? */
	gMtkWmtCtx.eDrvStatus[drvType] = DRV_STS_POWER_OFF;

	if (iRet) {
		WMT_ERR_FUNC("WMT-CORE: type(0x%x) function off failed, ret(%d)\n", drvType, iRet);
		osal_assert(0);
		/* no matter subsystem function control fail or not,
		*chip should be powered off when no subsystem is active
		*/
		/* return iRet; */
	}

	/* check all sub-func and do power off */
	if ((DRV_STS_POWER_OFF == gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_BT]) &&
	    (DRV_STS_POWER_OFF == gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_GPS]) &&
	    (DRV_STS_POWER_OFF == gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_FM]) &&
	    (DRV_STS_POWER_OFF == gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WIFI]) &&
	    (DRV_STS_POWER_OFF == gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_LPBK]) &&
	    (DRV_STS_POWER_OFF == gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_COREDUMP])) {
		WMT_INFO_FUNC("WMT-CORE:Fun(%d) [POWER_OFF] and power down chip\n", drvType);

		iRet = opfunc_pwr_off(pWmtOp);
		if (iRet) {
			WMT_ERR_FUNC("WMT-CORE: wmt_pwr_off fail(%d) when turn off func(%d)\n", iRet, drvType);
			osal_assert(0);
		}
	}

	wmt_core_dump_func_state("AF FUNC OFF");
	m2note_wmt_trace_state("func_off_exit", drvType, iRet);
	return iRet;
}

/* TODO:[ChangeFeature][George] is this OP obsoleted? */
static INT32 opfunc_reg_rw(P_WMT_OP pWmtOp)
{
	INT32 iret;

	if (DRV_STS_FUNC_ON != gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT]) {
		WMT_ERR_FUNC("reg_rw when WMT is powered off\n");
		return -1;
	}
	iret = wmt_core_reg_rw_raw(pWmtOp->au4OpData[0],
				   pWmtOp->au4OpData[1], (PUINT32) pWmtOp->au4OpData[2], pWmtOp->au4OpData[3]);

	return iret;
}

static INT32 opfunc_exit(P_WMT_OP pWmtOp)
{
	/* TODO: [FixMe][George] is ok to leave this function empty??? */
	WMT_WARN_FUNC("EMPTY FUNCTION\n");
	return 0;
}

static INT32 opfunc_pwr_sv(P_WMT_OP pWmtOp)
{
	INT32 ret = -1;
	UINT32 u4_result = 0;
	UINT32 evt_len;
	UINT8 evt_buf[16] = { 0 };
	unsigned long ctrlPa1 = 0;
	unsigned long ctrlPa2 = 0;

	typedef INT32(*STP_PSM_CB) (INT32);
	STP_PSM_CB psm_cb = NULL;

	if (SLEEP == pWmtOp->au4OpData[0]) {
		WMT_DBG_FUNC("**** Send sleep command\n");
		/* mtk_wcn_stp_set_psm_state(ACT_INACT); */
		/* (*kal_stp_flush_rx)(WMT_TASK_INDX); */
		ret = wmt_core_tx((PUINT8) &WMT_SLEEP_CMD[0], sizeof(WMT_SLEEP_CMD), &u4_result, 0);
		if (ret || (u4_result != sizeof(WMT_SLEEP_CMD))) {
			WMT_ERR_FUNC("wmt_core: SLEEP_CMD ret(%d) cmd len err(%d, %d) ", ret, u4_result,
				     sizeof(WMT_SLEEP_CMD));
			goto pwr_sv_done;
		}

		evt_len = sizeof(WMT_SLEEP_EVT);
		ret = wmt_core_rx(evt_buf, evt_len, &u4_result);
		if (ret || (u4_result != evt_len)) {
			unsigned long type = WMTDRV_TYPE_WMT;
			unsigned long reason = 33;
			unsigned long ctrlpa = 1;

			wmt_core_rx_flush(WMT_TASK_INDX);
			WMT_ERR_FUNC("wmt_core: read SLEEP_EVT fail(%d) len(%d, %d)", ret, u4_result, evt_len);
			mtk_wcn_stp_dbg_dump_package();
			ret = wmt_core_ctrl(WMT_CTRL_EVT_PARSER, &ctrlpa, 0);
			if (!ret) {	/* parser ok */
				reason = 38;	/* host schedule issue reason code */
				WMT_WARN_FUNC("This evt error may be caused by system schedule issue\n");
			}
			wmt_core_ctrl(WMT_CTRL_EVT_ERR_TRG_ASSERT, &type, &reason);
			goto pwr_sv_done;
		}

		if (osal_memcmp(evt_buf, WMT_SLEEP_EVT, sizeof(WMT_SLEEP_EVT)) != 0) {
			WMT_ERR_FUNC("wmt_core: compare WMT_SLEEP_EVT error\n");
			wmt_core_rx_flush(WMT_TASK_INDX);
			WMT_ERR_FUNC("wmt_core: rx(%d):[%02X,%02X,%02X,%02X,%02X,%02X]\n",
				u4_result,
				evt_buf[0],
				evt_buf[1],
				evt_buf[2],
				evt_buf[3],
				evt_buf[4],
				evt_buf[5]);
			WMT_ERR_FUNC("wmt_core: exp(%d):[%02X,%02X,%02X,%02X,%02X,%02X]\n",
				sizeof(WMT_SLEEP_EVT),
				WMT_SLEEP_EVT[0],
				WMT_SLEEP_EVT[1],
				WMT_SLEEP_EVT[2],
			    WMT_SLEEP_EVT[3],
			    WMT_SLEEP_EVT[4],
			    WMT_SLEEP_EVT[5]);
			mtk_wcn_stp_dbg_dump_package();
			goto pwr_sv_done;
		} else {
			WMT_DBG_FUNC("Send sleep command OK!\n");
		}
	} else if (pWmtOp->au4OpData[0] == WAKEUP) {
		WMT_DBG_FUNC("wakeup connsys by btif");

		ret = wmt_core_ctrl(WMT_CTRL_SOC_WAKEUP_CONSYS, &ctrlPa1, &ctrlPa2);
		if (ret) {
			WMT_ERR_FUNC("wmt-core:WAKEUP_CONSYS by BTIF fail(%d)", ret);
			goto pwr_sv_done;
		}
#if 0
		WMT_DBG_FUNC("**** Send wakeup command\n");
		ret = wmt_core_tx(WMT_WAKEUP_CMD, sizeof(WMT_WAKEUP_CMD), &u4_result, 1);

		if (ret || (u4_result != sizeof(WMT_WAKEUP_CMD))) {
			wmt_core_rx_flush(WMT_TASK_INDX);
			WMT_ERR_FUNC("wmt_core: WAKEUP_CMD ret(%d) cmd len err(%d, %d) ", ret, u4_result,
				     sizeof(WMT_WAKEUP_CMD));
			goto pwr_sv_done;
		}
#endif
		evt_len = sizeof(WMT_WAKEUP_EVT);
		ret = wmt_core_rx(evt_buf, evt_len, &u4_result);
		if (ret || (u4_result != evt_len)) {
			unsigned long type = WMTDRV_TYPE_WMT;
			unsigned long reason = 34;
			unsigned long ctrlpa = 2;

			WMT_ERR_FUNC("wmt_core: read WAKEUP_EVT fail(%d) len(%d, %d)", ret, u4_result, evt_len);
			mtk_wcn_stp_dbg_dump_package();
			ret = wmt_core_ctrl(WMT_CTRL_EVT_PARSER, &ctrlpa, 0);
			if (!ret) {	/* parser ok */
				reason = 39;	/* host schedule issue reason code */
				WMT_WARN_FUNC("This evt error may be caused by system schedule issue\n");
			}
			wmt_core_ctrl(WMT_CTRL_EVT_ERR_TRG_ASSERT, &type, &reason);
			goto pwr_sv_done;
		}

		if (osal_memcmp(evt_buf, WMT_WAKEUP_EVT, sizeof(WMT_WAKEUP_EVT)) != 0) {
			WMT_ERR_FUNC("wmt_core: compare WMT_WAKEUP_EVT error\n");
			wmt_core_rx_flush(WMT_TASK_INDX);
			WMT_ERR_FUNC("wmt_core: rx(%d):[%02X,%02X,%02X,%02X,%02X,%02X]\n",
				u4_result,
				evt_buf[0],
				evt_buf[1],
				evt_buf[2],
				evt_buf[3],
				evt_buf[4],
				evt_buf[5]);
			WMT_ERR_FUNC("wmt_core: exp(%d):[%02X,%02X,%02X,%02X,%02X,%02X]\n",
				sizeof(WMT_WAKEUP_EVT),
				WMT_WAKEUP_EVT[0],
				WMT_WAKEUP_EVT[1],
				WMT_WAKEUP_EVT[2],
			    WMT_WAKEUP_EVT[3],
			    WMT_WAKEUP_EVT[4],
			    WMT_WAKEUP_EVT[5]);
			mtk_wcn_stp_dbg_dump_package();
			goto pwr_sv_done;
		} else {
			WMT_DBG_FUNC("Send wakeup command OK!\n");
		}
	} else if (pWmtOp->au4OpData[0] == HOST_AWAKE) {

		WMT_DBG_FUNC("**** Send host awake command\n");

		psm_cb = (STP_PSM_CB) pWmtOp->au4OpData[1];
		/* (*kal_stp_flush_rx)(WMT_TASK_INDX); */
		ret = wmt_core_tx((PUINT8) WMT_HOST_AWAKE_CMD, sizeof(WMT_HOST_AWAKE_CMD), &u4_result, 0);
		if (ret || (u4_result != sizeof(WMT_HOST_AWAKE_CMD))) {
			WMT_ERR_FUNC("wmt_core: HOST_AWAKE_CMD ret(%d) cmd len err(%d, %d) ", ret, u4_result,
				     sizeof(WMT_HOST_AWAKE_CMD));
			goto pwr_sv_done;
		}

		evt_len = sizeof(WMT_HOST_AWAKE_EVT);
		ret = wmt_core_rx(evt_buf, evt_len, &u4_result);
		if (ret || (u4_result != evt_len)) {
			unsigned long type = WMTDRV_TYPE_WMT;
			unsigned long reason = 35;
			unsigned long ctrlpa = 3;

			wmt_core_rx_flush(WMT_TASK_INDX);
			WMT_ERR_FUNC("wmt_core: read HOST_AWAKE_EVT fail(%d) len(%d, %d)", ret, u4_result, evt_len);
			mtk_wcn_stp_dbg_dump_package();
			ret = wmt_core_ctrl(WMT_CTRL_EVT_PARSER, &ctrlpa, 0);
			if (!ret) {	/* parser ok */
				reason = 40;	/* host schedule issue reason code */
				WMT_WARN_FUNC("This evt error may be caused by system schedule issue\n");
			}
			wmt_core_ctrl(WMT_CTRL_EVT_ERR_TRG_ASSERT, &type, &reason);
			goto pwr_sv_done;
		}

		if (osal_memcmp(evt_buf, WMT_HOST_AWAKE_EVT, sizeof(WMT_HOST_AWAKE_EVT)) != 0) {
			WMT_ERR_FUNC("wmt_core: compare WMT_HOST_AWAKE_EVT error\n");
			wmt_core_rx_flush(WMT_TASK_INDX);
			WMT_ERR_FUNC("wmt_core: rx(%d):[%02X,%02X,%02X,%02X,%02X,%02X]\n",
				u4_result,
				evt_buf[0],
				evt_buf[1],
				evt_buf[2],
				evt_buf[3],
				evt_buf[4],
				evt_buf[5]);
			WMT_ERR_FUNC("wmt_core: exp(%d):[%02X,%02X,%02X,%02X,%02X,%02X]\n",
				sizeof(WMT_HOST_AWAKE_EVT),
				WMT_HOST_AWAKE_EVT[0],
				WMT_HOST_AWAKE_EVT[1],
				WMT_HOST_AWAKE_EVT[2],
			    WMT_HOST_AWAKE_EVT[3],
			    WMT_HOST_AWAKE_EVT[4],
			    WMT_HOST_AWAKE_EVT[5]);
			mtk_wcn_stp_dbg_dump_package();
			/* goto pwr_sv_done; */
		} else {
			WMT_DBG_FUNC("Send host awake command OK!\n");
		}
	}
pwr_sv_done:

	if (pWmtOp->au4OpData[0] < STP_PSM_MAX_ACTION) {
		psm_cb = (STP_PSM_CB) pWmtOp->au4OpData[1];
		WMT_DBG_FUNC("Do STP-CB! %d %p / %p\n", pWmtOp->au4OpData[0], (PVOID) pWmtOp->au4OpData[1],
			     (PVOID) psm_cb);
		if (NULL != psm_cb) {
			psm_cb(pWmtOp->au4OpData[0]);
		} else {
			WMT_ERR_FUNC("fatal error !!!, psm_cb = %p, god, someone must have corrupted our memory.\n",
				     psm_cb);
		}
	}

	return ret;
}

static INT32 opfunc_dsns(P_WMT_OP pWmtOp)
{

	INT32 iRet = -1;
	UINT32 u4Res;
	UINT32 evtLen;
	UINT8 evtBuf[16] = { 0 };

	WMT_DSNS_CMD[4] = (UINT8) pWmtOp->au4OpData[0];
	WMT_DSNS_CMD[5] = (UINT8) pWmtOp->au4OpData[1];

	/* send command */
	/* iRet = (*kal_stp_tx)(WMT_DSNS_CMD, osal_sizeof(WMT_DSNS_CMD), &u4Res); */
	iRet = wmt_core_tx((PUINT8) WMT_DSNS_CMD, osal_sizeof(WMT_DSNS_CMD), &u4Res, MTK_WCN_BOOL_FALSE);
	if (iRet || (u4Res != osal_sizeof(WMT_DSNS_CMD))) {
		WMT_ERR_FUNC("WMT-CORE: DSNS_CMD iRet(%d) cmd len err(%d, %d)\n", iRet, u4Res,
			     osal_sizeof(WMT_DSNS_CMD));
		return iRet;
	}

	evtLen = osal_sizeof(WMT_DSNS_EVT);

	iRet = wmt_core_rx(evtBuf, evtLen, &u4Res);
	if (iRet || (u4Res != evtLen)) {
		WMT_ERR_FUNC("WMT-CORE: read DSNS_EVT fail(%d) len(%d, %d)\n", iRet, u4Res, evtLen);
		mtk_wcn_stp_dbg_dump_package();
		return iRet;
	}

	if (osal_memcmp(evtBuf, WMT_DSNS_EVT, osal_sizeof(WMT_DSNS_EVT)) != 0) {
		WMT_ERR_FUNC("WMT-CORE: compare WMT_DSNS_EVT error\n");
		WMT_ERR_FUNC("WMT-CORE: rx(%d):[%02X,%02X,%02X,%02X,%02X] exp(%d):[%02X,%02X,%02X,%02X,%02X]\n",
			     u4Res, evtBuf[0], evtBuf[1], evtBuf[2], evtBuf[3], evtBuf[4],
			     osal_sizeof(WMT_DSNS_EVT), WMT_DSNS_EVT[0], WMT_DSNS_EVT[1], WMT_DSNS_EVT[2],
			     WMT_DSNS_EVT[3], WMT_DSNS_EVT[4]);
	} else {
		WMT_INFO_FUNC("Send WMT_DSNS_CMD command OK!\n");
	}

	return iRet;
}

#if CFG_CORE_INTERNAL_TXRX
INT32 wmt_core_lpbk_do_stp_init(void)
{
	INT32 iRet = 0;
	unsigned long ctrlPa1 = 0;
	unsigned long ctrlPa2 = 0;

	ctrlPa1 = 0;
	ctrlPa2 = 0;
	iRet = wmt_core_ctrl(WMT_CTRL_STP_OPEN, &ctrlPa1, &ctrlPa2);
	if (iRet) {
		WMT_ERR_FUNC("WMT-CORE: wmt open stp\n");
		return -1;
	}

	ctrlPa1 = WMT_STP_CONF_MODE;
	ctrlPa2 = MTKSTP_BTIF_MAND_MODE;
	iRet += wmt_core_ctrl(WMT_CTRL_STP_CONF, &ctrlPa1, &ctrlPa2);

	ctrlPa1 = WMT_STP_CONF_EN;
	ctrlPa2 = 1;
	iRet += wmt_core_ctrl(WMT_CTRL_STP_CONF, &ctrlPa1, &ctrlPa2);
	if (iRet) {
		WMT_ERR_FUNC("WMT-CORE: stp_init <1><2> fail:%d\n", iRet);
		return -2;
	}
}

INT32 wmt_core_lpbk_do_stp_deinit(void)
{
	INT32 iRet = 0;
	unsigned long ctrlPa1 = 0;
	unsigned long ctrlPa2 = 0;

	ctrlPa1 = 0;
	ctrlPa2 = 0;
	iRet = wmt_core_ctrl(WMT_CTRL_STP_CLOSE, &ctrlPa1, &ctrlPa2);
	if (iRet) {
		WMT_ERR_FUNC("WMT-CORE: wmt open stp\n");
		return -1;
	}

	return 0;
}
#endif
static INT32 opfunc_lpbk(P_WMT_OP pWmtOp)
{

	INT32 iRet;
	UINT32 u4WrittenSize = 0;
	UINT32 u4ReadSize = 0;
	UINT32 buf_length = 0;
	UINT32 *pbuffer = NULL;
	UINT16 len_in_cmd;

	/* UINT32 offset; */
	UINT8 WMT_TEST_LPBK_CMD[] = { 0x1, 0x2, 0x0, 0x0, 0x7 };
	UINT8 WMT_TEST_LPBK_EVT[] = { 0x2, 0x2, 0x0, 0x0, 0x0 };

	/* UINT8 lpbk_buf[1024 + 5] = {0}; */
	MTK_WCN_BOOL fgFail;

	buf_length = pWmtOp->au4OpData[0];	/* packet length */
	pbuffer = (VOID *) pWmtOp->au4OpData[1];	/* packet buffer pointer */
	WMT_DBG_FUNC("WMT-CORE: -->wmt_do_lpbk\n");

#if 0
	osal_memcpy(&WMT_TEST_LPBK_EVT[0], &WMT_TEST_LPBK_CMD[0], osal_sizeof(WMT_TEST_LPBK_CMD));
#endif
#if !CFG_CORE_INTERNAL_TXRX
	/*check if WMTDRV_TYPE_LPBK function is already on */
	if (DRV_STS_FUNC_ON != gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_LPBK]
	    || buf_length + osal_sizeof(WMT_TEST_LPBK_CMD) > osal_sizeof(gLpbkBuf)) {
		WMT_ERR_FUNC("WMT-CORE: abnormal LPBK in wmt_do_lpbk\n");
		osal_assert(0);
		return -2;
	}
#endif
	/*package loopback for STP */

	/* init buffer */
	osal_memset(gLpbkBuf, 0, osal_sizeof(gLpbkBuf));

	len_in_cmd = buf_length + 1;	/* add flag field */

	osal_memcpy(&WMT_TEST_LPBK_CMD[2], &len_in_cmd, 2);
	osal_memcpy(&WMT_TEST_LPBK_EVT[2], &len_in_cmd, 2);

	/* wmt cmd */
	osal_memcpy(gLpbkBuf, WMT_TEST_LPBK_CMD, osal_sizeof(WMT_TEST_LPBK_CMD));
	osal_memcpy(gLpbkBuf + osal_sizeof(WMT_TEST_LPBK_CMD), pbuffer, buf_length);

	do {
		fgFail = MTK_WCN_BOOL_TRUE;
		/*send packet through STP */

		/* iRet = (*kal_stp_tx)(
		*(PUINT8)gLpbkBuf,
		*osal_sizeof(WMT_TEST_LPBK_CMD) + buf_length,
		*&u4WrittenSize);
		*/
		iRet = wmt_core_tx((PUINT8) gLpbkBuf,
				(osal_sizeof(WMT_TEST_LPBK_CMD) + buf_length),
				&u4WrittenSize,
				MTK_WCN_BOOL_FALSE);
		if (iRet) {
			WMT_ERR_FUNC("opfunc_lpbk wmt_core_tx failed\n");
			break;
		}
		WMT_INFO_FUNC("opfunc_lpbk wmt_core_tx OK\n");

		/*receive firmware response from STP */
		iRet = wmt_core_rx((PUINT8) gLpbkBuf, (osal_sizeof(WMT_TEST_LPBK_EVT) + buf_length), &u4ReadSize);
		if (iRet) {
			WMT_ERR_FUNC("opfunc_lpbk wmt_core_rx failed\n");
			break;
		}
		WMT_INFO_FUNC("opfunc_lpbk wmt_core_rx  OK\n");
		/*check if loopback response ok or not */
		if (u4ReadSize != (osal_sizeof(WMT_TEST_LPBK_CMD) + buf_length)) {
			WMT_ERR_FUNC("lpbk event read size wrong(%d, %d)\n", u4ReadSize,
				     (osal_sizeof(WMT_TEST_LPBK_CMD) + buf_length));
			break;
		}
		WMT_INFO_FUNC("lpbk event read size right(%d, %d)\n", u4ReadSize,
			      (osal_sizeof(WMT_TEST_LPBK_CMD) + buf_length));

		if (osal_memcmp(WMT_TEST_LPBK_EVT, gLpbkBuf, osal_sizeof(WMT_TEST_LPBK_EVT))) {
			WMT_ERR_FUNC("WMT-CORE WMT_TEST_LPBK_EVT error! read len %d [%02x,%02x,%02x,%02x,%02x]\n",
				     (INT32) u4ReadSize, gLpbkBuf[0], gLpbkBuf[1], gLpbkBuf[2], gLpbkBuf[3], gLpbkBuf[4]
			    );
			break;
		}
		pWmtOp->au4OpData[0] = u4ReadSize - osal_sizeof(WMT_TEST_LPBK_EVT);
		osal_memcpy((VOID *) pWmtOp->au4OpData[1], gLpbkBuf + osal_sizeof(WMT_TEST_LPBK_CMD), buf_length);
		fgFail = MTK_WCN_BOOL_FALSE;
	} while (0);
	/*return result */
	/* WMT_DBG_FUNC("WMT-CORE: <--wmt_do_lpbk, fgFail = %d\n", fgFail); */
	return fgFail;

}

static INT32 opfunc_cmd_test(P_WMT_OP pWmtOp)
{

	INT32 iRet = 0;
	UINT32 cmdNo = 0;
	UINT32 cmdNoPa = 0;

	UINT8 tstCmd[64];
	UINT8 tstEvt[64];
	UINT8 tstEvtTmp[64];
	UINT32 u4Res;
	SIZE_T tstCmdSz = 0;
	SIZE_T tstEvtSz = 0;

	UINT8 *pRes = NULL;
	UINT32 resBufRoom = 0;
	/*test command list */
	/*1 */
	UINT8 WMT_ASSERT_CMD[] = { 0x01, 0x02, 0x01, 0x00, 0x08 };
	UINT8 WMT_ASSERT_EVT[] = { 0x02, 0x02, 0x00, 0x00, 0x00 };
	UINT8 WMT_NOACK_CMD[] = { 0x01, 0x02, 0x01, 0x00, 0x0A };
	UINT8 WMT_NOACK_EVT[] = { 0x02, 0x02, 0x00, 0x00, 0x00 };
	UINT8 WMT_WARNRST_CMD[] = { 0x01, 0x02, 0x01, 0x00, 0x0B };
	UINT8 WMT_WARNRST_EVT[] = { 0x02, 0x02, 0x00, 0x00, 0x00 };
	UINT8 WMT_FWLOGTST_CMD[] = { 0x01, 0x02, 0x01, 0x00, 0x0C };
	UINT8 WMT_FWLOGTST_EVT[] = { 0x02, 0x02, 0x00, 0x00, 0x00 };

	UINT8 WMT_EXCEPTION_CMD[] = { 0x01, 0x02, 0x01, 0x00, 0x09 };
	UINT8 WMT_EXCEPTION_EVT[] = { 0x02, 0x02, 0x00, 0x00, 0x00 };
	/*2 */
	UINT8 WMT_COEXDBG_CMD[] = { 0x01, 0x10, 0x02, 0x00,
		0x08,
		0xAA		/*Debugging Parameter */
	};
	UINT8 WMT_COEXDBG_1_EVT[] = { 0x02, 0x10, 0x05, 0x00,
		0x00,
		0xAA, 0xAA, 0xAA, 0xAA	/*event content */
	};
	UINT8 WMT_COEXDBG_2_EVT[] = { 0x02, 0x10, 0x07, 0x00,
		0x00,
		0xAA, 0xAA, 0xAA, 0xAA, 0xBB, 0xBB	/*event content */
	};
	UINT8 WMT_COEXDBG_3_EVT[] = { 0x02, 0x10, 0x0B, 0x00,
		0x00,
		0xAA, 0xAA, 0xAA, 0xAA, 0xBB, 0xBB, 0xBB, 0xBB	/*event content */
	};
	/*test command list -end */

	cmdNo = pWmtOp->au4OpData[0];

	WMT_INFO_FUNC("Send Test command %d!\n", cmdNo);
	if (cmdNo == 0) {
		/*dead command */
		WMT_INFO_FUNC("Send Assert command !\n");
		tstCmdSz = osal_sizeof(WMT_ASSERT_CMD);
		tstEvtSz = osal_sizeof(WMT_ASSERT_EVT);
		osal_memcpy(tstCmd, WMT_ASSERT_CMD, tstCmdSz);
		osal_memcpy(tstEvt, WMT_ASSERT_EVT, tstEvtSz);
	} else if (cmdNo == 1) {
		/*dead command */
		WMT_INFO_FUNC("Send Exception command !\n");
		tstCmdSz = osal_sizeof(WMT_EXCEPTION_CMD);
		tstEvtSz = osal_sizeof(WMT_EXCEPTION_EVT);
		osal_memcpy(tstCmd, WMT_EXCEPTION_CMD, tstCmdSz);
		osal_memcpy(tstEvt, WMT_EXCEPTION_EVT, tstEvtSz);
	} else if (cmdNo == 2) {
		cmdNoPa = pWmtOp->au4OpData[1];
		pRes = (PUINT8) pWmtOp->au4OpData[2];
		resBufRoom = pWmtOp->au4OpData[3];
		if (cmdNoPa <= 0xf) {
			WMT_INFO_FUNC("Send Coexistence Debug command [0x%x]!\n", cmdNoPa);
			tstCmdSz = osal_sizeof(WMT_COEXDBG_CMD);
			osal_memcpy(tstCmd, WMT_COEXDBG_CMD, tstCmdSz);
			if (tstCmdSz > 5)
				tstCmd[5] = cmdNoPa;

			/*setup the expected event length */
			if (cmdNoPa <= 0x4) {
				tstEvtSz = osal_sizeof(WMT_COEXDBG_1_EVT);
				osal_memcpy(tstEvt, WMT_COEXDBG_1_EVT, tstEvtSz);
			} else if (cmdNoPa == 0x5) {
				tstEvtSz = osal_sizeof(WMT_COEXDBG_2_EVT);
				osal_memcpy(tstEvt, WMT_COEXDBG_2_EVT, tstEvtSz);
			} else if (cmdNoPa >= 0x6 && cmdNoPa <= 0xf) {
				tstEvtSz = osal_sizeof(WMT_COEXDBG_3_EVT);
				osal_memcpy(tstEvt, WMT_COEXDBG_3_EVT, tstEvtSz);
			} else {

			}
		} else {
			WMT_ERR_FUNC("cmdNoPa is wrong\n");
			return iRet;
		}
	} else if (cmdNo == 3) {
		/*dead command */
		WMT_INFO_FUNC("Send No Ack command !\n");
		tstCmdSz = osal_sizeof(WMT_NOACK_CMD);
		tstEvtSz = osal_sizeof(WMT_NOACK_EVT);
		osal_memcpy(tstCmd, WMT_NOACK_CMD, tstCmdSz);
		osal_memcpy(tstEvt, WMT_NOACK_EVT, tstEvtSz);
	} else if (cmdNo == 4) {
		/*dead command */
		WMT_INFO_FUNC("Send Warm reset command !\n");
		tstCmdSz = osal_sizeof(WMT_WARNRST_CMD);
		tstEvtSz = osal_sizeof(WMT_WARNRST_EVT);
		osal_memcpy(tstCmd, WMT_WARNRST_CMD, tstCmdSz);
		osal_memcpy(tstEvt, WMT_WARNRST_EVT, tstEvtSz);
	} else if (cmdNo == 5) {
		/*dead command */
		WMT_INFO_FUNC("Send f/w log test command !\n");
		tstCmdSz = osal_sizeof(WMT_FWLOGTST_CMD);
		tstEvtSz = osal_sizeof(WMT_FWLOGTST_EVT);
		osal_memcpy(tstCmd, WMT_FWLOGTST_CMD, tstCmdSz);
		osal_memcpy(tstEvt, WMT_FWLOGTST_EVT, tstEvtSz);
	}

	else {
		/*Placed youer test WMT command here, easiler to integrate and test with F/W side */
	}

	/* send command */
	/* iRet = (*kal_stp_tx)(tstCmd, tstCmdSz, &u4Res); */
	iRet = wmt_core_tx((PUINT8) tstCmd, tstCmdSz, &u4Res, MTK_WCN_BOOL_FALSE);
	if (iRet || (u4Res != tstCmdSz)) {
		WMT_ERR_FUNC("WMT-CORE: wmt_cmd_test iRet(%d) cmd len err(%d, %d)\n", iRet, u4Res, tstCmdSz);
		return -1;
	}

	if ((cmdNo == 0) || (cmdNo == 1) || cmdNo == 3) {
		WMT_INFO_FUNC("WMT-CORE: not to rx event for assert command\n");
		return 0;
	}

	iRet = wmt_core_rx(tstEvtTmp, tstEvtSz, &u4Res);

	/*Event Post Handling */
	if (cmdNo == 2) {
		WMT_INFO_FUNC("#=========================================================#\n");
		WMT_INFO_FUNC("coext debugging id = %d", cmdNoPa);
		if (tstEvtSz > 5) {
			wmt_core_dump_data(&tstEvtTmp[5], "coex debugging ", tstEvtSz - 5);
		} else {
			/* error log */
			WMT_ERR_FUNC("error coex debugging event\n");
		}
		/*put response to buffer for shell to read */
		if (pRes != NULL && resBufRoom > 0) {
			pWmtOp->au4OpData[3] = resBufRoom < tstEvtSz - 5 ? resBufRoom : tstEvtSz - 5;
			osal_memcpy(pRes, &tstEvtTmp[5], pWmtOp->au4OpData[3]);
		} else
			pWmtOp->au4OpData[3] = 0;
		WMT_INFO_FUNC("#=========================================================#\n");
	}

	return iRet;

}

static INT32 opfunc_hw_rst(P_WMT_OP pWmtOp)
{

	INT32 iRet = -1;
	unsigned long ctrlPa1;
	unsigned long ctrlPa2;

	wmt_core_dump_func_state("BE HW RST");
	m2note_wmt_trace_state("hw_rst_entry", WMTDRV_TYPE_WMT, 0);
    /*-->Reset WMT  data structure*/
	/* gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_BT]   = DRV_STS_POWER_OFF; */
	gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_FM] = DRV_STS_POWER_OFF;
	gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_GPS] = DRV_STS_POWER_OFF;
	/* gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WIFI] = DRV_STS_POWER_OFF; */
	gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_LPBK] = DRV_STS_POWER_OFF;
	gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_STP] = DRV_STS_POWER_OFF;
	gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_COREDUMP] = DRV_STS_POWER_OFF;
	/*enable power off flag, if flag=0, power off connsys will not be executed */
	mtk_wcn_set_connsys_power_off_flag(MTK_WCN_BOOL_TRUE);
	/* if wmt is poweroff, we need poweron chip first */
	/* Zhiguo : this action also needed in BTIF interface to avoid KE */
#if 1
	if (DRV_STS_POWER_OFF == gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT]) {
		WMT_WARN_FUNC("WMT-CORE: WMT is off, need re-poweron\n");
		/* power on control */
		ctrlPa1 = 0;
		ctrlPa2 = 0;
		iRet = wmt_core_ctrl(WMT_CTRL_HW_PWR_ON, &ctrlPa1, &ctrlPa2);
		if (iRet) {
			WMT_ERR_FUNC("WMT-CORE: WMT_CTRL_HW_PWR_ON fail iRet(%d)\n", iRet);
			m2note_wmt_trace_state("hw_rst_pwr_on_fail", WMTDRV_TYPE_WMT, iRet);
			return -1;
		}
		gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT] = DRV_STS_POWER_ON;
		m2note_wmt_trace_state("hw_rst_pwr_on_after", WMTDRV_TYPE_WMT, iRet);
	}
#endif
	if (gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_BT] == DRV_STS_FUNC_ON) {

		m2note_wmt_trace_state("hw_rst_bt_off_before", WMTDRV_TYPE_BT, 0);
		ctrlPa1 = BT_PALDO;
		ctrlPa2 = PALDO_OFF;
		iRet = wmt_core_ctrl(WMT_CTRL_SOC_PALDO_CTRL, &ctrlPa1, &ctrlPa2);
		if (iRet)
			WMT_ERR_FUNC("WMT-CORE: wmt_ctrl_soc_paldo_ctrl failed(%d)(%d)(%d)\n", iRet, ctrlPa1, ctrlPa2);

		gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_BT] = DRV_STS_POWER_OFF;
		m2note_wmt_trace_state("hw_rst_bt_off_after", WMTDRV_TYPE_BT, iRet);
	}

	if (gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WIFI] == DRV_STS_FUNC_ON) {

		m2note_wmt_trace_state("hw_rst_wifi_off_before", WMTDRV_TYPE_WIFI, 0);
		if (NULL != gpWmtFuncOps[WMTDRV_TYPE_WIFI] && NULL != gpWmtFuncOps[WMTDRV_TYPE_WIFI]->func_off) {
			iRet = gpWmtFuncOps[WMTDRV_TYPE_WIFI]->func_off(gMtkWmtCtx.p_ic_ops, wmt_conf_get_cfg());
			if (iRet) {
				WMT_ERR_FUNC("WMT-CORE: turn off WIFI func fail (%d)\n", iRet);

				/* check all sub-func and do power off */
			} else {
				WMT_INFO_FUNC("wmt core: turn off  WIFI func ok!!\n");
			}
		}
		gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WIFI] = DRV_STS_POWER_OFF;
		m2note_wmt_trace_state("hw_rst_wifi_off_after", WMTDRV_TYPE_WIFI, iRet);
	}
#if 0
	/*<4>Power off Combo chip */
	ctrlPa1 = 0;
	ctrlPa2 = 0;
	iRet = wmt_core_ctrl(WMT_CTRL_HW_RST, &ctrlPa1, &ctrlPa2);
	if (iRet)
		WMT_ERR_FUNC("WMT-CORE: [HW RST] WMT_CTRL_POWER_OFF fail (%d)", iRet);
	WMT_INFO_FUNC("WMT-CORE: [HW RST] WMT_CTRL_POWER_OFF ok (%d)", iRet);
#endif
	gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT] = DRV_STS_POWER_OFF;
	m2note_wmt_trace_state("hw_rst_ctrl_before", WMTDRV_TYPE_WMT, 0);

    /*-->PesetCombo chip*/
	iRet = wmt_core_ctrl(WMT_CTRL_HW_RST, &ctrlPa1, &ctrlPa2);
	if (iRet)
		WMT_ERR_FUNC("WMT-CORE: -->[HW RST] fail iRet(%d)\n", iRet);
	WMT_WARN_FUNC("WMT-CORE: -->[HW RST] ok\n");

	gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT] = DRV_STS_POWER_ON;
	m2note_wmt_trace_state("hw_rst_ctrl_after", WMTDRV_TYPE_WMT, iRet);

	/* 4  close stp */
	ctrlPa1 = 0;
	ctrlPa2 = 0;
	iRet = wmt_core_ctrl(WMT_CTRL_STP_CLOSE, &ctrlPa1, &ctrlPa2);
	if (iRet) {
		if (iRet == -2) {
			WMT_INFO_FUNC("WMT-CORE:stp should have be closed\n");
			m2note_wmt_trace_state("hw_rst_stp_already_closed", WMTDRV_TYPE_STP, iRet);
			return 0;
		}
		WMT_ERR_FUNC("WMT-CORE: wmt close stp failed\n");
		m2note_wmt_trace_state("hw_rst_stp_close_fail", WMTDRV_TYPE_STP, iRet);
		return -1;
	}

	wmt_core_dump_func_state("AF HW RST");
	m2note_wmt_trace_state("hw_rst_exit", WMTDRV_TYPE_WMT, iRet);
	return iRet;

}

static INT32 opfunc_sw_rst(P_WMT_OP pWmtOp)
{
	INT32 iRet = 0;

	iRet = wmt_core_stp_init();
	if (!iRet)
		gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT] = DRV_STS_FUNC_ON;
	return iRet;
}

static INT32 opfunc_stp_rst(P_WMT_OP pWmtOp)
{

	return 0;
}

static INT32 opfunc_therm_ctrl(P_WMT_OP pWmtOp)
{

	INT32 iRet = -1;
	UINT32 u4Res;
	UINT32 evtLen;
	UINT8 evtBuf[16] = { 0 };

	WMT_THERM_CMD[4] = pWmtOp->au4OpData[0];	/*CMD type, refer to ENUM_WMTTHERM_TYPE_T */

	/* send command */
	/* iRet = (*kal_stp_tx)(WMT_THERM_CMD, osal_sizeof(WMT_THERM_CMD), &u4Res); */
	iRet = wmt_core_tx((PUINT8) WMT_THERM_CMD, osal_sizeof(WMT_THERM_CMD), &u4Res, MTK_WCN_BOOL_FALSE);
	if (iRet || (u4Res != osal_sizeof(WMT_THERM_CMD))) {
		WMT_ERR_FUNC("WMT-CORE: THERM_CTRL_CMD iRet(%d) cmd len err(%d, %d)\n", iRet, u4Res,
			     osal_sizeof(WMT_THERM_CMD));
		return iRet;
	}

	evtLen = 16;

	iRet = wmt_core_rx(evtBuf, evtLen, &u4Res);
	if (iRet || ((u4Res != osal_sizeof(WMT_THERM_CTRL_EVT)) && (u4Res != osal_sizeof(WMT_THERM_READ_EVT)))) {
		WMT_ERR_FUNC("WMT-CORE: read THERM_CTRL_EVT/THERM_READ_EVENT fail(%d) len(%d, %d)\n", iRet, u4Res,
			     evtLen);
		mtk_wcn_stp_dbg_dump_package();
		return iRet;
	}
	if (u4Res == osal_sizeof(WMT_THERM_CTRL_EVT)) {
		if (osal_memcmp(evtBuf, WMT_THERM_CTRL_EVT, osal_sizeof(WMT_THERM_CTRL_EVT)) != 0) {
			WMT_ERR_FUNC("WMT-CORE: compare WMT_THERM_CTRL_EVT error\n");
			WMT_ERR_FUNC("WMT-CORE: rx(%d):[%02X,%02X,%02X,%02X,%02X] exp(%d):[%02X,%02X,%02X,%02X,%02X]\n",
				     u4Res, evtBuf[0], evtBuf[1], evtBuf[2], evtBuf[3], evtBuf[4],
				     osal_sizeof(WMT_THERM_CTRL_EVT), WMT_THERM_CTRL_EVT[0], WMT_THERM_CTRL_EVT[1],
				     WMT_THERM_CTRL_EVT[2], WMT_THERM_CTRL_EVT[3], WMT_THERM_CTRL_EVT[4]);
			pWmtOp->au4OpData[1] = MTK_WCN_BOOL_FALSE;	/*will return to function driver */
			mtk_wcn_stp_dbg_dump_package();
		} else {
			WMT_DBG_FUNC("Send WMT_THERM_CTRL_CMD command OK!\n");
			pWmtOp->au4OpData[1] = MTK_WCN_BOOL_TRUE;	/*will return to function driver */
		}
	} else {
		/*no need to judge the real thermal value */
		if (osal_memcmp(evtBuf, WMT_THERM_READ_EVT, osal_sizeof(WMT_THERM_READ_EVT) - 1) != 0) {
			WMT_ERR_FUNC("WMT-CORE: compare WMT_THERM_READ_EVT error\n");
			WMT_ERR_FUNC("WMT-CORE: rx(%d):[%02X,%02X,%02X,%02X,%02X,%02X] exp(%d):[%02X,%02X,%02X,%02X]\n",
				     u4Res, evtBuf[0], evtBuf[1], evtBuf[2], evtBuf[3], evtBuf[4], evtBuf[5],
				     osal_sizeof(WMT_THERM_READ_EVT), WMT_THERM_READ_EVT[0], WMT_THERM_READ_EVT[1],
				     WMT_THERM_READ_EVT[2], WMT_THERM_READ_EVT[3]);
			pWmtOp->au4OpData[1] = 0xFF;	/*will return to function driver */
			mtk_wcn_stp_dbg_dump_package();
		} else {
			WMT_DBG_FUNC("Send WMT_THERM_READ_CMD command OK!\n");
			pWmtOp->au4OpData[1] = evtBuf[5];	/*will return to function driver */
		}
	}

	return iRet;

}

static INT32 opfunc_efuse_rw(P_WMT_OP pWmtOp)
{

	INT32 iRet = -1;
	UINT32 u4Res;
	UINT32 evtLen;
	UINT8 evtBuf[16] = { 0 };

	if (DRV_STS_FUNC_ON != gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT]) {
		WMT_ERR_FUNC("WMT-CORE: wmt_efuse_rw fail: chip is powered off\n");
		return -1;
	}

	WMT_EFUSE_CMD[4] = (pWmtOp->au4OpData[0]) ? 0x1 : 0x2;	/* w:2, r:1 */
	osal_memcpy(&WMT_EFUSE_CMD[6], (PUINT8) &pWmtOp->au4OpData[1], 2);	/* address */
	osal_memcpy(&WMT_EFUSE_CMD[8], (PUINT32) pWmtOp->au4OpData[2], 4);	/* value */

	wmt_core_dump_data(&WMT_EFUSE_CMD[0], "efuse_cmd", osal_sizeof(WMT_EFUSE_CMD));

	/* send command */
	/* iRet = (*kal_stp_tx)(WMT_EFUSE_CMD, osal_sizeof(WMT_EFUSE_CMD), &u4Res); */
	iRet = wmt_core_tx((PUINT8) WMT_EFUSE_CMD, osal_sizeof(WMT_EFUSE_CMD), &u4Res, MTK_WCN_BOOL_FALSE);
	if (iRet || (u4Res != osal_sizeof(WMT_EFUSE_CMD))) {
		WMT_ERR_FUNC("WMT-CORE: EFUSE_CMD iRet(%d) cmd len err(%d, %d)\n", iRet, u4Res,
			     osal_sizeof(WMT_EFUSE_CMD));
		return iRet;
	}

	evtLen = (pWmtOp->au4OpData[0]) ? osal_sizeof(WMT_EFUSE_EVT) : osal_sizeof(WMT_EFUSE_EVT);

	iRet = wmt_core_rx(evtBuf, evtLen, &u4Res);
	if (iRet || (u4Res != evtLen))
		WMT_ERR_FUNC("WMT-CORE: read REG_EVB fail(%d) len(%d, %d)\n", iRet, u4Res, evtLen);
	wmt_core_dump_data(&evtBuf[0], "efuse_evt", osal_sizeof(evtBuf));

	return iRet;

}

static INT32 opfunc_gpio_ctrl(P_WMT_OP pWmtOp)
{
	INT32 iRet = -1;
	WMT_IC_PIN_ID id;
	WMT_IC_PIN_STATE stat;
	UINT32 flag;

	if (DRV_STS_FUNC_ON != gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_WMT]) {
		WMT_ERR_FUNC("WMT-CORE: wmt_gpio_ctrl fail: chip is powered off\n");
		return -1;
	}

	if (!gMtkWmtCtx.p_ic_ops->ic_pin_ctrl) {
		WMT_ERR_FUNC("WMT-CORE: error, gMtkWmtCtx.p_ic_ops->ic_pin_ctrl(NULL)\n");
		return -1;
	}

	id = pWmtOp->au4OpData[0];
	stat = pWmtOp->au4OpData[1];
	flag = pWmtOp->au4OpData[2];

	WMT_INFO_FUNC("ic pin id:%d, stat:%d, flag:0x%x\n", id, stat, flag);

	iRet = (*(gMtkWmtCtx.p_ic_ops->ic_pin_ctrl)) (id, stat, flag);

	return iRet;
}

MTK_WCN_BOOL wmt_core_is_quick_ps_support(void)
{
	P_WMT_CTX pctx = &gMtkWmtCtx;

	if ((NULL != pctx->p_ic_ops) && (NULL != pctx->p_ic_ops->is_quick_sleep))
		return (*(pctx->p_ic_ops->is_quick_sleep)) ();

	return MTK_WCN_BOOL_FALSE;
}

MTK_WCN_BOOL wmt_core_get_aee_dump_flag(void)
{
	MTK_WCN_BOOL bRet = MTK_WCN_BOOL_FALSE;
	P_WMT_CTX pctx = &gMtkWmtCtx;

	if ((NULL != pctx->p_ic_ops) && (NULL != pctx->p_ic_ops->is_aee_dump_support))
		bRet = (*(pctx->p_ic_ops->is_aee_dump_support)) ();
	else
		bRet = MTK_WCN_BOOL_FALSE;

	return bRet;
}

INT32 opfunc_pin_state(P_WMT_OP pWmtOp)
{

	unsigned long ctrlPa1 = 0;
	unsigned long ctrlPa2 = 0;
	UINT32 iRet = 0;

	iRet = wmt_core_ctrl(WMT_CTRL_HW_STATE_DUMP, &ctrlPa1, &ctrlPa2);
	return iRet;
}

static INT32 opfunc_bgw_ds(P_WMT_OP pWmtOp)
{
	INT32 iRet = -1;
	UINT32 u4WrittenSize = 0;
	UINT32 u4ReadSize = 0;
	UINT32 buf_len = 0;
	UINT8 *buffer = NULL;
	UINT8 evt_buffer[8] = { 0 };
	MTK_WCN_BOOL fgFail;

	UINT8 WMT_BGW_DESENSE_CMD[] = {
		0x01, 0x0e, 0x0f, 0x00,
		0x02, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00
	};
	UINT8 WMT_BGW_DESENSE_EVT[] = { 0x02, 0x0e, 0x01, 0x00, 0x00 };

	buf_len = pWmtOp->au4OpData[0];
	buffer = (PUINT8) pWmtOp->au4OpData[1];

	osal_memcpy(&WMT_BGW_DESENSE_CMD[5], buffer, buf_len);

	do {
		fgFail = MTK_WCN_BOOL_TRUE;

		iRet =
		    wmt_core_tx(&WMT_BGW_DESENSE_CMD[0], osal_sizeof(WMT_BGW_DESENSE_CMD), &u4WrittenSize,
				MTK_WCN_BOOL_FALSE);
		if (iRet || (u4WrittenSize != osal_sizeof(WMT_BGW_DESENSE_CMD))) {
			WMT_ERR_FUNC("bgw desense tx CMD fail(%d),size(%d)\n", iRet, u4WrittenSize);
			break;
		}

		iRet = wmt_core_rx(evt_buffer, osal_sizeof(WMT_BGW_DESENSE_EVT), &u4ReadSize);
		if (iRet || (u4ReadSize != osal_sizeof(WMT_BGW_DESENSE_EVT))) {
			WMT_ERR_FUNC("bgw desense rx EVT fail(%d),size(%d)\n", iRet, u4ReadSize);
			break;
		}

		if (osal_memcmp(evt_buffer, WMT_BGW_DESENSE_EVT, osal_sizeof(WMT_BGW_DESENSE_EVT)) != 0) {
			WMT_ERR_FUNC
			    ("bgw desense WMT_BGW_DESENSE_EVT compare fail:0x%02x,0x%02x,0x%02x,0x%02x,0x%02x\n",
			     evt_buffer[0], evt_buffer[1], evt_buffer[2], evt_buffer[3], evt_buffer[4]);
			break;
		}

		fgFail = MTK_WCN_BOOL_FALSE;

	} while (0);

	return fgFail;
}

static INT32 opfunc_set_mcu_clk(P_WMT_OP pWmtOp)
{
	UINT32 kind = 0;
	INT32 iRet = -1;
	UINT32 u4WrittenSize = 0;
	UINT32 u4ReadSize = 0;
	UINT8 evt_buffer[12] = { 0 };
	MTK_WCN_BOOL fgFail;
	PUINT8 set_mcu_clk_str[] = {
		"Enable MCU PLL",
		"SET MCU CLK to 26M",
		"SET MCU CLK to 37M",
		"SET MCU CLK to 64M",
		"SET MCU CLK to 69M",
		"SET MCU CLK to 104M",
		"SET MCU CLK to 118.857M",
		"SET MCU CLK to 138.67M",
		"Disable MCU PLL"
	};
	UINT8 WMT_SET_MCU_CLK_CMD[] = {
		0x01, 0x08, 0x10, 0x00,
		0x01, 0x01, 0x00, 0x01,
		0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00,
		0xff, 0xff, 0xff, 0xff
	};
	UINT8 WMT_SET_MCU_CLK_EVT[] = { 0x02, 0x08, 0x04, 0x00, 0x00, 0x00, 0x00, 0x01 };

	UINT8 WMT_EN_MCU_CLK_CMD[] = { 0x34, 0x03, 0x00, 0x80, 0x00, 0x00, 0x01, 0x00 };	/* enable pll clk */
	UINT8 WMT_26_MCU_CLK_CMD[] = { 0x0c, 0x01, 0x00, 0x80, 0x00, 0x4d, 0x84, 0x00 };	/* set 26M */
	UINT8 WMT_37_MCU_CLK_CMD[] = { 0x0c, 0x01, 0x00, 0x80, 0x1e, 0x4d, 0x84, 0x00 };	/* set 37.8M */
	UINT8 WMT_64_MCU_CLK_CMD[] = { 0x0c, 0x01, 0x00, 0x80, 0x1d, 0x4d, 0x84, 0x00 };	/* set 64M */
	UINT8 WMT_69_MCU_CLK_CMD[] = { 0x0c, 0x01, 0x00, 0x80, 0x1c, 0x4d, 0x84, 0x00 };	/* set 69M */
	UINT8 WMT_104_MCU_CLK_CMD[] = { 0x0c, 0x01, 0x00, 0x80, 0x5b, 0x4d, 0x84, 0x00 };	/* set 104M */
	UINT8 WMT_108_MCU_CLK_CMD[] = { 0x0c, 0x01, 0x00, 0x80, 0x5a, 0x4d, 0x84, 0x00 };	/* set 118.857M */
	UINT8 WMT_138_MCU_CLK_CMD[] = { 0x0c, 0x01, 0x00, 0x80, 0x59, 0x4d, 0x84, 0x00 };	/* set 138.67M */
	UINT8 WMT_DIS_MCU_CLK_CMD[] = { 0x34, 0x03, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00 };	/* disable pll clk */

	kind = pWmtOp->au4OpData[0];
	WMT_INFO_FUNC("do %s\n", set_mcu_clk_str[kind]);

	switch (kind) {
	case 0:
		osal_memcpy(&WMT_SET_MCU_CLK_CMD[8], &WMT_EN_MCU_CLK_CMD[0], osal_sizeof(WMT_EN_MCU_CLK_CMD));
		break;
	case 1:
		osal_memcpy(&WMT_SET_MCU_CLK_CMD[8], &WMT_26_MCU_CLK_CMD[0], osal_sizeof(WMT_26_MCU_CLK_CMD));
		break;
	case 2:
		osal_memcpy(&WMT_SET_MCU_CLK_CMD[8], &WMT_37_MCU_CLK_CMD[0], osal_sizeof(WMT_37_MCU_CLK_CMD));
		break;
	case 3:
		osal_memcpy(&WMT_SET_MCU_CLK_CMD[8], &WMT_64_MCU_CLK_CMD[0], osal_sizeof(WMT_64_MCU_CLK_CMD));
		break;
	case 4:
		osal_memcpy(&WMT_SET_MCU_CLK_CMD[8], &WMT_69_MCU_CLK_CMD[0], osal_sizeof(WMT_69_MCU_CLK_CMD));
		break;
	case 5:
		osal_memcpy(&WMT_SET_MCU_CLK_CMD[8], &WMT_104_MCU_CLK_CMD[0], osal_sizeof(WMT_104_MCU_CLK_CMD));
		break;
	case 6:
		osal_memcpy(&WMT_SET_MCU_CLK_CMD[8], &WMT_108_MCU_CLK_CMD[0], osal_sizeof(WMT_108_MCU_CLK_CMD));
		break;
	case 7:
		osal_memcpy(&WMT_SET_MCU_CLK_CMD[8], &WMT_138_MCU_CLK_CMD[0], osal_sizeof(WMT_138_MCU_CLK_CMD));
		break;
	case 8:
		osal_memcpy(&WMT_SET_MCU_CLK_CMD[8], &WMT_DIS_MCU_CLK_CMD[0], osal_sizeof(WMT_DIS_MCU_CLK_CMD));
		break;
	default:
		WMT_ERR_FUNC("unknown kind\n");
		break;
	}

	do {
		fgFail = MTK_WCN_BOOL_TRUE;

		iRet =
		    wmt_core_tx(&WMT_SET_MCU_CLK_CMD[0], osal_sizeof(WMT_SET_MCU_CLK_CMD), &u4WrittenSize,
				MTK_WCN_BOOL_FALSE);
		if (iRet || (u4WrittenSize != osal_sizeof(WMT_SET_MCU_CLK_CMD))) {
			WMT_ERR_FUNC("WMT_SET_MCU_CLK_CMD fail(%d),size(%d)\n", iRet, u4WrittenSize);
			break;
		}

		iRet = wmt_core_rx(evt_buffer, osal_sizeof(WMT_SET_MCU_CLK_EVT), &u4ReadSize);
		if (iRet || (u4ReadSize != osal_sizeof(WMT_SET_MCU_CLK_EVT))) {
			WMT_ERR_FUNC("WMT_SET_MCU_CLK_EVT fail(%d),size(%d)\n", iRet, u4ReadSize);
			break;
		}

		if (osal_memcmp(evt_buffer, WMT_SET_MCU_CLK_EVT, osal_sizeof(WMT_SET_MCU_CLK_EVT)) != 0) {
			WMT_ERR_FUNC("WMT_SET_MCU_CLK_EVT compare fail:0x%02x,0x%02x,0x%02x,0x%02x,0x%02x\n",
				     evt_buffer[0], evt_buffer[1], evt_buffer[2], evt_buffer[3], evt_buffer[4],
				     evt_buffer[5], evt_buffer[6], evt_buffer[7]);
			break;
		}

		fgFail = MTK_WCN_BOOL_FALSE;

	} while (0);

	if (MTK_WCN_BOOL_FALSE == fgFail)
		WMT_INFO_FUNC("wmt-core:%s: ok!\n", set_mcu_clk_str[kind]);

	WMT_INFO_FUNC("wmt-core:%s: fail!\n", set_mcu_clk_str[kind]);

	return fgFail;
}

static INT32 opfunc_adie_lpbk_test(P_WMT_OP pWmtOp)
{
	UINT8 *buffer = NULL;
	MTK_WCN_BOOL fgFail;
	UINT32 u4Res;
	UINT32 aDieChipid = 0;
	UINT8 soc_adie_chipid_cmd[] = { 0x01, 0x13, 0x04, 0x00, 0x02, 0x04, 0x24, 0x00 };
	UINT8 soc_adie_chipid_evt[] = { 0x02, 0x13, 0x09, 0x00, 0x00, 0x02, 0x04, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00 };
	UINT8 evtbuf[20];
	INT32 iRet = -1;

	buffer = (PUINT8) pWmtOp->au4OpData[1];

	do {
		fgFail = MTK_WCN_BOOL_TRUE;

		/* read A die chipid by wmt cmd */
		iRet =
		    wmt_core_tx((PUINT8) &soc_adie_chipid_cmd[0], osal_sizeof(soc_adie_chipid_cmd), &u4Res,
				MTK_WCN_BOOL_FALSE);
		if (iRet || (u4Res != osal_sizeof(soc_adie_chipid_cmd))) {
			WMT_ERR_FUNC("wmt_core:read A die chipid CMD fail(%d),size(%d)\n", iRet, u4Res);
			break;
		}
		osal_memset(evtbuf, 0, osal_sizeof(evtbuf));
		iRet = wmt_core_rx(evtbuf, osal_sizeof(soc_adie_chipid_evt), &u4Res);
		if (iRet || (u4Res != osal_sizeof(soc_adie_chipid_evt))) {
			WMT_ERR_FUNC("wmt_core:read A die chipid EVT fail(%d),size(%d)\n", iRet, u4Res);
			break;
		}
		osal_memcpy(&aDieChipid, &evtbuf[u4Res - 2], 2);
		osal_memcpy(buffer, &evtbuf[u4Res - 2], 2);
		pWmtOp->au4OpData[0] = 2;
		WMT_INFO_FUNC("get SOC A die chipid(0x%x)\n", aDieChipid);

		fgFail = MTK_WCN_BOOL_FALSE;

	} while (0);

	return fgFail;
}

/* m2note: read an analog-die (RF transceiver) 16-bit register via a WMT cmd and report
 * it at WARN level. reg=0x2404 returns the a-die chip-id (0x6625 on MT6625). Any other
 * reg reads the RX/RF path config. A valid value means the RF analog block answers. */
INT32 mtk_wcn_m2note_adie_reg_read(UINT32 reg)
{
	UINT32 u4Res = 0;
	UINT32 val = 0;
	INT32 iRet = -1;
	/* cmd: {0x01,0x13,len,0x00,0x02,REG_LO,REG_HI,0x00} reads a 16-bit a-die reg */
	UINT8 cmd[] = { 0x01, 0x13, 0x04, 0x00, 0x02,
			(UINT8)(reg & 0xff), (UINT8)((reg >> 8) & 0xff), 0x00 };
	UINT8 evt_tmpl[] = { 0x02, 0x13, 0x09, 0x00, 0x00, 0x02, 0x04, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00 };
	UINT8 evtbuf[20];

	iRet = wmt_core_tx((PUINT8) &cmd[0], osal_sizeof(cmd), &u4Res, MTK_WCN_BOOL_FALSE);
	if (iRet || (u4Res != osal_sizeof(cmd))) {
		WMT_WARN_FUNC("M2NOTE_WCN_ADIE_TRACE reg=0x%04x stage=cmd_fail ret=%d size=%d\n", reg, iRet, u4Res);
		return -1;
	}
	osal_memset(evtbuf, 0, osal_sizeof(evtbuf));
	iRet = wmt_core_rx(evtbuf, osal_sizeof(evt_tmpl), &u4Res);
	if (iRet || (u4Res != osal_sizeof(evt_tmpl))) {
		WMT_WARN_FUNC("M2NOTE_WCN_ADIE_TRACE reg=0x%04x stage=evt_fail ret=%d size=%d\n", reg, iRet, u4Res);
		return -2;
	}
	if (evtbuf[0] != 0x02 || evtbuf[1] != 0x13 || evtbuf[4] != 0x00 ||
	    evtbuf[5] != 0x02 || evtbuf[6] != (UINT8)(reg & 0xff) ||
	    evtbuf[7] != (UINT8)((reg >> 8) & 0xff) || evtbuf[8] != 0x00) {
		WMT_WARN_FUNC("M2NOTE_WCN_ADIE_TRACE reg=0x%04x stage=evt_bad ret=%d size=%d evt=[%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X]\n",
			      reg, iRet, u4Res,
			      evtbuf[0], evtbuf[1], evtbuf[2], evtbuf[3],
			      evtbuf[4], evtbuf[5], evtbuf[6], evtbuf[7],
			      evtbuf[8], evtbuf[9], evtbuf[10], evtbuf[11],
			      evtbuf[12]);
		return -3;
	}
	if (u4Res >= 2)
		osal_memcpy(&val, &evtbuf[u4Res - 2], 2);
	WMT_WARN_FUNC("M2NOTE_WCN_ADIE_TRACE reg=0x%04x stage=ok val=0x%04x evt=[%02X %02X %02X %02X %02X %02X %02X %02X]\n",
		      reg, val, evtbuf[0], evtbuf[1], evtbuf[2], evtbuf[3],
		      evtbuf[4], evtbuf[5], evtbuf[u4Res >= 2 ? u4Res - 2 : 0],
		      evtbuf[u4Res >= 1 ? u4Res - 1 : 0]);
	return (INT32) val;
}

/* m2note: write a 16-bit analog-die register and require a matching WMT event.
 * This is intentionally strict: a bad event is evidence, not success. */
INT32 mtk_wcn_m2note_adie_reg_write(UINT32 reg, UINT32 val)
{
	UINT32 u4Res = 0;
	INT32 iRet = -1;
	UINT8 cmd[] = { 0x01, 0x13, 0x06, 0x00, 0x01,
			(UINT8)(reg & 0xff), (UINT8)((reg >> 8) & 0xff), 0x00,
			(UINT8)(val & 0xff), (UINT8)((val >> 8) & 0xff) };
	UINT8 evt_tmpl[] = { 0x02, 0x13, 0x09, 0x00, 0x00, 0x01,
			     0x04, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00 };
	UINT8 evtbuf[20];

	iRet = wmt_core_tx((PUINT8) &cmd[0], osal_sizeof(cmd), &u4Res, MTK_WCN_BOOL_FALSE);
	if (iRet || (u4Res != osal_sizeof(cmd))) {
		WMT_WARN_FUNC("M2NOTE_WCN_ADIE_WRITE_TRACE reg=0x%04x val=0x%04x stage=cmd_fail ret=%d size=%d\n",
			      reg, val & 0xffff, iRet, u4Res);
		return -1;
	}
	osal_memset(evtbuf, 0, osal_sizeof(evtbuf));
	iRet = wmt_core_rx(evtbuf, osal_sizeof(evt_tmpl), &u4Res);
	if (iRet || (u4Res != osal_sizeof(evt_tmpl))) {
		WMT_WARN_FUNC("M2NOTE_WCN_ADIE_WRITE_TRACE reg=0x%04x val=0x%04x stage=evt_fail ret=%d size=%d\n",
			      reg, val & 0xffff, iRet, u4Res);
		return -2;
	}
	if (evtbuf[0] != 0x02 || evtbuf[1] != 0x13 || evtbuf[4] != 0x00 ||
	    evtbuf[5] != 0x01 || evtbuf[6] != (UINT8)(reg & 0xff) ||
	    evtbuf[7] != (UINT8)((reg >> 8) & 0xff) || evtbuf[8] != 0x00) {
		WMT_WARN_FUNC("M2NOTE_WCN_ADIE_WRITE_TRACE reg=0x%04x val=0x%04x stage=evt_bad ret=%d size=%d evt=[%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X]\n",
			      reg, val & 0xffff, iRet, u4Res,
			      evtbuf[0], evtbuf[1], evtbuf[2], evtbuf[3],
			      evtbuf[4], evtbuf[5], evtbuf[6], evtbuf[7],
			      evtbuf[8], evtbuf[9], evtbuf[10], evtbuf[11],
			      evtbuf[12]);
		return -3;
	}

	WMT_WARN_FUNC("M2NOTE_WCN_ADIE_WRITE_TRACE reg=0x%04x val=0x%04x stage=ok evt=[%02X %02X %02X %02X %02X %02X %02X %02X]\n",
		      reg, val & 0xffff, evtbuf[0], evtbuf[1], evtbuf[2],
		      evtbuf[3], evtbuf[4], evtbuf[5], evtbuf[6], evtbuf[7]);
	return 0;
}

INT32 mtk_wcn_m2note_reg08_read_trace(const char *stage, UINT32 offset,
				      PUINT32 pVal)
{
	UINT32 u4Res = 0;
	UINT32 val = 0;
	UINT32 rxAddr = 0;
	UINT16 tail16 = 0;
	INT32 iRet = -1;
	const char *trace_stage = stage ? stage : "null";
	UINT8 cmd[sizeof(WMT_SET_REG_CMD)];
	UINT8 evtbuf[sizeof(WMT_SET_REG_RD_EVT)];

	osal_memcpy(cmd, WMT_SET_REG_CMD, sizeof(cmd));
	osal_memset(evtbuf, 0, sizeof(evtbuf));

	cmd[4] = 0x2;		/* read */
	osal_memcpy(&cmd[8], &offset, 4);
	osal_memset(&cmd[12], 0, 4);
	osal_memset(&cmd[16], 0xff, 4);

	iRet = wmt_core_tx(cmd, sizeof(cmd), &u4Res, MTK_WCN_BOOL_FALSE);
	if (iRet || (u4Res != sizeof(cmd))) {
		WMT_WARN_FUNC("M2NOTE_WCN_REG08_TRACE stage=%s offset=0x%08x phase=cmd_fail ret=%d size=%u\n",
			      trace_stage, offset, iRet, u4Res);
		return -1;
	}

	iRet = wmt_core_rx(evtbuf, sizeof(evtbuf), &u4Res);
	if (iRet || (u4Res != sizeof(evtbuf))) {
		WMT_WARN_FUNC("M2NOTE_WCN_REG08_TRACE stage=%s offset=0x%08x phase=evt_fail ret=%d size=%u\n",
			      trace_stage, offset, iRet, u4Res);
		return -2;
	}

	osal_memcpy(&rxAddr, &evtbuf[8], 4);
	osal_memcpy(&val, &evtbuf[12], 4);
	if (u4Res >= 2)
		osal_memcpy(&tail16, &evtbuf[u4Res - 2], 2);

	if (pVal)
		*pVal = val;

	WMT_WARN_FUNC("M2NOTE_WCN_REG08_TRACE stage=%s offset=0x%08x phase=ok ret=%d size=%u rx_addr=0x%08x val=0x%08x tail16=0x%04x evt=[%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X]\n",
		      trace_stage, offset, iRet, u4Res, rxAddr, val, tail16,
		      evtbuf[0], evtbuf[1], evtbuf[2], evtbuf[3],
		      evtbuf[4], evtbuf[5], evtbuf[6], evtbuf[7],
		      evtbuf[8], evtbuf[9], evtbuf[10], evtbuf[11],
		      evtbuf[12], evtbuf[13], evtbuf[14], evtbuf[15]);

	if (rxAddr != offset)
		return -3;
	return 0;
}

INT32 mtk_wcn_m2note_adie_chipid_probe(VOID)
{
	return mtk_wcn_m2note_adie_reg_read(0x2404);
}

#if CFG_WMT_LTE_COEX_HANDLING
static INT32 opfunc_idc_msg_handling(P_WMT_OP pWmtOp)
{
	MTK_WCN_BOOL fgFail;
	UINT32 u4Res;
	UINT8 host_lte_btwf_coex_cmd[] = { 0x01, 0x10, 0x00, 0x00, 0x00 };
	UINT8 host_lte_btwf_coex_evt[] = { 0x02, 0x10, 0x01, 0x00, 0x00 };
	UINT8 *pTxBuf = NULL;
	UINT8 evtbuf[8] = { 0 };
	INT32 iRet = -1;
	UINT16 msg_len = 0;
	UINT32 total_len = 0;
	UINT32 index = 0;
	UINT8 *msg_local_buffer = NULL;

	msg_local_buffer = kmalloc(1300, GFP_KERNEL);
	if (!msg_local_buffer) {
		WMT_ERR_FUNC("msg_local_buffer kmalloc memory fail\n");
		WMT_WARN_FUNC("M2NOTE_WCN_IDC_CORE_TRACE stage=alloc_fail ret=0\n");
		return 0;
	}

	pTxBuf = (UINT8 *) pWmtOp->au4OpData[0];
	if (NULL == pTxBuf) {
		WMT_ERR_FUNC("idc msg buffer is NULL\n");
		WMT_WARN_FUNC("M2NOTE_WCN_IDC_CORE_TRACE stage=null_buffer ret=-1\n");
		kfree(msg_local_buffer);
		return -1;
	}
	iRet = wmt_lib_idc_lock_aquire();
	if (iRet) {
		WMT_ERR_FUNC("--->lock idc_lock failed, ret=%d\n", iRet);
		WMT_WARN_FUNC("M2NOTE_WCN_IDC_CORE_TRACE stage=lock_fail ret=%d\n", iRet);
		kfree(msg_local_buffer);
		return iRet;
	}
	osal_memcpy(&msg_len, &pTxBuf[0], osal_sizeof(msg_len));
	if (msg_len > 1200) {
		wmt_lib_idc_lock_release();
		WMT_ERR_FUNC("abnormal idc msg len:%d\n", msg_len);
		WMT_WARN_FUNC("M2NOTE_WCN_IDC_CORE_TRACE stage=bad_len len=%u ret=-2\n", msg_len);
		kfree(msg_local_buffer);
		return -2;
	}
	msg_len += 1;	/*flag byte */

	osal_memcpy(&host_lte_btwf_coex_cmd[2], &msg_len, 2);
	host_lte_btwf_coex_cmd[4] = (pWmtOp->au4OpData[1] & 0x00ff);
	osal_memcpy(&msg_local_buffer[0], &host_lte_btwf_coex_cmd[0], osal_sizeof(host_lte_btwf_coex_cmd));
	osal_memcpy(&msg_local_buffer[osal_sizeof(host_lte_btwf_coex_cmd)],
		&pTxBuf[osal_sizeof(msg_len)], msg_len - 1);

	wmt_lib_idc_lock_release();
	total_len = osal_sizeof(host_lte_btwf_coex_cmd) + msg_len - 1;

	WMT_WARN_FUNC("M2NOTE_WCN_IDC_CORE_TRACE stage=tx_start raw_len=%u total=%u flag=0x%02x data=[%02X %02X %02X %02X %02X %02X]\n",
		      msg_len - 1, total_len, host_lte_btwf_coex_cmd[4],
		      total_len > 0 ? msg_local_buffer[0] : 0,
		      total_len > 1 ? msg_local_buffer[1] : 0,
		      total_len > 2 ? msg_local_buffer[2] : 0,
		      total_len > 3 ? msg_local_buffer[3] : 0,
		      total_len > 4 ? msg_local_buffer[4] : 0,
		      total_len > 5 ? msg_local_buffer[5] : 0);

	WMT_DBG_FUNC("wmt_core:idc msg payload len form lte(%d),wmt msg total len(%d)\n", msg_len - 1,
		     total_len);
	WMT_DBG_FUNC("wmt_core:idc msg payload:\n");

	for (index = 0; index < total_len; index++)
		WMT_DBG_FUNC("0x%02x ", msg_local_buffer[index]);


	do {
		fgFail = MTK_WCN_BOOL_TRUE;

		/* read A die chipid by wmt cmd */
		iRet = wmt_core_tx((PUINT8) &msg_local_buffer[0], total_len, &u4Res, MTK_WCN_BOOL_FALSE);
		if (iRet || (u4Res != total_len)) {
			WMT_ERR_FUNC("wmt_core:send lte idc msg to connsys fail(%d),size(%d)\n", iRet, u4Res);
			WMT_WARN_FUNC("M2NOTE_WCN_IDC_CORE_TRACE stage=tx_fail ret=%d wrote=%u exp=%u\n",
				      iRet, u4Res, total_len);
			break;
		}
		osal_memset(evtbuf, 0, osal_sizeof(evtbuf));
		iRet = wmt_core_rx(evtbuf, osal_sizeof(host_lte_btwf_coex_evt), &u4Res);
		if (iRet || (u4Res != osal_sizeof(host_lte_btwf_coex_evt))) {
			WMT_ERR_FUNC("wmt_core:recv host_lte_btwf_coex_evt fail(%d),size(%d)\n", iRet, u4Res);
			WMT_WARN_FUNC("M2NOTE_WCN_IDC_CORE_TRACE stage=rx_fail ret=%d got=%u exp=%u evt=[%02X %02X %02X %02X %02X]\n",
				      iRet, u4Res, (UINT32)osal_sizeof(host_lte_btwf_coex_evt),
				      evtbuf[0], evtbuf[1], evtbuf[2], evtbuf[3], evtbuf[4]);
			break;
		}

		fgFail = MTK_WCN_BOOL_FALSE;

	} while (0);
	WMT_WARN_FUNC("M2NOTE_WCN_IDC_CORE_TRACE stage=done fail=%u ret=%d got=%u evt=[%02X %02X %02X %02X %02X]\n",
		      fgFail, iRet, u4Res, evtbuf[0], evtbuf[1], evtbuf[2], evtbuf[3], evtbuf[4]);
	kfree(msg_local_buffer);
	return fgFail;
}
#endif

VOID wmt_core_set_coredump_state(ENUM_DRV_STS state)
{
	WMT_INFO_FUNC("wmt-core: set coredump state(%d)\n", state);
	gMtkWmtCtx.eDrvStatus[WMTDRV_TYPE_COREDUMP] = state;
}

#if CFG_WMT_LTE_COEX_HANDLING
/*TEST CODE*/
static UINT32 g_open_wmt_lte_flag;
VOID wmt_core_set_flag_for_test(UINT32 enable)
{
	WMT_INFO_FUNC("%s wmt_lte_flag\n", enable ? "enable" : "disable");
	g_open_wmt_lte_flag = enable;
}

UINT32 wmt_core_get_flag_for_test(VOID)
{
	return g_open_wmt_lte_flag;
}
#endif
