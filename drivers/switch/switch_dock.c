/*
 * Copyright (C) 2010 Malata, Corp. All Right Reserved.
 *
 * Athor: LiuZheng <xmlz@malata.com>
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/switch.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>

#include <linux/switch_dock.h>

#define __SWITCH_DOCK_GENERIC_DEBUG__		0

#define TAG				"switch-dock:  "

#if __SWITCH_DOCK_GENERIC_DEBUG__
#define logd(x...)		do { printk(TAG x); } while(0)
#else
#define logd(x...)		do { } while(0)
#endif

#define loge(x...)		do { printk(TAG x); } while(0)

#define BIT_DESKTOP		0
#define BIT_CAR			1

struct dock_switch_device {
	struct switch_dev sdev;
	int 	gpio_desktop;
	int 	irq_desktop;
	int	gpio_desktop_active_low;
	int 	gpio_car;
	int 	irq_car;
	int	gpio_car_active_low;
	int	state;
	struct work_struct work;
};

static void dock_switch_work_func(struct work_struct *work) 
{
	int state = 0, value;
	struct dock_switch_device *dock_switch  = container_of(work, 
			struct dock_switch_device, work);
	
	logd("dock_switch_work_func\n");

	if (dock_switch->gpio_desktop) {
		value = gpio_get_value(dock_switch->gpio_desktop);
		logd("desktop_dock = %d", value);
		if (dock_switch->gpio_desktop_active_low) 
			value = !value;
		state |= (value & 0x1) << BIT_DESKTOP;
	}

	if (dock_switch->gpio_car) {
		value = gpio_get_value(dock_switch->gpio_car);
		if (dock_switch->gpio_car_active_low)
			value = !value;
		state |= (value & 0x1) << BIT_CAR;
	}

	logd("state = %d\n", state);

	if (dock_switch->state != state) {
		dock_switch->state = state;
		switch_set_state(&dock_switch->sdev, state ? 1 : 0);
	}
}

static irqreturn_t dock_switch_irq_handler(int irq, void *arg)
{
	struct dock_switch_device *dock_switch = (struct dock_switch_device *)arg;
	logd("dock_switch_irq_handler\n");
	schedule_work(&dock_switch->work);
	return IRQ_HANDLED;
}

static ssize_t dock_switch_print_state(struct switch_dev *sdev, char *buffer)
{
	struct dock_switch_device *dock_switch = container_of(sdev, 
			struct dock_switch_device, sdev);
	return sprintf(buffer, "%d", dock_switch->state);
}

static int dock_switch_probe(struct platform_device *pdev)
{
	int ret;
	struct dock_switch_device *dock_switch;
	struct dock_switch_platform_data *pdata = (struct dock_switch_platform_data *)
			pdev->dev.platform_data;
	
	if (!pdata || (!pdata->gpio_desktop && !pdata->gpio_car))
		return -EBUSY;
	
	dock_switch = kzalloc(sizeof(struct dock_switch_device), GFP_KERNEL);
	if (!dock_switch)
		return -ENOMEM;

	if (pdata->gpio_desktop) {
		dock_switch->gpio_desktop = pdata->gpio_desktop;
		dock_switch->gpio_desktop_active_low = pdata->gpio_desktop_active_low;
		ret = gpio_request(dock_switch->gpio_desktop, "switch_desktop");
		if (ret < 0) {
			loge("err_request_gpio_desktop\n");
			goto err_request_gpio_desktop;
		}
		gpio_direction_input(dock_switch->gpio_desktop);
		dock_switch->irq_desktop = gpio_to_irq(dock_switch->gpio_desktop);
		ret = request_irq(dock_switch->irq_desktop, dock_switch_irq_handler, 
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "switch_desktop", 
				dock_switch);
		if (ret < 0) {
			loge("err_request_irq_desktop\n");
			goto err_request_irq_desktop;
		}
	}

	if (pdata->gpio_car) {
		dock_switch->gpio_car = pdata->gpio_car;
		dock_switch->gpio_car_active_low = pdata->gpio_car_active_low;
		ret = gpio_request(dock_switch->gpio_car, "switch_car");
		if (ret < 0) {
			loge("err_request_gpio_car\n");
			goto err_request_gpio_car;
		}
		gpio_direction_input(dock_switch->gpio_car);
		dock_switch->irq_car = gpio_to_irq(dock_switch->gpio_car);
		ret = request_irq(dock_switch->irq_car, dock_switch_irq_handler, 
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "switch_car",
				dock_switch);
		if (ret < 0) {
			loge("err_request_irq_car\n");
			goto err_request_irq_car;
		}
	}

	INIT_WORK(&dock_switch->work, dock_switch_work_func);
	
	dock_switch->sdev.name = DOCK_SWITCH_NAME;
	dock_switch->sdev.print_state = dock_switch_print_state;
	ret = switch_dev_register(&dock_switch->sdev);
	if (ret < 0) {
		logd("err_register_switch\n");
		goto err_register_switch;
	}

	if (dock_switch->irq_car) 
		enable_irq(dock_switch->irq_car);
	if (dock_switch->irq_desktop)
		enable_irq(dock_switch->irq_desktop);

	platform_set_drvdata(pdev, dock_switch);

	logd("dock_switch_probe() successed\n");

	return 0;

err_register_switch:
	if (dock_switch->irq_car)
		free_irq(dock_switch->irq_car, NULL);
err_request_irq_car:
	if (dock_switch->gpio_car) 
		gpio_free(dock_switch->gpio_car);
err_request_gpio_car:
	if (dock_switch->irq_desktop)
		free_irq(dock_switch->irq_desktop, NULL);
err_request_irq_desktop:
	if (dock_switch->gpio_desktop)
		gpio_free(dock_switch->gpio_desktop);
err_request_gpio_desktop:
	kfree(dock_switch);

	loge("dock_switch_probe() failed\n");

	return ret;
}

static int dock_switch_remove(struct platform_device *pdev)
{
	struct dock_switch_device *dock_switch = platform_get_drvdata(pdev);
	switch_dev_unregister(&dock_switch->sdev);
	if (dock_switch->irq_car)
		free_irq(dock_switch->irq_car, NULL);
	if (dock_switch->gpio_car) 
		gpio_free(dock_switch->gpio_car);
	if (dock_switch->irq_desktop)
		free_irq(dock_switch->irq_desktop, NULL);
	if (dock_switch->gpio_desktop)
		gpio_free(dock_switch->gpio_desktop);
	kfree(dock_switch);
	return 0;
}

static int dock_switch_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct dock_switch_device *dock_switch = platform_get_drvdata(pdev);
	cancel_work_sync(&dock_switch->work);
	if (dock_switch->irq_car) 
		disable_irq(dock_switch->irq_car);
	if (dock_switch->irq_desktop)
		disable_irq(dock_switch->irq_desktop);
	return 0;
}

static int dock_switch_resume(struct platform_device *pdev)
{
	struct dock_switch_device *dock_switch = platform_get_drvdata(pdev);
	if (dock_switch->irq_car) 
		enable_irq(dock_switch->irq_car);
	if (dock_switch->irq_desktop)
		enable_irq(dock_switch->irq_desktop);
	schedule_work(&dock_switch->work);
	return 0;
}

static struct platform_driver dock_switch_platform_driver = {
	.probe = dock_switch_probe,
	.remove = dock_switch_remove, 
	.suspend = dock_switch_suspend, 
	.resume = dock_switch_resume, 
	.driver = {
		.name = DOCK_SWITCH_NAME,
		.owner = THIS_MODULE, 
	}, 
};

static int dock_switch_init(void)
{
	logd("dock_switch_init \n");
	return platform_driver_register(&dock_switch_platform_driver);
}

static void dock_switch_exit(void)
{
	logd("dock_switch_exit \n");
	platform_driver_unregister(&dock_switch_platform_driver);
}

module_init(dock_switch_init);
module_exit(dock_switch_exit);
