#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/delay.h>
#include <linux/jiffies.h>

#include <nvcommon.h>
#include <nvos.h>
#include <nvodm_services.h>

#include "odm_i2c.h"
#include "odm_sbi.h"
#include "logd.h"

#define BQ20Z70_SLAVE_ADDRESS			(0x16)
#define BATTERY_POLL_TIMER_INTERVAL		(10 * 1000) 

#define ACIN_PORT						('h'-'a')
#define ACIN_PIN						2


#if (defined(CONFIG_SMBAA1011_BATTERY) && defined(CONFIG_SMBAA1002_BATTERY))
#error "Both CONFIG_A1011_BATTERY and CONFIG_A1002_BATTERY is defined, is this right?"
#endif

#ifdef CONFIG_SMBA1011_BATTERY
#define CHEN_PORT						('b' - 'a')
#define CHEN_PIN						0
#endif

#ifdef CONFIG_SMBA1002_BATTERY
#define CHEN_PORT						('k'-'a')
#define CHEN_PIN						7
#endif

#define CHARGE_DISABLE_TEMPERATURE		60
#define CHARGE_ENABLE_TEMPERATURE		40

static int ac_power_get_property(struct power_supply *psy,
		enum power_supply_property psp, union power_supply_propval *val);

static int battery_power_get_property(struct power_supply *psy,
		enum power_supply_property psp, union power_supply_propval *val);

static struct timer_list battery_poll_timer;

static struct work_struct battery_work;

struct smart_battery_dev {
	struct odm_i2c_dev dev;

	NvOdmServicesGpioHandle 		gpio_handle;
	NvOdmGpioPinHandle	  			acin_pin_handle;
	NvOdmServicesGpioIntrHandle 	acin_pin_intr_handle;
	NvOdmGpioPinHandle				chen_pin_handle;

	int 	ac_online;
	int		chen;
	int 	battery_alarm;
	int 	status;
	int 	present;
	int 	temperature;
	int 	rsoc;
	int 	voltage_now;
	int 	voltage_avg;
	int 	current_now;
	int 	current_avg;
	int 	time_to_empty_now;
	int 	time_to_empty_avg;
	int 	time_to_full_avg;
	int 	chemistry;
	int 	health;
	/* no run_time_to_full */
} battery_dev;

static const enum power_supply_property ac_power_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static const enum power_supply_property battery_power_properties[] = {
	POWER_SUPPLY_PROP_PRESENT,
//	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
//	POWER_SUPPLY_PROP_VOLTAGE_AVG,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_CURRENT_AVG,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_CAPACITY,	// RSOC
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG,
	POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW,
	POWER_SUPPLY_PROP_TIME_TO_FULL_AVG, 
	/* No POWER_SUPPLY_PROP_TIME_TO_FULL_NOW, */
	POWER_SUPPLY_PROP_TECHNOLOGY,
};

static char *ac_supply_list[] = {
	"battery",
};

static struct power_supply all_supplies[] = {
	{
		.name = "battery",
		.type = POWER_SUPPLY_TYPE_BATTERY,
		.properties = battery_power_properties,
		.num_properties = ARRAY_SIZE(battery_power_properties),
		.get_property = battery_power_get_property,
	},

	{
		.name = "ac",
		.type = POWER_SUPPLY_TYPE_MAINS,
		.supplied_to = ac_supply_list,
		.num_supplicants = ARRAY_SIZE(ac_supply_list),
		.properties = ac_power_properties,
		.num_properties = ARRAY_SIZE(ac_power_properties),
		.get_property = ac_power_get_property,
	}
};

/**
 * TODO: How to implement this logd_t() function ?
 */
#if 0
static void logd_t(unsigned const char *tag, unsigned const char *msg, ....)
{
}
#endif

static int ac_power_get_property(struct power_supply *psy,
		enum power_supply_property psp, union power_supply_propval *val)
{
	int retval = 0;

	logd("ac_power_get_property\r\n");

