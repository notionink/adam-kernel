/*
 * Copyright (C) 2010 Malata, Corp. All Right Reseved.
 *
 * Athor: LiuZheng 
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>

#include <linux/leds_smb.h>

#define __BATTERY_LED_GENERIC_DEBUG__		0

#define TAG			"battery-led:  "

#if __BATTERY_LED_GENERIC_DEBUG__
#define	logd(x...)			do { printk(TAG x); } while(0)
#else
#define	logd(x...)			do {} while(0)
#endif

#define loge(x...)			do { printk(TAG x); } while(0)

#define BATTERY_LED_DEFAULT_ON			200
#define BATTERY_LED_DEFAULT_OFF			200


// TEGRA_GPIO_PH3
#define BATTERY_LED_TAKE_CHARGE			59
// TEGRA_GPIO_PC3 19
#define BATTERY_LED_RED					19
// TEGRA_GPIO_PU2 162
#define BATTERY_LED_GREEN				162
// TEGRA_GPIO_PV1 169
#define BATTERY_LED_BLUE				169

struct battery_led_device {
	unsigned int color;
	unsigned int mode, on, off, phase;
	struct delayed_work work;
	struct smb_led_device led;
};

static int battery_led_get_color(struct smb_led_device *led, unsigned int *color)
{
	struct battery_led_device *battery_led;
	logd("battery_led_get_color() IN\r\n");
	battery_led = container_of(led, struct battery_led_device, led);
	*color = battery_led->color;
	logd("battery_led_get_color() OUT\r\n");
	return 0;
}

static int battery_led_set_color(struct smb_led_device *led, unsigned int color)
{
	struct battery_led_device *battery_led;
	logd("battery_led_set_color() IN\r\n");
	battery_led = container_of(led, struct battery_led_device, led);
	if (battery_led->color != color) {
		cancel_delayed_work_sync(&battery_led->work);
		battery_led->color = color;
		schedule_delayed_work(&battery_led->work, 0);
	}
	return 0;
}

static int battery_led_get_flash_mode(struct smb_led_device *led, unsigned int *mode)
{
	struct battery_led_device *battery_led;
	
	logd("battery_led_get_flash_mode() IN\r\n");

	battery_led = container_of(led, struct battery_led_device, led);
	*mode = battery_led->mode;

	logd("battery_led_get_flash_mode() OUT\r\n");

	return 0;
}

static int battery_led_set_flash_mode(struct smb_led_device *led, unsigned int mode)
{
	struct battery_led_device *battery_led;
	logd("battery_led_set_flash_mode() IN\r\n");
	battery_led = container_of(led, struct battery_led_device, led);
	logd(TAG "mode=%d\n", mode);
	switch (mode) {
		case LED_FLASH_TIMED:
		case LED_FLASH_HARDWARE:
		case LED_FLASH_NONE:
			battery_led->mode = mode;
			schedule_delayed_work(&battery_led->work, 0);
				break;
		default:
			/* Do nothing */
			logd("unknown flash mode \n");
			break;
	}
	logd("battery_led_set_flash_mode() OUT\r\n");
	return 0;
}

static int battery_led_get_flash_on(struct smb_led_device *led, unsigned int *on)
{
	struct battery_led_device *battery_led;
	logd("battery_led_get_flash_on() IN\r\n");
	battery_led = container_of(led, struct battery_led_device, led);
	*on = battery_led->on;
	logd("battery_led_get_flash_on() OUT\r\n");
	return 0;
}

static int battery_led_set_flash_on(struct smb_led_device *led, unsigned int on)
{
	struct battery_led_device *battery_led;
	logd("battery_led_set_flash_on() IN\r\n");
	battery_led = container_of(led, struct battery_led_device, led);
	if (battery_led->on != on) {
		//cancel_delayed_work(&battery_led->work);
		battery_led->on = on;
		//schedule_delayed_work(&battery_led->work, 0);
	}
	logd("battery_led_set_flash_on() OUT\r\n");
	return 0;
}

