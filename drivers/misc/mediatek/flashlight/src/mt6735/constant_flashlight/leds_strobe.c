#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/time.h>
#include "kd_flashlight.h"
#include <asm/io.h>
#include <asm/uaccess.h>
#include <kd_camera_hw.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/leds.h>
#include <linux/of.h>
#include <mt_gpio.h>

#define TAG_NAME "[leds_strobe.c]"
#define PK_DBG_NONE(fmt, arg...)    do {} while (0)
#define PK_DBG_FUNC(fmt, arg...)    pr_info(TAG_NAME "%s: " fmt, __func__, ##arg)
#define PK_ERR_FUNC(fmt, arg...)    pr_err(TAG_NAME "%s: " fmt, __func__, ##arg)

#define DEBUG_LEDS_STROBE
#ifdef DEBUG_LEDS_STROBE
#define PK_DBG PK_DBG_FUNC
#else
#define PK_DBG(a, ...)
#endif

#undef PK_ERR
#define PK_ERR  PK_ERR_FUNC

#define REG_ENABLE          0x01
#define REG_IVFM            0x02
#define REG_FLASH_LED1_BR   0x03
#define REG_FLASH_LED2_BR   0x04
#define REG_TORCH_LED1_BR   0x05
#define REG_TORCH_LED2_BR   0x06
#define REG_BOOST_CONFIG    0x07
#define REG_FLASH_TOUT      0x08
#define REG_TEMP            0x09
#define REG_FLAG0           0x0a
#define REG_FLAG1           0x0b
#define REG_DEVICE_ID       0x0c

#define DUTY_NUM 25
#define LM3644_NAME "leds-LM3644"

#ifndef GPIO_CAMERA_FLASH_EN_PIN
#define GPIO_CAMERA_FLASH_EN_PIN (201 | 0x80000000)
#endif

#ifndef GPIO_CAMERA_FLASH_EN_PIN_M_GPIO
#define GPIO_CAMERA_FLASH_EN_PIN_M_GPIO GPIO_MODE_00
#endif

static DEFINE_SPINLOCK(g_strobeSMPLock);
static DEFINE_MUTEX(g_strobeSem);

static u32 strobe_Res;
static u32 strobe_Timeus;
static bool g_strobe_On;
static int gDuty;
static int g_timeOutTimeMs;
static bool torch_flag;
static struct work_struct workTimeOut;
static struct hrtimer g_timeOutTimer;

static int gLedDuty[DUTY_NUM] = {
	0x11, 0x22, 0x33, 0x43, 0x54, 0x66, 0x0f, 0x13, 0x16, 0x19,
	0x1c, 0x20, 0x24, 0x28, 0x2c, 0x31, 0x36, 0x3b, 0x41, 0x46,
	0x4b, 0x50, 0x55, 0x5a, 0x61,
};

static int gFlashDuty[DUTY_NUM] = {
	0x01, 0x03, 0x05, 0x07, 0x09, 0x0c, 0x0f, 0x13, 0x16, 0x19,
	0x1c, 0x20, 0x24, 0x28, 0x2c, 0x31, 0x36, 0x3b, 0x41, 0x46,
	0x4b, 0x50, 0x55, 0x5a, 0x61,
};

struct LM3644_platform_data {
	u8 torch_pin_enable;
	u8 pam_sync_pin_enable;
	u8 thermal_comp_mode_enable;
	u8 strobe_pin_disable;
	u8 vout_mode_enable;
};

struct LM3644_chip_data {
	struct i2c_client *client;
	struct LM3644_platform_data *pdata;
	struct mutex lock;
	u8 last_flag;
	u8 no_pdata;
};

static struct i2c_client *LM3644_i2c_client;

static int LM3644_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	struct LM3644_chip_data *chip;
	int ret;

	if (!client)
		return -ENODEV;

	chip = i2c_get_clientdata(client);
	if (!chip)
		return -ENODEV;

	mutex_lock(&chip->lock);
	ret = i2c_smbus_write_byte_data(client, reg, val);
	mutex_unlock(&chip->lock);

	if (ret < 0)
		PK_ERR("failed writing reg=0x%02x ret=%d addr=0x%02x\n",
		       reg, ret, client->addr);
	return ret;
}

static int LM3644_read_reg(struct i2c_client *client, u8 reg)
{
	struct LM3644_chip_data *chip;
	int val;

	if (!client)
		return -ENODEV;

	chip = i2c_get_clientdata(client);
	if (!chip)
		return -ENODEV;

	mutex_lock(&chip->lock);
	val = i2c_smbus_read_byte_data(client, reg);
	mutex_unlock(&chip->lock);

	if (val < 0)
		PK_ERR("failed reading reg=0x%02x ret=%d addr=0x%02x\n",
		       reg, val, client->addr);
	return val;
}