	if (psp == POWER_SUPPLY_PROP_ONLINE) {
		val->intval = battery_dev.ac_online;
	} else {
		retval = -EINVAL;
	}
	return retval;
}

static int battery_power_get_property(struct power_supply *psy, 
		enum power_supply_property psp, union power_supply_propval *val)
{
	int retval = 0;

	logd("battery_power_get_property\r\n");

	switch (psp) {
		case POWER_SUPPLY_PROP_STATUS:
			logd ("POWER_SUPPLY_PROP_STATUS\r\n");
			val->intval = battery_dev.status;
			break;
		case POWER_SUPPLY_PROP_PRESENT:
			logd ("POWER_SUPPLY_PROP_PRESENT\r\n");
			val->intval = battery_dev.present;
			break;
		case POWER_SUPPLY_PROP_TECHNOLOGY:
			logd ("POWER_SUPPLY_PROP_TECHNOLOGY\r\n");
			val->intval = battery_dev.chemistry;
			break;
		case POWER_SUPPLY_PROP_VOLTAGE_NOW:
			logd ("POWER_SUPPLY_PROP_VOLTAGE_NOW\r\n");
			val->intval = battery_dev.voltage_now;
			break;
		case POWER_SUPPLY_PROP_VOLTAGE_AVG:
			logd ("POWER_SUPPLY_PROP_VOLTAGE_AVG");
			val->intval = battery_dev.voltage_avg;
			break;
		case POWER_SUPPLY_PROP_CURRENT_NOW:
			logd ("POWER_SUPPLY_PROP_CURRENT_NOW\r\n");
			val->intval = battery_dev.current_now;
			break;
		case POWER_SUPPLY_PROP_CURRENT_AVG:
			logd ("POWER_SUPPLY_PROP_CURENT_AVG\r\n");
			val->intval = battery_dev.current_avg;
			break;
		case POWER_SUPPLY_PROP_TEMP:
			logd ("POWER_SUPPLY_PROP_TEMP\r\n");
			val->intval = battery_dev.temperature;
			break;
		case POWER_SUPPLY_PROP_CAPACITY:
			logd ("POWER_SUPPLY_PROP_CAPACITY\r\n");
			#ifdef CONFIG_7379Y_V11
			val->intval = (battery_dev.rsoc<10) ? 0 : (battery_dev.rsoc-3)*100/97;
			#elif defined(CONFIG_CLIENT_FLEX)
			val->intval = (battery_dev.rsoc<10) ? 0 : (battery_dev.rsoc-10)*10/9;
			#else
			val->intval = battery_dev.rsoc;
			#endif
			break;
		case POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW:
			logd ("POWER_SUPPLY_PROP_TIME_TO_EMPTY_NOW\r\n");
			val->intval = battery_dev.time_to_empty_now;
			break;
		case POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG:
			logd ("POWER_SUPPLY_PROP_TIME_TO_EMPTY_AVG\r\n");
			val->intval = battery_dev.time_to_empty_avg;
			break;
		case POWER_SUPPLY_PROP_TIME_TO_FULL_AVG:
			logd ("POWER_SUPPLY_PROP_TIME_TO_FULL_AVG\r\n");
			val->intval = battery_dev.time_to_full_avg;
			break;
		case POWER_SUPPLY_PROP_HEALTH:
			logd ("POWER_SUPPLY_PROP_HEALTH \r\n");
			val->intval = battery_dev.health;
			break;
		default:
			retval = -EINVAL;
	}
	return retval;
}

int charge_enable(bool enable)
{
	if (battery_dev.chen != enable) {
		battery_dev.chen = enable;
		NvOdmGpioSetState(battery_dev.gpio_handle, battery_dev.chen_pin_handle, enable);
	}
}

/*
 * return 0 to indicate changed.
 */