static int battery_led_get_flash_off(struct smb_led_device *led, unsigned int *off)
{
	struct battery_led_device *battery_led;
	logd("battery_led_get_flash_off() IN\r\n");
	battery_led = container_of(led, struct battery_led_device, led);
	*off = battery_led->off;
	logd("battery_led_get_flash_off() OUT\r\n");
	return 0;
}

static int battery_led_set_flash_off(struct smb_led_device *led, unsigned int off)
{
	struct battery_led_device *battery_led;

	logd("battery_led_set_flash_off() IN\r\n");

	battery_led = container_of(led, struct battery_led_device, led);
	if (battery_led->off != off) {
		//cancel_delayed_work(&battery_led->work);
		battery_led->off = off;
		//schedule_delayed_work(&battery_led->work, 0);
	}
	logd("battery_led_set_flash_off() OUT\r\n");

	return 0;
}

static void battery_led_work_func(struct work_struct *work)
{
	u64 delay;
	unsigned int major;
	struct battery_led_device *battery_led;
	
	logd("battery_led_work_func() IN\r\n");

	battery_led = container_of(work, struct battery_led_device, work.work);
	major = major_color(battery_led->color);
	if (major == COLOR_BLACK) {
		gpio_set_value(BATTERY_LED_RED, 0);
		gpio_set_value(BATTERY_LED_GREEN, 0);
		gpio_set_value(BATTERY_LED_BLUE, 0);
		return;
	}

	logd(TAG "color = %x\n", battery_led->color);
	logd(TAG "major = %x\n", major);
	logd(TAG "flash_mode=%d\n", battery_led->mode);

	if (battery_led->mode == LED_FLASH_HARDWARE 
			|| battery_led->mode == LED_FLASH_TIMED) {
		switch (major) {
			case COLOR_RED:
				gpio_set_value(BATTERY_LED_RED, battery_led->phase);
				gpio_set_value(BATTERY_LED_GREEN, 0);
				gpio_set_value(BATTERY_LED_BLUE, 0);
				break;
			case COLOR_GREEN:
				gpio_set_value(BATTERY_LED_RED, 0);
				gpio_set_value(BATTERY_LED_GREEN, battery_led->phase);
				gpio_set_value(BATTERY_LED_BLUE, 0);
				break;
			case COLOR_BLUE:
				gpio_set_value(BATTERY_LED_RED, 0);
				gpio_set_value(BATTERY_LED_GREEN, 0);
				gpio_set_value(BATTERY_LED_BLUE, battery_led->phase);
				break;
			default:
				return;
		}
		battery_led->phase = battery_led->phase ? 0 : 1;
		delay = min((battery_led->phase ? msecs_to_jiffies(battery_led->off) 
			: msecs_to_jiffies(battery_led->on)), 100);
		schedule_delayed_work(&battery_led->work, delay);
	} else {
		switch (major) {
			case COLOR_RED:
				gpio_set_value(BATTERY_LED_RED, 1);
				gpio_set_value(BATTERY_LED_GREEN, 0);
				gpio_set_value(BATTERY_LED_BLUE, 0);
				break;
			case COLOR_GREEN:
				gpio_set_value(BATTERY_LED_RED, 0);
				gpio_set_value(BATTERY_LED_GREEN, 1);
				gpio_set_value(BATTERY_LED_BLUE, 0);
				break;
			case COLOR_BLUE:
				gpio_set_value(BATTERY_LED_RED, 0);
				gpio_set_value(BATTERY_LED_GREEN, 0);
				gpio_set_value(BATTERY_LED_BLUE, 1);
				break;
			default:
				break;
		}
	}

	logd("battery_led_work_func() OUT\r\n");
}