static void LM3644_dump_state(const char *stage)
{
	int enable = LM3644_read_reg(LM3644_i2c_client, REG_ENABLE);
	int ivfm = LM3644_read_reg(LM3644_i2c_client, REG_IVFM);
	int flash1 = LM3644_read_reg(LM3644_i2c_client, REG_FLASH_LED1_BR);
	int flash2 = LM3644_read_reg(LM3644_i2c_client, REG_FLASH_LED2_BR);
	int torch1 = LM3644_read_reg(LM3644_i2c_client, REG_TORCH_LED1_BR);
	int torch2 = LM3644_read_reg(LM3644_i2c_client, REG_TORCH_LED2_BR);
	int boost = LM3644_read_reg(LM3644_i2c_client, REG_BOOST_CONFIG);
	int tout = LM3644_read_reg(LM3644_i2c_client, REG_FLASH_TOUT);
	int temp = LM3644_read_reg(LM3644_i2c_client, REG_TEMP);
	int flag0 = LM3644_read_reg(LM3644_i2c_client, REG_FLAG0);
	int flag1 = LM3644_read_reg(LM3644_i2c_client, REG_FLAG1);
	int device = LM3644_read_reg(LM3644_i2c_client, REG_DEVICE_ID);

	PK_DBG("M2NOTE_FLASH_LM3644_READBACK stage=%s enable=0x%02x ivfm=0x%02x flash1=0x%02x flash2=0x%02x torch1=0x%02x torch2=0x%02x boost=0x%02x tout=0x%02x temp=0x%02x flag0=0x%02x flag1=0x%02x device=0x%02x torch_flag=%d duty=%d on=%d\n",
	       stage, enable, ivfm, flash1, flash2, torch1, torch2, boost,
	       tout, temp, flag0, flag1, device, torch_flag, gDuty,
	       g_strobe_On);
}

static void LM3644_dump_gpio(const char *stage)
{
	unsigned long pin = GPIO_CAMERA_FLASH_EN_PIN;

	PK_DBG("M2NOTE_FLASH_LM3644_GPIO stage=%s pin=201 mode=%d dir=%d out=%d in=%d\n",
	       stage, mt_get_gpio_mode(pin), mt_get_gpio_dir(pin),
	       mt_get_gpio_out(pin), mt_get_gpio_in(pin));
}

void LM3644_set_torch_mode_from_timeout(unsigned int timeout_ms,
					const char *stage)
{
	torch_flag = (timeout_ms == 20000 || timeout_ms == 0);
	PK_DBG("M2NOTE_FLASH_LM3644 stage=torch_mode_source source=%s timeout_ms=%u torch_flag=%d\n",
	       stage, timeout_ms, torch_flag);
}

static void LM3644_enable_gpio(void)
{
	mt_set_gpio_mode(GPIO_CAMERA_FLASH_EN_PIN, GPIO_CAMERA_FLASH_EN_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CAMERA_FLASH_EN_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CAMERA_FLASH_EN_PIN, GPIO_OUT_ONE);
	PK_DBG("M2NOTE_FLASH_LM3644_GPIO stage=enable pin=201\n");
	LM3644_dump_gpio("enable_readback");
	LM3644_dump_state("gpio_enable");
}

static void LM3644_disable_gpio(void)
{
	mt_set_gpio_mode(GPIO_CAMERA_FLASH_EN_PIN, GPIO_CAMERA_FLASH_EN_PIN_M_GPIO);
	mt_set_gpio_dir(GPIO_CAMERA_FLASH_EN_PIN, GPIO_DIR_OUT);
	mt_set_gpio_out(GPIO_CAMERA_FLASH_EN_PIN, GPIO_OUT_ZERO);
	PK_DBG("M2NOTE_FLASH_LM3644_GPIO stage=disable pin=201\n");
	LM3644_dump_gpio("disable_readback");
	LM3644_dump_state("gpio_disable");
}

int FL_dim_duty_led1(unsigned int duty)
{
	u8 reg;
	u8 val;
	int ret;

	if (duty >= DUTY_NUM)
		duty = DUTY_NUM - 1;

	gDuty = duty;
	if (torch_flag) {
		reg = REG_TORCH_LED1_BR;
		val = gLedDuty[gDuty];
		PK_DBG("M2NOTE_FLASH_LM3644 stage=duty_led1 mode=torch duty=%u val=0x%02x\n",
		       duty, val);
	} else {
		reg = REG_FLASH_LED1_BR;
		val = gFlashDuty[gDuty];
		PK_DBG("M2NOTE_FLASH_LM3644 stage=duty_led1 mode=flash duty=%u val=0x%02x\n",
		       duty, val);
	}

	ret = LM3644_write_reg(LM3644_i2c_client, reg, val);
	LM3644_dump_state("duty_led1");
	return ret;
}

