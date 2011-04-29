/*
 * Copyright (C) 2010 Malata, Corp. All Right Reserved.
 *
 * Athor: LiuZheng<xmlz@malata.com>
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/leds_smb.h>

#define TAG						"WIFI_LED: "
#define WIFI_LED_DEV_NAME		"WIFI_LED"

extern void Nv_WIFI_LED_Control(unsigned int enable);
struct wifi_led_device {
	int color;
	struct smb_led_device led;
};

static int wifi_led_set_color(struct smb_led_device *led, unsigned int color)
{
	struct wifi_led_device *wifi_led;
	wifi_led = container_of(led, struct wifi_led_device, led);
	pr_debug(TAG "wifi_led_set_color\n");
	if (wifi_led->color != color) {
		pr_debug(TAG "color=%d\n", color);
		wifi_led->color = color;
		Nv_WIFI_LED_Control((color ? 1 : 0));
	}
	return 0;
}

static int wifi_led_get_color(struct smb_led_device *led, unsigned int *color)
{
	struct wifi_led_device *wifi_led;
	wifi_led = container_of(led, struct wifi_led_device, led);
	*color = wifi_led->color;
	return 0;
}

static int wifi_led_probe(struct platform_device *pdev)
{
	int ret;
	struct wifi_led_device *wifi_led;
	wifi_led = kzalloc(sizeof(struct wifi_led_device), GFP_KERNEL);
	if (wifi_led == NULL) {
		pr_err(TAG "kzalloc=NULL\n");
		return -ENOMEM;
	}
	wifi_led->led.set_color = wifi_led_set_color;
	wifi_led->led.get_color = wifi_led_get_color;
	wifi_led->led.name = "wifi";
	ret = smb_led_device_register(&pdev->dev, &wifi_led->led);
	if (ret < 0) {
		pr_err(TAG "smb_led_device_register failed\n");
		goto err_register_led;
	}
	Nv_WIFI_LED_Control(0);
	platform_set_drvdata(pdev, &wifi_led);
	return 0;
err_register_led:
	kfree(wifi_led);
	return ret;
}

static int wifi_led_remove(struct platform_device *pdev)
{
	struct wifi_led_device *wifi_led;
	wifi_led = (struct wifi_led_device *)platform_get_drvdata(pdev);
	smb_led_device_unregister(&wifi_led->led);
	kfree(wifi_led);
	return 0;
}

static int wifi_led_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct wifi_led_device *wifi_led;
	wifi_led = (struct wifi_led_device *)platform_get_drvdata(pdev);
	wifi_led->color = 0;
	Nv_WIFI_LED_Control(0);
	return 0;
}

static int wifi_led_resume(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver wifi_led_driver = {
	.probe = wifi_led_probe, 
	.remove = wifi_led_remove,
	.suspend = wifi_led_suspend,
	.resume = wifi_led_resume, 
	.driver = {
		.name = WIFI_LED_DEV_NAME, 
		.owner = THIS_MODULE, 
	}, 
};

static int wifi_led_init(void)
{
	return platform_driver_register(&wifi_led_driver);
}

static void wifi_led_exit(void)
{
	platform_driver_unregister(&wifi_led_driver);
}

module_init(wifi_led_init);
module_exit(wifi_led_exit);