int update_ac_state() 
{
	/*TODO: how to detect ac attach? */
	int acin;
	
	NvOdmGpioGetState(battery_dev.gpio_handle, battery_dev.acin_pin_handle, &acin);
	acin = !acin;

	logd((acin?"ac attach" : "ac detach\r\n"));

	if (acin != battery_dev.ac_online) {
		battery_dev.ac_online = acin;
		return 0;
	}
	return -1;
}

/*
 * return 0 to indicate changed.
 */
int update_battery_state()
{
	short current_now, current_avg;
	unsigned short status, rsoc, temperature, serial_number, health;
	unsigned short voltage_now, voltage_avg;
	unsigned short int time_to_empty_now, time_to_empty_avg, time_to_full_avg;

	if (BatteryStatus(&battery_dev.dev, &status)
		|| RelativeStateOfCharge(&battery_dev.dev, &rsoc)
		|| Temperature(&battery_dev.dev, &temperature)
		|| Voltage(&battery_dev.dev, &voltage_now)
//		|| AverageVoltage(&battery_dev.dev, &voltage_avg)
		|| Current(&battery_dev.dev, &current_now)
		|| AverageCurrent(&battery_dev.dev, &current_avg)
		|| RunTimeToEmpty(&battery_dev.dev, &time_to_empty_now)
		|| AverageTimeToEmpty(&battery_dev.dev, &time_to_empty_avg)
		|| AverageTimeToEmpty(&battery_dev.dev, &time_to_full_avg))
//		|| StateOfHealth(&battery_dev.dev, &health))
	{
		battery_dev.present = 0;
		battery_dev.status = POWER_SUPPLY_STATUS_UNKNOWN;
		logd("Present=%d\r\n", battery_dev.present);
		return 0;
	}

	battery_dev.battery_alarm = status & SBI_BS_ALL_ALARM;
	
	if (battery_dev.battery_alarm) {
	
		logd("BatteryStatus=%d\r\n", status);

		if (status & SBI_BS_OVER_CHARGED_ALARM) {
			logd("OVER_CHARGED_ALARM\r\n");
		}
		if (status & SBI_BS_OVER_TEMP_ALARM) {
			logd("OVER_TEMP_ALARM\r\n");
		}
		if (status & SBI_BS_TERMINATE_CHARGE_ALARM) {
			logd("TERMINATE_CHARGE_ALARM\r\n");
		}
		if (status & SBI_BS_TERMINATE_DISCHARGE_ALARM) {
			logd("TERMIANTE_DISCHARGE_ALARM\r\n");
		} 
		if (status & SBI_BS_REMAINING_CAPACITY_ALARM) {
			logd("REMAINING_CAPACITY_ALARM\r\n");
		} 
		if (status & SBI_BS_REMAINING_TIME_ALARM) {
			logd("REMAINING_TIME_ALARM\r\n");
		}
	}

	if (status & SBI_BS_FULLY_CHARGED) {
		battery_dev.status = POWER_SUPPLY_STATUS_FULL;
		logd("Status=%s", "FULL");
	} else {
		if (!battery_dev.ac_online) {
			battery_dev.status = POWER_SUPPLY_STATUS_DISCHARGING;
			logd("Status=%s", "DISCHARGING");
		} else {
			battery_dev.status = (status & SBI_BS_DISCHARGING) ? POWER_SUPPLY_STATUS_DISCHARGING : POWER_SUPPLY_STATUS_CHARGING;
			logd("Status=%s", ((status & SBI_BS_DISCHARGING) ? "DISCHARGING" : "CHARGING"));
		}
	}

	battery_dev.present = 1;
	battery_dev.temperature = temperature / 10;		// 0.1K to 1K
	battery_dev.rsoc = rsoc;
	battery_dev.voltage_now = voltage_now * 1000;	// mV to uV
//	battery_dev.voltage_avg = voltage_avg;
	battery_dev.current_now = current_now * 1000;	// mA to uA
	battery_dev.current_avg = current_avg * 1000;	// mA to uA
	battery_dev.time_to_empty_now = time_to_empty_now * 60;	// min to sec
	battery_dev.time_to_empty_avg = time_to_empty_avg * 60;	// min to sec
	battery_dev.time_to_full_avg = time_to_full_avg * 60;	// min to sec
	battery_dev.chemistry = POWER_SUPPLY_TECHNOLOGY_LION;
//	battery_dev.health = health;
//	battery_dev.ac_online = (battery_dev.status != POWER_SUPPLY_STATUS_DISCHARGING);

	logd("Present=%d\r\n", battery_dev.present);
	logd("Temperature=%d\r\n", battery_dev.temperature);
	logd("RelativeStateOfChage=%d\r\n", battery_dev.rsoc);
	logd("Voltage=%d\r\n", battery_dev.voltage_now);
//	logd("AverageVoltage=%d\r\n", battery_dev.voltage_avg);
	logd("Current=%d\r\n", battery_dev.current_now);
	logd("AverageCurrent=%d\r\n", battery_dev.current_avg);
	logd("RunTimeToEmpty=%d\r\n", battery_dev.time_to_empty_now);
	logd("AvgTimeToEmpty=%d\r\n", battery_dev.time_to_empty_avg);
//	logd("Health=%d\r\n", battery_dev.health);

	return 0;
}

