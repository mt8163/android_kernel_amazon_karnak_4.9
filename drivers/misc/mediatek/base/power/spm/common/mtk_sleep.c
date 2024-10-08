/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/suspend.h>
#include <linux/console.h>

#include <mtk_sleep_internal.h>
#include <mtk_spm_suspend_internal.h>
#include <mtk_idle_sysfs.h>
#include <mtk_power_gs_api.h>
#include <mtk_idle.h>
#include <mtk_idle_internal.h>
#ifdef CONFIG_MTK_SND_SOC_NEW_ARCH
#include <mtk-soc-afe-control.h>
#endif /* CONFIG_MTK_SND_SOC_NEW_ARCH */
#include <mtk_mcdi_api.h>

static DEFINE_SPINLOCK(slp_lock);

unsigned long slp_dp_cnt[NR_CPUS] = {0};
static unsigned int slp_wake_reason = WR_NONE;

static bool slp_suspend_ops_valid_on;
static bool slp_ck26m_on;

bool slp_dump_gpio;
bool slp_dump_golden_setting;
int slp_dump_golden_setting_type = GS_PMIC;

/* FIXME: */
static u32 slp_spm_flags = {
	/* SPM_FLAG_DIS_CPU_PDN | */
	/* SPM_FLAG_DIS_INFRA_PDN | */
	/* SPM_FLAG_DIS_DDRPHY_PDN | */
	SPM_FLAG_DIS_VCORE_DVS |
	SPM_FLAG_DIS_VCORE_DFS |
	/* SPM_FLAG_DIS_VPROC_VSRAM_DVS | */
	SPM_FLAG_DIS_ATF_ABORT |
	SPM_FLAG_SUSPEND_OPTION
};

static u32 slp_spm_flags1 = {
	0
};

static u32 slp_spm_flags1;
static int slp_suspend_ops_valid(suspend_state_t state)
{
	if (slp_suspend_ops_valid_on)
		return state == PM_SUSPEND_MEM;
	else
		return 0;
}

static int slp_suspend_ops_begin(suspend_state_t state)
{
	/* legacy log */
	pr_info("[SLP] @@@@@@@@@@@@@@@@\tChip_pm_begin(%u)(%u)\t@@@@@@@@@@@@@@@@\n",
			is_cpu_pdn(slp_spm_flags), is_infra_pdn(slp_spm_flags));

	slp_wake_reason = WR_NONE;

	return 0;
}

static int slp_suspend_ops_prepare(void)
{
#if 0
	/* legacy log */
	pr_debug("[SLP] @@@@@@@@@@@@@@@@\tChip_pm_prepare\t@@@@@@@@@@@@@@@@\n");
#endif
	return 0;
}

#ifdef CONFIG_MTK_SND_SOC_NEW_ARCH
bool __attribute__ ((weak)) ConditionEnterSuspend(void)
{
	pr_info("NO %s !!!\n", __func__);
	return true;
}
#endif /* MTK_SUSPEND_AUDIO_SUPPORT */

#ifdef CONFIG_MTK_SYSTRACKER
void __attribute__ ((weak)) systracker_enable(void)
{
	pr_info("NO %s !!!\n", __func__);
}
#endif /* CONFIG_MTK_SYSTRACKER */

#ifdef CONFIG_MTK_BUS_TRACER
void __attribute__ ((weak)) bus_tracer_enable(void)
{
	pr_info("NO %s !!!\n", __func__);
}
#endif /* CONFIG_MTK_BUS_TRACER */

void __attribute__((weak)) subsys_if_on(void)
{
	pr_info("NO %s !!!\n", __func__);
}

void __attribute__((weak)) pll_if_on(void)
{
	pr_info("NO %s !!!\n", __func__);
}

void __attribute__((weak))
gpio_dump_regs(void)
{

}

void __attribute__((weak))
spm_output_sleep_option(void)
{

}

int __attribute__((weak))
spm_set_sleep_wakesrc(u32 wakesrc, bool enable, bool replace)
{
	return 0;
}

bool __attribute__((weak)) spm_is_enable_sleep(void)
{
	pr_info("NO %s !!!\n", __func__);
	return false;
}

unsigned int __attribute__((weak))
spm_go_to_sleep(u32 spm_flags, u32 spm_data)
{
	return 0;
}