static int battery_led_probe(struct platform_device *pdev) 
{
	int ret;
	struct battery_led_device *battery_led;

	logd("battery_led_probe() IN\r\n");

	battery_led = kzalloc(sizeof(struct battery_led_device), GFP_KERNEL);
	if (battery_led == NULL) {
		return -ENOMEM;
	}
	
	battery_led->led.set_color = battery_led_set_color;
	battery_led->led.get_color = battery_led_get_color;
	battery_led->led.set_flash_mode = battery_led_set_flash_mode;
	battery_led->led.get_flash_mode = battery_led_get_flash_mode;
	battery_led->led.set_flash_on = battery_led_set_flash_on;
	battery_led->led.get_flash_on = battery_led_get_flash_on;
	battery_led->led.set_flash_off = battery_led_set_flash_off;
	battery_led->led.get_flash_off = battery_led_get_flash_off;
	battery_led->led.name = "battery";
	battery_led->on = BATTERY_LED_DEFAULT_ON;
	battery_led->off = BATTERY_LED_DEFAULT_OFF;
	INIT_DELAYED_WORK(&battery_led->work, battery_led_work_func);
	platform_set_drvdata(pdev, &battery_led);
	ret = smb_led_device_register(&pdev->dev, &battery_led->led);
	if (ret < 0) 
		goto err_register_smb_led_device;

	ret = gpio_request(BATTERY_LED_TAKE_CHARGE, "battery_led_tc");
	if (ret < 0) 
		goto err_request_gpio_tc;
	gpio_direction_output(BATTERY_LED_TAKE_CHARGE, 1);

	ret = gpio_request(BATTERY_LED_RED, "battery_led_red");
	if (ret < 0)
		goto err_request_gpio_red;
	gpio_direction_output(BATTERY_LED_RED, 0);

	ret = gpio_request(BATTERY_LED_GREEN, "battery_led_green");
	if (ret < 0)
		goto err_request_gpio_green;
	gpio_direction_output(BATTERY_LED_GREEN, 0);
	
	ret = gpio_request(BATTERY_LED_BLUE, "battery_led_blue");
	if (ret < 0) 
		goto err_request_gpio_blue;
	gpio_direction_output(BATTERY_LED_BLUE, 0);
	
	logd("battery_led_probe() OUT\r\n");

	return ret;

err_request_gpio_blue:
	gpio_free(BATTERY_LED_GREEN);
err_request_gpio_green:
	gpio_free(BATTERY_LED_RED);
err_request_gpio_red:
	gpio_direction_input(BATTERY_LED_TAKE_CHARGE);
	gpio_free(BATTERY_LED_TAKE_CHARGE);
err_request_gpio_tc:
	smb_led_device_unregister(&battery_led->led);
err_register_smb_led_device:
	return -EFAULT;
}

static int battery_led_remove(struct platform_device *pdev)
{
	struct battery_led_device *battery_led;
	battery_led = (struct battery_led_device *)platform_get_drvdata(pdev);
	smb_led_device_unregister(&battery_led->led);
	return 0;
}

static int battery_led_suspend(struct platform_device *pdev, pm_message_t state)
{
	gpio_direction_input(BATTERY_LED_TAKE_CHARGE);
	return 0;
}

static int battery_led_resume(struct platform_device *pdev)
{
	gpio_direction_output(BATTERY_LED_TAKE_CHARGE, 1);
	return 0;
}

static struct platform_driver led_battery_platform_driver = {
	.probe = battery_led_probe,
	.remove = battery_led_remove,
	.suspend = battery_led_suspend,
	.resume = battery_led_resume, 
	.driver = {
		.name = "smba1006_battery_led",
		.owner = THIS_MODULE, 
	}, 
};

static int leds_battery_init(void) 
{
	return platform_driver_register(&led_battery_platform_driver);
}

static void leds_battery_exit(void)
{
	platform_driver_unregister(&led_battery_platform_driver);
}

module_init(leds_battery_init);
module_exit(leds_battery_exit);