static void battery_work_func(struct work_struct *work)
{
	bool ac_changed, battery_changed;
	logd("battery_work working...\r\n");

	ac_changed = !update_ac_state();
	battery_changed = !update_battery_state();

	/* Safe battery temprature check */
	if (battery_dev.chen) {
		if (battery_dev.temperature > CHARGE_DISABLE_TEMPERATURE) {
			charge_enable(false);
		}
	} else {
		if (battery_dev.temperature < CHARGE_ENABLE_TEMPERATURE) {
			charge_enable(true);
		}
	}

	if (ac_changed) {
		power_supply_changed(&all_supplies[1]);
	}
	if (battery_changed) {
		power_supply_changed(&all_supplies[0]);
	}
}

static void battery_poll_timer_func(unsigned long unused)
{
	schedule_work(&battery_work);
	mod_timer(&battery_poll_timer, jiffies + msecs_to_jiffies(BATTERY_POLL_TIMER_INTERVAL));
}

void acin_isr(void *arg)
{
	logd("acin_isr...\r\n");
	schedule_work(&battery_work);
	NvOdmGpioInterruptDone(battery_dev.acin_pin_intr_handle);
}

static int smart_battery_probe(struct platform_device *pdev)
{
	int i;

	logd("battery: probe() IN\r\n");

	memset(&battery_dev, 0, sizeof(struct smart_battery_dev));

	odm_smbus_init(&battery_dev.dev, 0, BQ20Z70_SLAVE_ADDRESS, 1000);
	if (odm_i2c_open(&battery_dev.dev)) {
		logd("battery: failed_open_i2c\r\n");
		goto failed_open_i2c;
	}

	battery_dev.gpio_handle = NvOdmGpioOpen();
	if (!battery_dev.gpio_handle) {
		logd("battery: failed_open_gpio\r\n");
		goto failed_open_gpio;
	}
	battery_dev.acin_pin_handle = NvOdmGpioAcquirePinHandle(battery_dev.gpio_handle, 
			ACIN_PORT, ACIN_PIN);
	if (!battery_dev.acin_pin_handle) {
		logd("battery: failed_aquire_pin\r\n");
		goto failed_aquire_pin;
	}	
	NvOdmGpioConfig(battery_dev.gpio_handle, battery_dev.acin_pin_handle, NvOdmGpioPinMode_InputData);
	if (!NvOdmGpioInterruptRegister(battery_dev.gpio_handle, &battery_dev.acin_pin_intr_handle, 
			battery_dev.acin_pin_handle, NvOdmGpioPinMode_InputInterruptAny, acin_isr, NULL, 0)) {
		logd("battery: failed_register_inrt\r\n");
		goto failed_register_intr;
	}
	battery_dev.chen_pin_handle = NvOdmGpioAcquirePinHandle(battery_dev.gpio_handle, 
			CHEN_PORT, CHEN_PIN);
	if (!battery_dev.chen_pin_handle) {
		goto failed_acquire_chen;
	}
	NvOdmGpioConfig(battery_dev.gpio_handle, battery_dev.chen_pin_handle, NvOdmGpioPinMode_Output);
	NvOdmGpioSetState(battery_dev.gpio_handle, battery_dev.chen_pin_handle, 0);

	update_ac_state();
	update_battery_state();

	INIT_WORK(&battery_work, battery_work_func);

	for (i = 0; i < ARRAY_SIZE(all_supplies); i++) {
		power_supply_register(&pdev->dev, &all_supplies[i]);
	}

	setup_timer(&battery_poll_timer, battery_poll_timer_func, 0);
	mod_timer(&battery_poll_timer, jiffies + msecs_to_jiffies(BATTERY_POLL_TIMER_INTERVAL));

	logd("battery: probe() OUT");
	
	return 0;

	NvOdmGpioReleasePinHandle(battery_dev.gpio_handle, battery_dev.chen_pin_handle);
failed_acquire_chen:
	NvOdmGpioInterruptUnregister(battery_dev.gpio_handle, battery_dev.acin_pin_handle, battery_dev.acin_pin_intr_handle);
failed_register_intr:
	NvOdmGpioReleasePinHandle(battery_dev.gpio_handle, battery_dev.acin_pin_handle);
failed_aquire_pin:
	NvOdmGpioClose(battery_dev.gpio_handle);
failed_open_gpio:
	odm_i2c_close(&battery_dev.dev);
failed_open_i2c:

	logd("battery_probe failed... ");
	return -1;
}

