#ifndef _INTEGRITY_MODULE_H
#define _INTEGRITY_MODULE_H

#define INTEGRITY_PLATFORM_LINUX 1

#define MAX_REPORTING_PERIOD                  (3600*24)
#define THREAD_SLEEP_TIME_MSEC                    10000
#define INTEGRITY_MODULE_VERSION                     10
#define BATTERY_METRICS_BUFF_SIZE                   512
#define ANDROID_LOG_INFO_LOCAL                        4
#define STRESS_PERIOD                               450
#define STRESS_REPORT_PERIOD                      28800
#define VUSB_CEILING                              20000
#define STRESS_STRING_IDENTIFIER                     '_'
#define STRESS_BATT_VOLT_LOWER_LIMIT               2700
#define STRESS_BATT_VOLT_UPPER_LIMIT               4550
#define STRESS_BATT_TEMP_LOWER_LIMIT            (-5000)
#define STRESS_BATT_TEMP_UPPER_LIMIT              88000
#define STRESS_INT_TO_CHAR_LOWER_LIMIT             (-1)
#define STRESS_INT_TO_CHAR_UPPER_LIMIT               63
#define STRESS_STRING_BUFF_SIZE    ((2*STRESS_REPORT_PERIOD/STRESS_PERIOD) + 1)

#define INTEGRITY_BATTERY_MODULE   "integrity_batt"
#define INTEGRITY_METRICS_REGION    "us-east-1"
#define INTEGRITY_METRICS_SAMPLING  "100"
#define INTEGRITY_METRICS_PREDEFINED_FIELDS "_deviceId=;SY,_softwareVersion=;SY,_platform=;SY,_model=;SY,_deviceType=;SY,_countryOfResidence=;SY,"

#define INTEGRITY_BATTERY_GROUP    "gr263bbh"
#define STRESS_PULSE_DEBUG_SCHEMA  "w8t9/2/07330400"
#define STRESS_REPORT_SCHEMA       "r71b/2/0c330401"
#define CHARGE_STATE_REPORT_SCHEMA "2iop/2/0c330401"
#define CHARGE_STATE_DEBUG_SCHEMA  "uolx/2/07330400"
#define SOC_CORNER_95_SCHEMA       "ghc6/2/05330401"
#define SOC_CORNER_15_1_SCHEMA     "3ujh/2/05330401"
#define SOC_CORNER_15_2_SCHEMA     "k28i/2/05330401"

#define STRESS_PULSE_DEBUG_STRING  "PULSEDEBUG=1;IN,stressFrame=%d;IN,stressPeriod=%d;IN,Vol=%d;IN,temp=%d;IN,stress=%s;SY,iVer=%d;IN"
#define STRESS_REPORT_STRING       "BattStress=1;IN,stress_period=%d;IN,stress=%s;SY,iVer=%d;IN"
#define CHARGE_STATE_REPORT_STRING "UNLOAD=1;IN,Charger_status=%d;IN,Elaps_Sec=%ld;IN,iVol=%d;IN,fVol=%d;IN,lVol=%d;IN,iSOC=%d;IN,Bat_aTemp=%d;IN,Vir_aTemp=%d;IN,Bat_pTemp=%d;IN,Vir_pTemp=%d;IN,bTemp=%d;IN,Cycles=%d;IN,pVUsb=%d;IN,fVUsb=%d;IN,mVUsb=%d;IN,aVUsb=%d;IN,aVol=%d;IN,fSOC=%d;IN,ct=%lu;IN,iVer=%d;IN"
#define CHARGE_STATE_DEBUG_STRING  "ThreadLocal=1;IN,Charger_status=%d;IN,AvgVbatt=%d;IN,AvgVUSB=%d;IN,mVUsb=%d;IN,n_count=%d;IN,Elaps_Sec=%ld;IN,Bat_vTemp=%d;IN,elaps_sec=%ld;IN,elaps_sec_start=%ld;IN,elaps_sec_prev=%ld;IN,delta_elaps_sec=%ld;IN,calc_elaps_sec=%ld;IN"
#define SOC_CORNER_95_STRING       "time_soc95=1;IN,Elaps_Sec=%ld;IN"
#define SOC_CORNER_15_1_STRING     "time_soc15_soc20=1;IN,Init_Vol=%d;IN,Init_SOC=%d;IN,Elaps_Sec=%ld;IN"
#define SOC_CORNER_15_2_STRING     "time_soc15_soc0=1;IN,Init_Vol=%d;IN,Init_SOC=%d;IN,Elaps_Sec=%ld;IN"


enum battery_metrics_info {
	TYPE_VBAT = 0,
	TYPE_VBUS,
	TYPE_TBAT,
	TYPE_SOC,
	TYPE_BAT_CYCLE,
	TYPE_CHG_STATE,
	TYPE_MAX,
};


struct integrity_metrics_data {

	unsigned int chg_sts;

	unsigned int batt_volt_init;
	unsigned int batt_volt_final;
	unsigned int batt_volt_avg;
	unsigned int usb_volt;
	unsigned int usb_volt_avg;
	unsigned int usb_volt_peak;
	unsigned int usb_volt_min;

	int soc;
	int fsoc;

	int batt_temp_peak;
	int batt_temp_virtual_peak;
	int batt_temp_avg;
	int batt_temp_avg_virtual;

	unsigned long n_count;
	unsigned long elaps_sec;
	unsigned long above_95_time;
	unsigned long below_15_time;
	bool batt_95_flag;
	bool batt_15_flag;
	bool battery_below_15_fired;
	unsigned int low_batt_init_volt;
	int low_batt_init_soc;

	unsigned int stress_frame;
	unsigned int stress_period;
	unsigned int stress_frame_count;
	char stress_string[STRESS_STRING_BUFF_SIZE];
	int batt_stress_temp_mili_c;
	unsigned int batt_stress_volts_mv;
	bool stress_below_5_fired;
};

struct integrity_driver_data {
	struct device *dev;
	struct platform_device *pdev;
	struct power_supply *batt_psy;
	struct timespec init_time;
	struct notifier_block notifier;
	struct delayed_work dwork;
	struct integrity_metrics_data metrics;
	bool is_suspend;
};

extern unsigned long get_virtualsensor_temp(void);
extern signed int gFG_battery_cycle;
#endif