int FL_dim_duty_led2(unsigned int duty)
{
	u8 reg;
	u8 val;
	int old;
	int ret;

	if (duty >= DUTY_NUM)
		duty = DUTY_NUM - 1;

	gDuty = duty;
	if (torch_flag) {
		reg = REG_TORCH_LED1_BR;
		old = LM3644_read_reg(LM3644_i2c_client, reg);
		if (old < 0)
			return old;
		ret = LM3644_write_reg(LM3644_i2c_client, reg, old & 0x7f);
		if (ret < 0)
			return ret;

		reg = REG_TORCH_LED2_BR;
		val = gLedDuty[gDuty];
		PK_DBG("M2NOTE_FLASH_LM3644 stage=duty_led2 mode=torch duty=%u val=0x%02x\n",
		       duty, val);
	} else {
		reg = REG_FLASH_LED1_BR;
		old = LM3644_read_reg(LM3644_i2c_client, reg);
		if (old < 0)
			return old;
		ret = LM3644_write_reg(LM3644_i2c_client, reg, old & 0x7f);
		if (ret < 0)
			return ret;

		reg = REG_FLASH_LED2_BR;
		val = gFlashDuty[gDuty];
		PK_DBG("M2NOTE_FLASH_LM3644 stage=duty_led2 mode=flash duty=%u val=0x%02x\n",
		       duty, val);
	}

	ret = LM3644_write_reg(LM3644_i2c_client, reg, val);
	LM3644_dump_state("duty_led2");
	return ret;
}

int FL_Enable_led1(void)
{
	int val;
	int ret;

	val = LM3644_read_reg(LM3644_i2c_client, REG_ENABLE);
	if (val < 0)
		return val;

	if (torch_flag) {
		val |= 0x09;
		PK_DBG("M2NOTE_FLASH_LM3644 stage=enable_led1 mode=torch\n");
	} else {
		val |= 0x0d;
		PK_DBG("M2NOTE_FLASH_LM3644 stage=enable_led1 mode=flash\n");
	}

	ret = LM3644_write_reg(LM3644_i2c_client, REG_ENABLE, val);
	LM3644_dump_state("enable_led1");
	usleep_range(5000, 6000);
	LM3644_dump_state("enable_led1_settled");
	return ret;
}

int FL_Enable_led2(void)
{
	int val;
	int ret;

	val = LM3644_read_reg(LM3644_i2c_client, REG_ENABLE);
	if (val < 0)
		return val;

	if (torch_flag) {
		val |= 0x0a;
		PK_DBG("M2NOTE_FLASH_LM3644 stage=enable_led2 mode=torch\n");
	} else {
		val |= 0x0e;
		PK_DBG("M2NOTE_FLASH_LM3644 stage=enable_led2 mode=flash\n");
	}

	ret = LM3644_write_reg(LM3644_i2c_client, REG_ENABLE, val);
	LM3644_dump_state("enable_led2");
	usleep_range(5000, 6000);
	LM3644_dump_state("enable_led2_settled");
	return ret;
}

int FL_Disable_led1(void)
{
	int val;
	int ret;

	val = LM3644_read_reg(LM3644_i2c_client, REG_ENABLE);
	if (val < 0)
		return val;

	PK_DBG("M2NOTE_FLASH_LM3644 stage=disable_led1\n");
	ret = LM3644_write_reg(LM3644_i2c_client, REG_ENABLE, val & 0xfe);
	LM3644_dump_state("disable_led1");
	return ret;
}

int FL_Disable_led2(void)
{
	int val;
	int ret;

	val = LM3644_read_reg(LM3644_i2c_client, REG_ENABLE);
	if (val < 0)
		return val;

	PK_DBG("M2NOTE_FLASH_LM3644 stage=disable_led2\n");
	ret = LM3644_write_reg(LM3644_i2c_client, REG_ENABLE, val & 0xfd);
	LM3644_dump_state("disable_led2");
	return ret;
}

int FL_Enable(void)
{
	return FL_Enable_led1();
}

int FL_dim_duty(unsigned int duty)
{
	return FL_dim_duty_led1(duty);
}

int FL_Disable(void)
{
	return FL_Disable_led1();
}