static void smart_battery_remove (struct platform_device *pdev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(all_supplies); i++) {
		power_supply_unregister(&all_supplies[i]);
	}
	NvOdmGpioReleasePinHandle(battery_dev.gpio_handle, battery_dev.chen_pin_handle);
	NvOdmGpioInterruptUnregister(battery_dev.gpio_handle, battery_dev.acin_pin_handle, battery_dev.acin_pin_intr_handle);
	NvOdmGpioReleasePinHandle(battery_dev.gpio_handle, battery_dev.acin_pin_handle);
	NvOdmGpioClose(battery_dev.gpio_handle);
	odm_i2c_close(&battery_dev.dev);
	return;
}

static int smart_battery_suspend (struct platform_device *pdev, 
	pm_message_t state) 
{
	logd("battery: suspend\r\n");
	NvOdmGpioInterruptMask(battery_dev.acin_pin_intr_handle, true);
	/* Kill the battery timer sync */
	del_timer_sync(&battery_poll_timer);
	flush_scheduled_work();
	return 0;
}

static int smart_battery_resume (struct platform_device *pdev)
{
	logd("battery: resume\r\n");
	NvOdmGpioInterruptMask(battery_dev.acin_pin_intr_handle, false);
	setup_timer(&battery_poll_timer, battery_poll_timer_func, 0);
	mod_timer(&battery_poll_timer, jiffies + msecs_to_jiffies(BATTERY_POLL_TIMER_INTERVAL));	
	return 0;
}

static struct platform_driver smart_battery_driver = {
	.probe = smart_battery_probe,
	.remove = smart_battery_remove,
	.suspend = smart_battery_suspend,
	.resume = smart_battery_resume,
	//.driver.name = "yoku_0563113_battery",
	.driver.name = "smba10xx_battery",
	.driver.owner = THIS_MODULE,
};

static int smart_battery_init(void)
{
	logd("battery: init() IN ");
	platform_driver_register(&smart_battery_driver);
	logd("battery: init() OUT ");
	return 0;
}

static void smart_battery_exit(void)
{
	platform_driver_unregister(&smart_battery_driver);
}

module_init(smart_battery_init);
module_exit(smart_battery_exit);

MODULE_AUTHOR("LiuZheng <xmlz@malata.com>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Driver for yoku_0563313 smart battery");
