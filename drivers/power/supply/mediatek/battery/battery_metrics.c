#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/metricslog.h>
#include <linux/power_supply.h>
#include <mt-plat/charging.h>
#include <mt-plat/battery_common.h>
#include <mt-plat/battery_meter.h>
#ifdef CONFIG_AMAZON_SIGN_OF_LIFE
#include <linux/sign_of_life.h>
#endif
#ifdef CONFIG_FB
#include <linux/notifier.h>
#include <linux/fb.h>
#endif
#define BATTERY_METRICS_BUFF_SIZE 1024
static char g_metrics_buf[BATTERY_METRICS_BUFF_SIZE];

#if defined(CONFIG_AMAZON_MINERVA_METRICS_LOG)
#define BATTERY_METRICS_EVENT_SIZE 128
static char event_buf[BATTERY_METRICS_EVENT_SIZE];
#endif

#define bat_metrics_log(domain, fmt, ...)				\
do {									\
	memset(g_metrics_buf, 0 , BATTERY_METRICS_BUFF_SIZE);		\
	snprintf(g_metrics_buf, sizeof(g_metrics_buf),	fmt, ##__VA_ARGS__);\
	log_to_metrics(ANDROID_LOG_INFO, domain, g_metrics_buf);	\
} while (0)

struct screen_state {
	struct timespec screen_on_time;
	struct timespec screen_off_time;
	int screen_on_soc;
	int screen_off_soc;
};

struct pm_state {
	struct timespec suspend_ts;
	struct timespec resume_ts;
	int suspend_soc;
	int resume_soc;
	int suspend_bat_car;
	int resume_bat_car;
};

struct fg_slp_current_data {
	struct timespec last_ts;
	int slp_ma;
};

struct bat_metrics_data {
	bool is_top_off_mode;
	bool is_demo_mode;
	bool vbus_on_old;
	u8 fault_type_old;
	u32 chg_sts_old;
	u32 chg_type_old;

	struct screen_state screen;
	struct pm_state pm;
	struct battery_meter_data bat_data;
	struct fg_slp_current_data slp_curr_data;
#if defined(CONFIG_FB)
	struct notifier_block pm_notifier;
#endif
};
static struct bat_metrics_data metrics_data;

#if defined(CONFIG_AMAZON_MINERVA_METRICS_LOG)
#define bat_minerva_log(fmt, ...) \
do { \
	memset(event_buf, 0, BATTERY_METRICS_EVENT_SIZE); \
	snprintf(event_buf, BATTERY_METRICS_EVENT_SIZE - 1, fmt, ##__VA_ARGS__); \
	bat_metrics_minerva_log(event_buf); \
} while (0)

static void bat_metrics_minerva_log(char *event)
{
	char *fmt = "%s:%s:100:%s,%s,ui_soc=%d;IN,soc=%d;IN,temp=%d;IN,vbat=%d;IN,"
		"ibat=%d;IN,event=%s;SY:us-east-1";

	minerva_metrics_log(g_metrics_buf, BATTERY_METRICS_BUFF_SIZE, fmt,
		METRICS_BATTERY_GROUP_ID, METRICS_BATTERY_SCHEMA_ID,
		PREDEFINED_ESSENTIAL_KEY, PREDEFINED_TZ_KEY,
		BMT_status.UI_SOC, metrics_data.bat_data.batt_capacity, BMT_status.temperature,
		BMT_status.bat_vol, BMT_status.ICharging, event);
}
#endif

int bat_metrics_slp_current(u32 ma)
{
	struct fg_slp_current_data *slp = &metrics_data.slp_curr_data;
	struct timespec now_ts, gap_ts;
	long elapsed;
#if defined(CONFIG_AMAZON_MINERVA_METRICS_LOG)
	char dimensions_buf[128];
#endif

	get_monotonic_boottime(&now_ts);
	if (slp->slp_ma == 0)
		goto exit;

	gap_ts = timespec_sub(now_ts, slp->last_ts);
	elapsed = gap_ts.tv_sec;
#if defined(CONFIG_AMAZON_METRICS_LOG)
	bat_metrics_log("battery",
			"FGSleepCurrent:def:Slp_mA_%d=1;CT;1,elapsed=%ld;TI;1:NR",
			ma, elapsed);
#endif

#if defined(CONFIG_AMAZON_MINERVA_METRICS_LOG)
		snprintf(dimensions_buf, 128, "\"FGSleepCurrent\"#\"%d\"", ma);
		minerva_counter_to_vitals(ANDROID_LOG_INFO,
			VITALS_BATTERY_GROUP_ID, VITALS_BATTERY_DRAIN_SCHEMA_ID,
			"battery", "battery", "drain",
			"suspend_drain", elapsed, "ms",
			NULL, VITALS_NORMAL,
			dimensions_buf, NULL);
#endif

exit:
	slp->slp_ma = ma;
	slp->last_ts = now_ts;
	return 0;
}

int bat_metrics_aicl(bool is_detected, u32 aicl_result)
{
#if defined(CONFIG_AMAZON_METRICS_LOG)
	bat_metrics_log("AICL",
		"%s:aicl:detected=%d;CT;1,aicl_result=%d;CT;1:NR",
		__func__, is_detected, aicl_result);
#endif

#if defined(CONFIG_AMAZON_MINERVA_METRICS_LOG)
	minerva_metrics_log(g_metrics_buf, BATTERY_METRICS_BUFF_SIZE,
		"%s:%s:100:%s,%s,adapter_power_mw=%d;IN,aicl_ma=%d;IN:us-east-1",
		METRICS_BATTERY_GROUP_ID, METRICS_BATTERY_ADAPTER_SCHEMA_ID,
		PREDEFINED_ESSENTIAL_KEY, PREDEFINED_TZ_KEY,
		is_detected ? "9000" : "5000",
		(aicl_result < 0) ? 0 : aicl_result);
#endif

	return 0;
}

int bat_metrics_vbus(bool is_on)
{
	if (metrics_data.vbus_on_old == is_on)
		return 0;

	metrics_data.vbus_on_old = is_on;
#if defined(CONFIG_AMAZON_METRICS_LOG)
	bat_metrics_log("USBCableEvent",
		"bq24297:vbus_%s=1;CT;1:NR", is_on ? "on" : "off");
#endif

#if defined(CONFIG_AMAZON_MINERVA_METRICS_LOG)
	bat_minerva_log("vbus_%s", is_on ? "on" : "off");
#endif

	return 0;
}

int bat_metrics_chrdet(u32 chr_type)
{
	static const char * const charger_type_text[] = {
		"UNKNOWN", "STANDARD_HOST", "CHARGING_HOST",
		"NONSTANDARD_CHARGER", "STANDARD_CHARGER", "APPLE_2_1A_CHARGER",
		"APPLE_1_0A_CHARGER", "APPLE_0_5A_CHARGER", "WIRELESS_CHARGER"
	};

	if (metrics_data.chg_type_old == chr_type)
		return 0;

	metrics_data.chg_type_old = chr_type;
#if defined(CONFIG_AMAZON_METRICS_LOG)
	if (chr_type > CHARGER_UNKNOWN && chr_type <= WIRELESS_CHARGER) {
		bat_metrics_log("USBCableEvent",
			"%s:bq24297:chg_type_%s=1;CT;1:NR",
			__func__, charger_type_text[chr_type]);
	}
#endif

#if defined(CONFIG_AMAZON_MINERVA_METRICS_LOG)
	if (chr_type > CHARGER_UNKNOWN && chr_type <= WIRELESS_CHARGER)
		bat_minerva_log("chg_type_%s", charger_type_text[chr_type]);
#endif

	return 0;
}

int bat_metrics_chg_fault(u8 fault_type)
{
	if (metrics_data.fault_type_old == fault_type)
		return 0;

	metrics_data.fault_type_old = fault_type;
#if defined(CONFIG_AMAZON_METRICS_LOG)
	if (fault_type != 0)
		bat_metrics_log("battery",
			"bq24297:def:charger_fault_type=%u;CT;1:NA",
			fault_type);
#endif

#if defined(CONFIG_AMAZON_MINERVA_METRICS_LOG)
	if (fault_type != 0)
		bat_minerva_log("charger_fault_type=%u", fault_type);
#endif

	return 0;
}

int bat_metrics_chg_state(u32 chg_sts)
{
	int soc = BMT_status.UI_SOC;
	int vbat = BMT_status.bat_vol;
	int ibat = BMT_status.ICharging;
	char *chg_sts_str;

	if (metrics_data.chg_sts_old == chg_sts)
		return 0;

	metrics_data.chg_sts_old = chg_sts;
	chg_sts_str = (chg_sts == POWER_SUPPLY_STATUS_CHARGING) ? "CHARGING" : "DISCHARGING";

#if defined(CONFIG_AMAZON_METRICS_LOG)
	bat_metrics_log("battery",
		"bq24297:def:POWER_STATUS_%s=1;CT;1,cap=%u;CT;1,mv=%d;CT;1," \
		"current_avg=%d;CT;1:NR", chg_sts_str, soc, vbat, ibat);
#endif

#if defined(CONFIG_AMAZON_MINERVA_METRICS_LOG)
	bat_minerva_log("POWER_STATUS_%s", chg_sts_str);
#endif

	return 0;
}

int bat_metrics_critical_shutdown(void)
{
	static bool written;

	if (BMT_status.charger_exist)
		return 0;

	if (BMT_status.bat_exist != true)
		return 0;

	if (BMT_status.UI_SOC != 0)
		return 0;

	if (written == false &&
		BMT_status.bat_vol <= batt_cust_data.system_off_voltage) {
		written = true;
#if defined(CONFIG_AMAZON_METRICS_LOG)
		bat_metrics_log("battery",
			"bq24297:def:critical_shutdown=1;CT;1:HI");
#endif

#if defined(CONFIG_AMAZON_MINERVA_METRICS_LOG)
		bat_minerva_log("critical_shutdown");
#endif
#ifdef CONFIG_AMAZON_SIGN_OF_LIFE
		life_cycle_set_special_mode(LIFE_CYCLE_SMODE_LOW_BATTERY);
#endif
	}

	return 0;
}

extern unsigned long get_virtualsensor_temp(void);
int bat_metrics_top_off_mode(bool is_on, long total_time_plug_in)
{
	struct battery_meter_data *bat_data = &metrics_data.bat_data;
	unsigned long virtual_temp = get_virtualsensor_temp();

	if (metrics_data.is_top_off_mode == is_on)
		return 0;

	battery_meter_get_data(bat_data);
	metrics_data.is_top_off_mode = is_on;
#if defined(CONFIG_AMAZON_METRICS_LOG)
	if (is_on) {
		bat_metrics_log("battery",
		"bq24297:def:Charging_Over_14days=1;CT;1," \
		"Total_Plug_Time=%ld;CT;1,Bat_Vol=%d;CT;1,UI_SOC=%d;CT;1," \
		"SOC=%d;CT;1,Bat_Temp=%d;CT;1,Vir_Avg_Temp=%ld;CT;1," \
		"Bat_Cycle_count=%d;CT;1:NA",
		total_time_plug_in, BMT_status.bat_vol, BMT_status.UI_SOC,
		BMT_status.SOC, BMT_status.temperature, virtual_temp,
		bat_data->battery_cycle);
	}
#endif

#if defined(CONFIG_AMAZON_MINERVA_METRICS_LOG)
	if (is_on)
		bat_minerva_log("Charging_Over_7days");
#endif

	return 0;
}

int bat_metrics_demo_mode(bool is_on, long total_time_plug_in)
{
	unsigned long virtual_temp = get_virtualsensor_temp();
	struct battery_meter_data *bat_data = &metrics_data.bat_data;

	if (metrics_data.is_demo_mode == is_on)
		return 0;

	battery_meter_get_data(bat_data);
	metrics_data.is_demo_mode = is_on;
#if defined(CONFIG_AMAZON_METRICS_LOG)
	if (is_on) {
		bat_metrics_log("battery",
		"bq24297:def:Store_Demo_Mode=1;CT;1,Total_Plug_Time=%ld;CT;1," \
		"Bat_Vol=%d;CT;1,UI_SOC=%d;CT;1,SOC=%d;CT;1,Bat_Temp=%d;CT;1," \
		"Vir_Avg_Temp=%ld;CT;1,Bat_Cycle_Count=%d;CT;1:NA",
		total_time_plug_in, BMT_status.bat_vol, BMT_status.UI_SOC,
		BMT_status.SOC, BMT_status.temperature, virtual_temp,
		bat_data->battery_cycle);
	}
#endif

#if defined(CONFIG_AMAZON_MINERVA_METRICS_LOG)
	if (is_on)
		bat_minerva_log("Store_Demo_Mode=%ld", total_time_plug_in);
#endif

	return 0;
}

#define SUSPEND_RESUME_INTEVAL_MIN 1
int bat_metrics_suspend(void)
{
	struct pm_state *pm = &metrics_data.pm;

	get_monotonic_boottime(&pm->suspend_ts);
	pm->suspend_soc = BMT_status.UI_SOC;
	pm->suspend_bat_car = battery_meter_get_car();

	return 0;
}

int bat_metrics_resume(void)
{
	struct battery_meter_data *bat_data = &metrics_data.bat_data;
	struct pm_state *pm = &metrics_data.pm;
	struct timespec resume_ts;
	struct timespec sleep_ts;
	int soc, diff_soc, resume_bat_car, diff_bat_car;
	long elaps_msec;
#if defined(CONFIG_AMAZON_MINERVA_METRICS_LOG)
	char dimensions_buf[128];
#endif

	soc = BMT_status.UI_SOC;
	get_monotonic_boottime(&resume_ts);
	resume_bat_car = battery_meter_get_car();
	if (pm->resume_soc == -1 || pm->suspend_soc == -1)
		goto exit;

	diff_soc = pm->suspend_soc - soc;
	diff_bat_car = pm->suspend_bat_car - resume_bat_car;
	sleep_ts = timespec_sub(resume_ts, pm->suspend_ts);
	elaps_msec = sleep_ts.tv_sec * 1000 + sleep_ts.tv_nsec / NSEC_PER_MSEC;
	pr_debug("%s: diff_soc: %d sleep_time(s): %ld [%ld - %ld]\n",
			__func__, diff_soc, sleep_ts.tv_sec,
			resume_ts.tv_sec, pm->suspend_ts.tv_sec);

	if (sleep_ts.tv_sec > SUSPEND_RESUME_INTEVAL_MIN) {
		pr_debug("%s: sleep_ts: %ld diff_soc: %d, diff_bat_car: %d\n",
			__func__, sleep_ts.tv_sec, diff_soc, diff_bat_car);
#if defined(CONFIG_AMAZON_METRICS_LOG)
		bat_metrics_log("drain_metrics",
			"suspend_drain:def:value=%d;CT;1,elapsed=%ld;TI;1:NR",
			diff_soc, elaps_msec);
#endif

#if defined(CONFIG_AMAZON_MINERVA_METRICS_LOG)
		snprintf(dimensions_buf, 128, "\"diff_soc\"#\"%d\"", diff_soc);
		minerva_counter_to_vitals(ANDROID_LOG_INFO,
			VITALS_BATTERY_GROUP_ID, VITALS_BATTERY_DRAIN_SCHEMA_ID,
			"battery", "battery", "drain",
			"suspend_drain", elaps_msec, "ms",
			NULL, VITALS_NORMAL,
			dimensions_buf, NULL);
#endif
	}

	battery_meter_get_data(bat_data);
#if defined(CONFIG_AMAZON_METRICS_LOG)
	bat_metrics_log("bq24297",
		"batt:def:cap=%d;CT;1,mv=%d;CT;1,current_avg=%d;CT;1," \
		"temp_g=%d;CT;1,charge=%d;CT;1,charge_design=%d;CT;1," \
		"aging_factor=%d;CT;1,battery_cycle=%d;CT;1,"
		"columb_sum=%d;CT;1:NR",
		BMT_status.UI_SOC, BMT_status.bat_vol,
		BMT_status.ICharging, BMT_status.temperature,
		bat_data->batt_capacity_aging, /*battery_remaining_charge,?*/
		bat_data->batt_capacity, /*battery_remaining_charge_design*/
		bat_data->aging_factor, /* aging factor */
		bat_data->battery_cycle,
		bat_data->columb_sum);
#endif

#if defined(CONFIG_AMAZON_MINERVA_METRICS_LOG)
	bat_minerva_log("bat_metrics_resume");
	snprintf(dimensions_buf, 128,
		"\"aging_factor\"#\"%d\"$\"bat_cycle\"#\"%d\"", bat_data->aging_factor, bat_data->battery_cycle);
	minerva_counter_to_vitals(ANDROID_LOG_INFO,
		VITALS_BATTERY_GROUP_ID, VITALS_BATTERY_AGING_SCHEMA_ID,
		"battery", "battery", "aging",
		NULL, bat_data->columb_sum, "rate",
		NULL, VITALS_NORMAL, dimensions_buf, NULL);
#endif

exit:
	pm->resume_bat_car = resume_bat_car;
	pm->resume_ts = resume_ts;
	pm->resume_soc = soc;
	return 0;
}

#if defined(CONFIG_FB)
static int bat_metrics_screen_on(void)
{
	struct timespec screen_on_time;
	struct timespec diff;
	struct screen_state *screen = &metrics_data.screen;
	long elaps_msec;
	int soc = BMT_status.UI_SOC, diff_soc;
#if defined(CONFIG_AMAZON_MINERVA_METRICS_LOG)
	char dimensions_buf[128];
#endif
	get_monotonic_boottime(&screen_on_time);
	if (screen->screen_on_soc == -1 || screen->screen_off_soc == -1)
		goto exit;

	diff_soc = screen->screen_off_soc - soc;
	diff = timespec_sub(screen_on_time, screen->screen_off_time);
	elaps_msec = diff.tv_sec * 1000 + diff.tv_nsec / NSEC_PER_MSEC;
#if defined(CONFIG_AMAZON_METRICS_LOG)
	bat_metrics_log("drain_metrics",
		"screen_off_drain:def:value=%d;CT;1,elapsed=%ld;TI;1:NR",
		diff_soc, elaps_msec);
#endif

#if defined(CONFIG_AMAZON_MINERVA_METRICS_LOG)
	snprintf(dimensions_buf, 128, "\"diff_soc\"#\"%d\"", diff_soc);
	minerva_counter_to_vitals(ANDROID_LOG_INFO,
		VITALS_BATTERY_GROUP_ID, VITALS_BATTERY_DRAIN_SCHEMA_ID,
		"battery", "battery", "drain",
		"screen_off_drain", elaps_msec, "ms",
		NULL, VITALS_NORMAL,
		dimensions_buf, NULL);
#endif

exit:
	screen->screen_on_time = screen_on_time;
	screen->screen_on_soc = soc;
	return 0;
}

static int bat_metrics_screen_off(void)
{
	struct timespec screen_off_time;
	struct timespec diff;
	struct screen_state *screen = &metrics_data.screen;
	long elaps_msec;
	int soc = BMT_status.UI_SOC, diff_soc;
#if defined(CONFIG_AMAZON_MINERVA_METRICS_LOG)
	char dimensions_buf[128];
#endif

	get_monotonic_boottime(&screen_off_time);
	if (screen->screen_on_soc == -1 || screen->screen_off_soc == -1)
		goto exit;

	diff_soc = screen->screen_on_soc - soc;
	diff = timespec_sub(screen_off_time, screen->screen_on_time);
	elaps_msec = diff.tv_sec * 1000 + diff.tv_nsec / NSEC_PER_MSEC;
#if defined(CONFIG_AMAZON_METRICS_LOG)
	bat_metrics_log("drain_metrics",
		"screen_on_drain:def:value=%d;CT;1,elapsed=%ld;TI;1:NR",
		diff_soc, elaps_msec);
#endif

#if defined(CONFIG_AMAZON_MINERVA_METRICS_LOG)
	snprintf(dimensions_buf, 128, "\"diff_soc\"#\"%d\"", diff_soc);
	minerva_counter_to_vitals(ANDROID_LOG_INFO,
		VITALS_BATTERY_GROUP_ID, VITALS_BATTERY_DRAIN_SCHEMA_ID,
		"battery", "battery", "drain",
		"screen_on_drain", elaps_msec, "ms",
		NULL, VITALS_NORMAL,
		dimensions_buf, NULL);
#endif

exit:
	screen->screen_off_time = screen_off_time;
	screen->screen_off_soc = soc;
	return 0;
}

static int pm_notifier_callback(struct notifier_block *notify,
			unsigned long event, void *data)
{
	struct fb_event *ev_data = data;
	int *blank;

	if (ev_data && ev_data->data && event == FB_EVENT_BLANK) {
		blank = ev_data->data;
		if (*blank == FB_BLANK_UNBLANK)
			bat_metrics_screen_on();

		else if (*blank == FB_BLANK_POWERDOWN)
			bat_metrics_screen_off();
	}

	return 0;
}
#endif

int bat_metrics_init(void)
{
	int ret = 0;

	metrics_data.screen.screen_off_soc = -1;
	metrics_data.screen.screen_off_soc = -1;
	metrics_data.pm.suspend_soc = -1;
	metrics_data.pm.resume_soc = -1;
#if defined(CONFIG_FB)
	metrics_data.pm_notifier.notifier_call = pm_notifier_callback;
	ret= fb_register_client(&metrics_data.pm_notifier);
	if (ret)
		pr_err("%s: fail to register pm notifier\n", __func__);
#endif

	return 0;
}

void bat_metrics_uninit(void)
{
#if defined(CONFIG_FB)
	fb_unregister_client(&metrics_data.pm_notifier);
#endif
}