static int slp_suspend_ops_enter(suspend_state_t state)
{
	int ret = 0;

#if SLP_SLEEP_DPIDLE_EN
#ifdef CONFIG_MTK_SND_SOC_NEW_ARCH
	int fm_radio_is_playing = 0;

	if (ConditionEnterSuspend() == true)
		fm_radio_is_playing = 0;
	else
		fm_radio_is_playing = 1;
#endif /* CONFIG_MTK_SND_SOC_NEW_ARCH */
#endif

#if 0
	/* legacy log */
	pr_debug("[SLP] @@@@@@@@@@@@@@@\tChip_pm_enter\t@@@@@@@@@@@@@@@\n");
#endif

#if !defined(CONFIG_FPGA_EARLY_PORTING)
	if (slp_dump_gpio)
		gpio_dump_regs();
#endif /* CONFIG_FPGA_EARLY_PORTING */

#if !defined(CONFIG_FPGA_EARLY_PORTING)
	pll_if_on();
	subsys_if_on();
#endif /* CONFIG_FPGA_EARLY_PORTING */

	if (is_infra_pdn(slp_spm_flags) && !is_cpu_pdn(slp_spm_flags)) {
		pr_info("[SLP] CANNOT SLEEP DUE TO INFRA PDN BUT CPU PON\n");
		ret = -EPERM;
		goto LEAVE_SLEEP;
	}

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	if (is_sspm_ipi_lock_spm()) {
		pr_info("[SLP] CANNOT SLEEP DUE TO SSPM IPI\n");
		ret = -EPERM;
		goto LEAVE_SLEEP;
	}
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */

#if !defined(CONFIG_FPGA_EARLY_PORTING)
	if (spm_load_firmware_status() < 1) {
		pr_info("SPM FIRMWARE IS NOT READY\n");
		ret = -EPERM;
		goto LEAVE_SLEEP;
	}
#endif /* CONFIG_FPGA_EARLY_PORTING */

	mcdi_task_pause(true);

#if SLP_SLEEP_DPIDLE_EN
#ifdef CONFIG_MTK_SND_SOC_NEW_ARCH
	if (slp_ck26m_on | fm_radio_is_playing) {
#else
	if (slp_ck26m_on) {
#endif /* CONFIG_MTK_SND_SOC_NEW_ARCH */
		mtk_idle_enter(IDLE_TYPE_DP, smp_processor_id(),
					MTK_IDLE_OPT_SLEEP_DPIDLE, 0);
		slp_wake_reason = get_slp_dp_last_wr();
		slp_dp_cnt[smp_processor_id()]++;

	} else
#endif

		slp_wake_reason = spm_go_to_sleep(slp_spm_flags,
			slp_spm_flags1);

	mcdi_task_pause(false);

LEAVE_SLEEP:
#if !defined(CONFIG_FPGA_EARLY_PORTING)
#ifdef CONFIG_MTK_SYSTRACKER
	systracker_enable();
#endif
#ifdef CONFIG_MTK_BUS_TRACER
	bus_tracer_enable();
#endif
#endif /* CONFIG_FPGA_EARLY_PORTING */

	return ret;
}

static void slp_suspend_ops_finish(void)
{
#if 0
	/* legacy log */
	pr_debug("[SLP] @@@@@@@@@@@@@@\tChip_pm_finish\t@@@@@@@@@@@@\n");
#endif
}

static void slp_suspend_ops_end(void)
{
#if 0
	/* legacy log */
	pr_debug("[SLP] @@@@@@@@@@@@@@\tChip_pm_end\t@@@@@@@@@@@@@@\n");
#endif
}

static const struct platform_suspend_ops slp_suspend_ops = {
	.valid = slp_suspend_ops_valid,
	.begin = slp_suspend_ops_begin,
	.prepare = slp_suspend_ops_prepare,
	.enter = slp_suspend_ops_enter,
	.finish = slp_suspend_ops_finish,
	.end = slp_suspend_ops_end,
};

__attribute__ ((weak))
int spm_set_dpidle_wakesrc(u32 wakesrc, bool enable, bool replace)
{
	pr_info("NO %s !!!\n", __func__);
	return 0;
}

/*
 * wakesrc : WAKE_SRC_XXX
 * enable  : enable or disable @wakesrc
 * ck26m_on: if true, mean @wakesrc needs 26M to work
 */
int slp_set_wakesrc(u32 wakesrc, bool enable, bool ck26m_on)
{
	int r;
	unsigned long flags;

	pr_info("[SLP] wakesrc = 0x%x, enable = %u, ck26m_on = %u\n",
		wakesrc, enable, ck26m_on);

#if SLP_REPLACE_DEF_WAKESRC
	if (wakesrc & WAKE_SRC_CFG_KEY)
#else
	if (!(wakesrc & WAKE_SRC_CFG_KEY))
#endif
		return -EPERM;

	spin_lock_irqsave(&slp_lock, flags);

#if SLP_REPLACE_DEF_WAKESRC
	if (ck26m_on)
		r = spm_set_dpidle_wakesrc(wakesrc, enable, true);
	else
		r = spm_set_sleep_wakesrc(wakesrc, enable, true);
#else
	if (ck26m_on)
		r = spm_set_dpidle_wakesrc(wakesrc & ~WAKE_SRC_CFG_KEY,
				enable, false);
	else
		r = spm_set_sleep_wakesrc(wakesrc & ~WAKE_SRC_CFG_KEY, enable,
				false);
#endif

	if (!r)
		slp_ck26m_on = ck26m_on;
	spin_unlock_irqrestore(&slp_lock, flags);
	return r;
}

unsigned int slp_get_wake_reason(void)
{
	return slp_wake_reason;
}
EXPORT_SYMBOL(slp_get_wake_reason);

/*
 * debugfs
 */
static ssize_t suspend_state_read(char *ToUserBuf, size_t sz, void *priv)
{
	char *p = ToUserBuf;
	int i;

	if (!ToUserBuf)
		return -EINVAL;
	#undef log
	#define log(fmt, args...) ({\
		p += scnprintf(p, sz - strlen(ToUserBuf), fmt, ##args); p; })

	log("*********** suspend state ************\n");
	log("suspend valid status = %d\n",
		       slp_suspend_ops_valid_on);
	log("*************** slp dp cnt ***********\n");
	for (i = 0; i < nr_cpu_ids; i++)
		log("cpu%d: slp_dp=%lu\n", i, slp_dp_cnt[i]);

	log("*********** suspend command ************\n");
	log("echo suspend 1/0 > /sys/kernel/debug/cpuidle/slp/suspend_state\n");

	return p - ToUserBuf;
}

static ssize_t suspend_state_write(char *FromUserBuf, size_t sz, void *priv)
{
	char cmd[128];
	int param;

	if (!FromUserBuf)
		return -EINVAL;

	if (sscanf(FromUserBuf, "%127s %d", cmd, &param) == 2) {
		if (!strcmp(cmd, "suspend")) {
			/* update suspend valid status */
			slp_suspend_ops_valid_on = param;

			/* suspend reinit ops */
			suspend_set_ops(&slp_suspend_ops);
		}
		return sz;
	}

	return -EINVAL;
}

static const struct mtk_idle_sysfs_op suspend_state_fops = {
	.fs_read = suspend_state_read,
	.fs_write = suspend_state_write,
};

void slp_module_init(void)
{
	struct mtk_idle_sysfs_handle pParent2ND;
	struct mtk_idle_sysfs_handle *pParent = NULL;

	slp_suspend_ops_valid_on = spm_is_enable_sleep();

	mtk_idle_sysfs_entry_create();
	if (mtk_idle_sysfs_entry_root_get(&pParent) == 0) {
		mtk_idle_sysfs_entry_func_create("slp", 0444
			, pParent, &pParent2ND);
		mtk_idle_sysfs_entry_func_node_add("suspend_state", 0444
			, &suspend_state_fops, &pParent2ND, NULL);
	}

	spm_output_sleep_option();
	pr_info("[SLP] SLEEP_DPIDLE_EN:%d, REPLACE_DEF_WAKESRC:%d",
		SLP_SLEEP_DPIDLE_EN, SLP_REPLACE_DEF_WAKESRC);
	pr_info(", SUSPEND_LOG_EN:%d\n", SLP_SUSPEND_LOG_EN);
	suspend_set_ops(&slp_suspend_ops);
#if SLP_SUSPEND_LOG_EN
	console_suspend_enabled = 0;
#endif
#ifdef CONFIG_PM_SLEEP_DEBUG
	pm_print_times_enabled = false;
#endif
}

module_param(slp_ck26m_on, bool, 0644);
module_param(slp_spm_flags, uint, 0644);

module_param(slp_dump_gpio, bool, 0644);
module_param(slp_dump_golden_setting, bool, 0644);
module_param(slp_dump_golden_setting_type, int, 0644);

MODULE_DESCRIPTION("Sleep Driver v0.1");
