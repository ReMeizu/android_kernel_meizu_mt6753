/*
 * BU64245 voice coil motor backend for the MTK MAINAF lens wrapper.
 *
 * Stock m2note 3.10 uses BU64245 at 8-bit I2C write ID 0x18.  This file
 * adapts that actuator protocol to the 3.18 common lens backend interface.
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <mt-plat/mt_gpio.h>
#include <cust_gpio_usage.h>

#include "lens_info.h"

#define AF_DRVNAME "BU64245_DRV"
#define AF_I2C_SLAVE_ADDR 0x18

#define LOG_INF(format, args...) pr_debug(AF_DRVNAME " [%s] " format, __func__, ##args)
#define M2NOTE_BU64245_TRACE(format, args...) \
	pr_err("M2NOTE_BU64245_TRACE " format, ##args)

static struct i2c_client *g_pstAF_I2Cclient;
static int *g_pAF_Opened;
static spinlock_t *g_pAF_SpinLock;

static long g_i4MotorStatus;
static long g_i4Dir;
static unsigned long g_u4AF_INF;
static unsigned long g_u4AF_MACRO = 1023;
static unsigned long g_u4TargetPosition;
static unsigned long g_u4CurrPosition;

static void BU64245_EnableAFPin(const char *caller)
{
	unsigned long pin = GPIO_CAMERA_AF_EN_PIN;
	int ret_mode;
	int ret_dir;
	int ret_out;

	ret_mode = mt_set_gpio_mode(pin, GPIO_CAMERA_AF_EN_PIN_M_GPIO);
	ret_dir = mt_set_gpio_dir(pin, GPIO_DIR_OUT);
	ret_out = mt_set_gpio_out(pin, GPIO_OUT_ONE);

	M2NOTE_BU64245_TRACE("stage=af_en caller=%s pin=0x%lx ret_mode=%d ret_dir=%d ret_out=%d\n",
			     caller, pin, ret_mode, ret_dir, ret_out);
}

static void BU64245_SelectI2CAddr(void)
{
	g_pstAF_I2Cclient->addr = AF_I2C_SLAVE_ADDR >> 1;
}

static int s4AF_ReadReg(unsigned short *a_pu2Result)
{
	int i4RetValue;
	char pBuff[2];

	if (!g_pstAF_I2Cclient)
		return -ENODEV;

	BU64245_SelectI2CAddr();
	BU64245_EnableAFPin("read");
	i4RetValue = i2c_master_recv(g_pstAF_I2Cclient, pBuff, 2);
	if (i4RetValue < 0) {
		M2NOTE_BU64245_TRACE("stage=read_fail addr7=0x%02x ret=%d\n",
				     g_pstAF_I2Cclient->addr, i4RetValue);
		return -EIO;
	}

	*a_pu2Result = (((u16)(pBuff[0] & 0x03)) << 8) + pBuff[1];

	return 0;
}

static int s4AF_WriteReg(u16 a_u2Data)
{
	int i4RetValue;
	char puSendCmd[2] = {
		(char)(((a_u2Data >> 8) & 0x03) | 0xc0),
		(char)(a_u2Data & 0xff)
	};

	if (!g_pstAF_I2Cclient)
		return -ENODEV;

	BU64245_SelectI2CAddr();
	BU64245_EnableAFPin("write");
	g_pstAF_I2Cclient->ext_flag |= I2C_A_FILTER_MSG;
	i4RetValue = i2c_master_send(g_pstAF_I2Cclient, puSendCmd, 2);
	if (i4RetValue < 0) {
		M2NOTE_BU64245_TRACE("stage=write_fail addr7=0x%02x target=%u ret=%d cmd0=0x%02x cmd1=0x%02x\n",
				     g_pstAF_I2Cclient->addr, a_u2Data,
				     i4RetValue, puSendCmd[0], puSendCmd[1]);
		return -EIO;
	}

	return 0;
}

static inline int getAFInfo(__user stAF_MotorInfo *pstMotorInfo)
{
	stAF_MotorInfo stMotorInfo;

	stMotorInfo.u4MacroPosition = g_u4AF_MACRO;
	stMotorInfo.u4InfPosition = g_u4AF_INF;
	stMotorInfo.u4CurrentPosition = g_u4CurrPosition;
	stMotorInfo.bIsSupportSR = 1;
	stMotorInfo.bIsMotorMoving = (g_i4MotorStatus == 1);
	stMotorInfo.bIsMotorOpen = (*g_pAF_Opened >= 1);

	if (copy_to_user(pstMotorInfo, &stMotorInfo, sizeof(stAF_MotorInfo)))
		LOG_INF("copy to user failed when getting motor information\n");

	return 0;
}

static inline int moveAF(unsigned long a_u4Position)
{
	int ret;

	if ((a_u4Position > g_u4AF_MACRO) || (a_u4Position < g_u4AF_INF)) {
		LOG_INF("out of range\n");
		M2NOTE_BU64245_TRACE("stage=move_range_fail target=%lu inf=%lu macro=%lu\n",
				     a_u4Position, g_u4AF_INF, g_u4AF_MACRO);
		return -EINVAL;
	}

	if (*g_pAF_Opened == 1) {
		unsigned short InitPos;

		ret = s4AF_ReadReg(&InitPos);

		spin_lock(g_pAF_SpinLock);
		if (ret == 0) {
			g_u4CurrPosition = (unsigned long)InitPos;
			M2NOTE_BU64245_TRACE("stage=init_pos pos=%u\n", InitPos);
		} else {
			g_u4CurrPosition = 0;
		}
		*g_pAF_Opened = 2;
		spin_unlock(g_pAF_SpinLock);
	}

	if (g_u4CurrPosition < a_u4Position) {
		spin_lock(g_pAF_SpinLock);
		g_i4Dir = 1;
		spin_unlock(g_pAF_SpinLock);
	} else if (g_u4CurrPosition > a_u4Position) {
		spin_lock(g_pAF_SpinLock);
		g_i4Dir = -1;
		spin_unlock(g_pAF_SpinLock);
	} else {
		return 0;
	}

	spin_lock(g_pAF_SpinLock);
	g_u4TargetPosition = a_u4Position;
	g_i4MotorStatus = 0;
	spin_unlock(g_pAF_SpinLock);

	if (s4AF_WriteReg((unsigned short)g_u4TargetPosition) == 0) {
		spin_lock(g_pAF_SpinLock);
		g_u4CurrPosition = (unsigned long)g_u4TargetPosition;
		spin_unlock(g_pAF_SpinLock);
	} else {
		spin_lock(g_pAF_SpinLock);
		g_i4MotorStatus = -1;
		spin_unlock(g_pAF_SpinLock);
		LOG_INF("set I2C failed when moving the motor\n");
	}

	return 0;
}

static inline int setAFInf(unsigned long a_u4Position)
{
	spin_lock(g_pAF_SpinLock);
	g_u4AF_INF = a_u4Position;
	spin_unlock(g_pAF_SpinLock);
	return 0;
}

static inline int setAFMacro(unsigned long a_u4Position)
{
	spin_lock(g_pAF_SpinLock);
	g_u4AF_MACRO = a_u4Position;
	spin_unlock(g_pAF_SpinLock);
	return 0;
}

long BU64245_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
		   unsigned long a_u4Param)
{
	long i4RetValue = 0;

	switch (a_u4Command) {
	case AFIOC_G_MOTORINFO:
		i4RetValue = getAFInfo((__user stAF_MotorInfo *)(a_u4Param));
		break;
	case AFIOC_T_MOVETO:
		i4RetValue = moveAF(a_u4Param);
		break;
	case AFIOC_T_SETINFPOS:
		i4RetValue = setAFInf(a_u4Param);
		break;
	case AFIOC_T_SETMACROPOS:
		i4RetValue = setAFMacro(a_u4Param);
		break;
	default:
		LOG_INF("No CMD\n");
		i4RetValue = -EPERM;
		break;
	}

	return i4RetValue;
}

int BU64245_Release(struct inode *a_pstInode, struct file *a_pstFile)
{
	LOG_INF("Start\n");

	if (*g_pAF_Opened) {
		spin_lock(g_pAF_SpinLock);
		*g_pAF_Opened = 0;
		spin_unlock(g_pAF_SpinLock);
	}

	LOG_INF("End\n");

	return 0;
}

int BU64245_move_to_nature(void)
{
	int ret = 0;
	long pos = (long)g_u4CurrPosition;
	const int step = 20;
	const int nature = 10;

	if (!g_pstAF_I2Cclient) {
		M2NOTE_BU64245_TRACE("stage=nature_skip reason=no_i2cclient\n");
		return -ENODEV;
	}

	M2NOTE_BU64245_TRACE("stage=nature_start curr=%ld target=%d\n",
			     pos, nature);

	if (pos <= nature)
		pos = nature;

	while (pos > nature) {
		ret = s4AF_WriteReg((u16)pos);
		if (ret)
			return ret;
		msleep(1);
		pos -= step;
		if (pos < nature)
			pos = nature;
	}

	ret = s4AF_WriteReg((u16)nature);
	if (!ret) {
		spin_lock(g_pAF_SpinLock);
		g_u4CurrPosition = nature;
		spin_unlock(g_pAF_SpinLock);
		M2NOTE_BU64245_TRACE("stage=nature_done pos=%d\n", nature);
	}

	return ret;
}
EXPORT_SYMBOL(BU64245_move_to_nature);

void BU64245_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
			  spinlock_t *pAF_SpinLock, int *pAF_Opened)
{
	g_pstAF_I2Cclient = pstAF_I2Cclient;
	g_pAF_SpinLock = pAF_SpinLock;
	g_pAF_Opened = pAF_Opened;
	M2NOTE_BU64245_TRACE("stage=set_i2cclient adapter=%d addr7=0x%02x\n",
			     pstAF_I2Cclient && pstAF_I2Cclient->adapter ?
			     pstAF_I2Cclient->adapter->nr : -1,
			     AF_I2C_SLAVE_ADDR >> 1);
}