int FL_Init(void)
{
	int ret;

	if (!LM3644_i2c_client) {
		PK_ERR("M2NOTE_FLASH_LM3644 stage=fl_init_missing_client\n");
		return -ENODEV;
	}

	ret = LM3644_write_reg(LM3644_i2c_client, REG_FLASH_TOUT, 0x0f);
	PK_DBG("M2NOTE_FLASH_LM3644 stage=fl_init ret=%d\n", ret);
	LM3644_dump_state("fl_init");
	return ret;
}

int FL_Uninit(void)
{
	return FL_Disable();
}

static void work_timeOutFunc(struct work_struct *data)
{
	FL_Disable();
	PK_DBG("M2NOTE_FLASH_LM3644 stage=timeout\n");
}

enum hrtimer_restart ledTimeOutCallback(struct hrtimer *timer)
{
	schedule_work(&workTimeOut);
	return HRTIMER_NORESTART;
}

static void timerInit(void)
{
	INIT_WORK(&workTimeOut, work_timeOutFunc);
	g_timeOutTimeMs = 1000;
	hrtimer_init(&g_timeOutTimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	g_timeOutTimer.function = ledTimeOutCallback;
}

static int constant_flashlight_ioctl(unsigned int cmd, unsigned long arg)
{
	int i4RetValue = 0;
	int ior_shift;
	int iow_shift;
	int iowr_shift;

	ior_shift = cmd - (_IOR(FLASHLIGHT_MAGIC, 0, int));
	iow_shift = cmd - (_IOW(FLASHLIGHT_MAGIC, 0, int));
	iowr_shift = cmd - (_IOWR(FLASHLIGHT_MAGIC, 0, int));
	PK_DBG("M2NOTE_FLASH_LM3644 stage=ioctl cmd=0x%x ior=%d iow=%d iowr=%d arg=%lu\n",
	       cmd, ior_shift, iow_shift, iowr_shift, arg);

	switch (cmd) {
	case FLASH_IOC_SET_TIME_OUT_TIME_MS:
		PK_DBG("FLASH_IOC_SET_TIME_OUT_TIME_MS: %d\n", (int)arg);
		LM3644_set_torch_mode_from_timeout(arg, "sid1_timeout");
		g_timeOutTimeMs = arg;
		break;

	case FLASH_IOC_SET_DUTY:
		PK_DBG("FLASHLIGHT_DUTY: %d\n", (int)arg);
		i4RetValue = FL_dim_duty(arg);
		break;

	case FLASH_IOC_SET_STEP:
		PK_DBG("FLASH_IOC_SET_STEP: %d\n", (int)arg);
		break;

	case FLASH_IOC_SET_ONOFF:
		PK_DBG("FLASHLIGHT_ONOFF: %d\n", (int)arg);
		if (arg == 1) {
			if (g_timeOutTimeMs != 0) {
				ktime_t ktime;

				ktime = ktime_set(0, g_timeOutTimeMs * 1000000);
				hrtimer_start(&g_timeOutTimer, ktime, HRTIMER_MODE_REL);
			}
			i4RetValue = FL_Enable();
			if (i4RetValue >= 0)
				g_strobe_On = true;
		} else {
			i4RetValue = FL_Disable();
			if (i4RetValue >= 0)
				g_strobe_On = false;
			hrtimer_cancel(&g_timeOutTimer);
		}
		break;

	case FLASH_IOC_SET_REG_ADR:
	case FLASH_IOC_SET_REG_VAL:
	case FLASH_IOC_SET_REG:
		break;

	case FLASH_IOC_GET_REG:
		i4RetValue = LM3644_read_reg(LM3644_i2c_client, arg);
		PK_DBG("FLASH_IOC_GET_REG arg=%d ret=%d\n", (int)arg, i4RetValue);
		break;

	default:
		PK_DBG("No such command\n");
		i4RetValue = -EPERM;
		break;
	}
	return i4RetValue;
}

static int constant_flashlight_open(void *pArg)
{
	int i4RetValue = 0;

	PK_DBG("constant_flashlight_open\n");

	if (strobe_Res == 0) {
		i4RetValue = FL_Init();
		if (i4RetValue < 0) {
			PK_ERR("M2NOTE_FLASH_LM3644 stage=open_init_failed ret=%d\n",
			       i4RetValue);
			return i4RetValue;
		}
		timerInit();
	}

	spin_lock_irq(&g_strobeSMPLock);
	if (strobe_Res) {
		PK_ERR("busy\n");
		i4RetValue = -EBUSY;
	} else {
		strobe_Res += 1;
	}
	spin_unlock_irq(&g_strobeSMPLock);

	return i4RetValue;
}

static int constant_flashlight_release(void *pArg)
{
	PK_DBG("constant_flashlight_release\n");

	if (strobe_Res) {
		spin_lock_irq(&g_strobeSMPLock);
		strobe_Res = 0;
		strobe_Timeus = 0;
		g_strobe_On = false;
		spin_unlock_irq(&g_strobeSMPLock);

		FL_Uninit();
	}

	return 0;
}

static FLASHLIGHT_FUNCTION_STRUCT constantFlashlightFunc = {
	constant_flashlight_open,
	constant_flashlight_release,
	constant_flashlight_ioctl
};

MUINT32 constantFlashlightInit(PFLASHLIGHT_FUNCTION_STRUCT *pfFunc)
{
	if (pfFunc != NULL)
		*pfFunc = &constantFlashlightFunc;
	return 0;
}

ssize_t strobe_VDIrq(void)
{
	return 0;
}
EXPORT_SYMBOL(strobe_VDIrq);

static int LM3644_chip_init(struct LM3644_chip_data *chip)
{
	return 0;
}

static int LM3644_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct LM3644_chip_data *chip;
	struct LM3644_platform_data *pdata = client->dev.platform_data;
	int err = -1;

	PK_DBG("M2NOTE_FLASH_LM3644 stage=probe_start addr=0x%02x bus=%d\n",
	       client->addr, client->adapter ? client->adapter->nr : -1);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		PK_ERR("LM3644 i2c functionality check failed\n");
		return -ENODEV;
	}

	chip = kzalloc(sizeof(struct LM3644_chip_data), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;
	chip->client = client;

	mutex_init(&chip->lock);
	i2c_set_clientdata(client, chip);

	if (pdata == NULL) {
		PK_DBG("LM3644 platform data missing, using zero defaults\n");
		pdata = kzalloc(sizeof(struct LM3644_platform_data), GFP_KERNEL);
		if (!pdata) {
			err = -ENOMEM;
			goto err_alloc_pdata;
		}
		chip->no_pdata = 1;
	}

	chip->pdata = pdata;
	err = LM3644_chip_init(chip);
	if (err < 0)
		goto err_chip_init;

	LM3644_i2c_client = client;
	LM3644_enable_gpio();
	PK_DBG("M2NOTE_FLASH_LM3644 stage=probe_done addr=0x%02x bus=%d\n",
	       client->addr, client->adapter ? client->adapter->nr : -1);
	return 0;

err_chip_init:
	if (chip->no_pdata)
		kfree(chip->pdata);
err_alloc_pdata:
	i2c_set_clientdata(client, NULL);
	kfree(chip);
	PK_ERR("LM3644 probe failed err=%d\n", err);
	return err;
}

