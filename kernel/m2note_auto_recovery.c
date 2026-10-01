/*
 * M2 Note bring-up watchdog.
 *
 * This is a diagnostic capture aid: preserve the target kernel log and reboot
 * into recovery on a fixed timer so Build Station can pull
 * pstore/last_kmsg/expdb even when userspace starts but ADB remains offline.
 */

#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/kmsg_dump.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/proc_fs.h>
#include <linux/reboot.h>
#include <linux/timer.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <mt-plat/aee.h>
#include <mt-plat/mtk_ram_console.h>
#include <mt-plat/mtk_rtc.h>

#define M2NOTE_AUTO_RECOVERY_PROC "m2note_auto_recovery"
#define M2NOTE_AUTO_RECOVERY_TAG "v195-manual-arm"

static unsigned int timeout_sec = 300;
module_param(timeout_sec, uint, 0644);

static unsigned int hard_timeout_grace_sec = 15;
module_param(hard_timeout_grace_sec, uint, 0644);

static bool arm_on_boot;
module_param(arm_on_boot, bool, 0644);

extern void wdt_arch_reset(char mode);
static atomic_t enabled = ATOMIC_INIT(0);
static atomic_t hard_timer_fired = ATOMIC_INIT(0);
static atomic_t marker_seq = ATOMIC_INIT(0);
static struct proc_dir_entry *proc_entry;
static struct timer_list recovery_hard_timer;

static void m2note_auto_recovery_work(struct work_struct *work);
static DECLARE_DELAYED_WORK(recovery_work, m2note_auto_recovery_work);

static void m2note_auto_recovery_emit(const char *stage, const char *action,
				      bool record_rr)
{
	unsigned int seq = atomic_inc_return(&marker_seq);

	pr_emerg("M2NOTE_AUTO_RECOVERY_TRACE tag=%s seq=%u stage=%s timeout_sec=%u hard_grace_sec=%u action=%s\n",
		 M2NOTE_AUTO_RECOVERY_TAG, seq, stage, timeout_sec,
		 hard_timeout_grace_sec, action);
	aee_sram_printk("M2NOTE_AUTO_RECOVERY_TRACE tag=%s seq=%u stage=%s timeout_sec=%u hard_grace_sec=%u action=%s\n",
			M2NOTE_AUTO_RECOVERY_TAG, seq, stage, timeout_sec,
			hard_timeout_grace_sec, action);

	if (record_rr) {
		aee_rr_rec_reboot_mode(AEE_REBOOT_MODE_WDT);
		aee_rr_rec_fiq_step(AEE_FIQ_STEP_KE_WDT_LOG);
	}
}

static void m2note_auto_recovery_note(const char *stage, const char *action)
{
	m2note_auto_recovery_emit(stage, action, false);
}

static void m2note_auto_recovery_record_rr(void)
{
	aee_rr_rec_reboot_mode(AEE_REBOOT_MODE_WDT);
	aee_rr_rec_fiq_step(AEE_FIQ_STEP_KE_WDT_LOG);
}

static void m2note_auto_recovery_hard_timer(unsigned long data)
{
	if (!atomic_read(&enabled))
		return;

	if (atomic_xchg(&hard_timer_fired, 1))
		return;

	m2note_auto_recovery_record_rr();
	rtc_mark_recovery();
	kmsg_dump(KMSG_DUMP_EMERG);
	mdelay(250);
	wdt_arch_reset(1);
	emergency_restart();
}

static void m2note_schedule_recovery(void)
{
	unsigned long timeout_jiffies;
	unsigned long hard_timeout_jiffies;

	if (!timeout_sec)
		return;

	timeout_jiffies = msecs_to_jiffies(timeout_sec * 1000);
	hard_timeout_jiffies = timeout_jiffies +
		msecs_to_jiffies(hard_timeout_grace_sec * 1000);

	atomic_set(&hard_timer_fired, 0);
	schedule_delayed_work(&recovery_work, timeout_jiffies);
	mod_timer(&recovery_hard_timer, jiffies + hard_timeout_jiffies);
	m2note_auto_recovery_note("armed", "schedule");
}

static void m2note_auto_recovery_work(struct work_struct *work)
{
	if (!atomic_read(&enabled)) {
		m2note_auto_recovery_note("timeout", "skip_disabled");
		return;
	}

	m2note_auto_recovery_record_rr();
	rtc_mark_recovery();
	kmsg_dump(KMSG_DUMP_OOPS);
	mdelay(250);
	wdt_arch_reset(1);
	emergency_restart();
}

static ssize_t m2note_auto_recovery_read(struct file *file, char __user *buf,
					 size_t count, loff_t *ppos)
{
	char tmp[160];
	int len;

	len = snprintf(tmp, sizeof(tmp),
		       "enabled=%d arm_on_boot=%u timeout_sec=%u hard_grace_sec=%u reset=wdt_arch_reset hard=wdt_arch_reset\n",
		       atomic_read(&enabled), arm_on_boot ? 1 : 0, timeout_sec,
		       hard_timeout_grace_sec);
	return simple_read_from_buffer(buf, count, ppos, tmp, len);
}

static ssize_t m2note_auto_recovery_write(struct file *file,
					  const char __user *buf,
					  size_t count, loff_t *ppos)
{
	char tmp[16];
	size_t len = min(count, sizeof(tmp) - 1);

	if (copy_from_user(tmp, buf, len))
		return -EFAULT;

	tmp[len] = '\0';
	if (tmp[0] == '0') {
		if (atomic_xchg(&enabled, 0))
			m2note_auto_recovery_note("disabled", "proc");
		cancel_delayed_work_sync(&recovery_work);
		del_timer_sync(&recovery_hard_timer);
	} else if (tmp[0] == '1') {
		atomic_set(&enabled, 1);
		cancel_delayed_work_sync(&recovery_work);
		del_timer_sync(&recovery_hard_timer);
		m2note_schedule_recovery();
	}

	return count;
}

static const struct file_operations m2note_auto_recovery_fops = {
	.owner = THIS_MODULE,
	.read = m2note_auto_recovery_read,
	.write = m2note_auto_recovery_write,
};

static int __init m2note_auto_recovery_init(void)
{
	atomic_set(&enabled, arm_on_boot ? 1 : 0);

	setup_timer(&recovery_hard_timer, m2note_auto_recovery_hard_timer, 0);

	proc_entry = proc_create(M2NOTE_AUTO_RECOVERY_PROC, 0664, NULL,
				 &m2note_auto_recovery_fops);
	if (!proc_entry)
		m2note_auto_recovery_note("init", "proc_create_failed");

	if (arm_on_boot)
		m2note_schedule_recovery();
	else
		m2note_auto_recovery_note("init", "boot_arm_disabled");

	return 0;
}
subsys_initcall(m2note_auto_recovery_init);