static int LM3644_remove(struct i2c_client *client)
{
	struct LM3644_chip_data *chip = i2c_get_clientdata(client);

	LM3644_disable_gpio();
	LM3644_i2c_client = NULL;
	if (chip) {
		if (chip->no_pdata)
			kfree(chip->pdata);
		kfree(chip);
	}
	return 0;
}

static const struct i2c_device_id LM3644_id[] = {
	{LM3644_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, LM3644_id);

#ifdef CONFIG_OF
static const struct of_device_id LM3644_of_match[] = {
	{.compatible = "mediatek,strobe_main"},
	{.compatible = "mediatek,STROBE_MAIN"},
	{},
};
MODULE_DEVICE_TABLE(of, LM3644_of_match);
#endif

static struct i2c_driver LM3644_i2c_driver = {
	.driver = {
		.name = LM3644_NAME,
#ifdef CONFIG_OF
		.of_match_table = LM3644_of_match,
#endif
	},
	.probe = LM3644_probe,
	.remove = LM3644_remove,
	.id_table = LM3644_id,
};

static int __init LM3644_init(void)
{
	int ret;

	PK_DBG("M2NOTE_FLASH_LM3644 stage=driver_init\n");
	ret = i2c_add_driver(&LM3644_i2c_driver);
	PK_DBG("M2NOTE_FLASH_LM3644 stage=driver_init_done ret=%d\n", ret);
	return ret;
}

static void __exit LM3644_exit(void)
{
	PK_DBG("M2NOTE_FLASH_LM3644 stage=driver_exit\n");
	i2c_del_driver(&LM3644_i2c_driver);
}

module_init(LM3644_init);
module_exit(LM3644_exit);

MODULE_DESCRIPTION("m2note stock LM3644 flash driver");
MODULE_AUTHOR("Meizu/MediaTek; adapted for m2note 3.18");
MODULE_LICENSE("GPL v2");
