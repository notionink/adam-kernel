/*
 * Copyright (C) 2010 Malata, Corp. All Right Reserved.
 *
 * Athor: LiuZheng <xmlz@malata.com>
 */
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <linux/pixel_qi_screen_ctrl.h>

#define TAG 			"pixelQi: "

struct ps_ctrl_device {
	int 	gpio;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct  early_suspend early_suspend;
#endif
};

static ssize_t
ps_ctrl_show(struct device *dev, struct device_attribute *attr, char *buffer);
static ssize_t
ps_ctrl_store(struct device *dev, struct device_attribute *attr, const char *buffer, ssize_t count);

static DEVICE_ATTR(state, 0777, ps_ctrl_show, ps_ctrl_store);

static ssize_t
ps_ctrl_show(struct device *device, struct device_attribute *attr, char *buffer)
{
	int state;
	struct ps_ctrl_device *dev;
	dev = (struct ps_ctrl_device *)dev_get_drvdata(device);
	state = gpio_get_value(dev->gpio);
	return sprintf(buffer, "%d", state);
}

static ssize_t
ps_ctrl_store(struct device *device, struct device_attribute *attr, const char *buffer, ssize_t count)
{
	int state;
	struct ps_ctrl_device *dev;
	dev = (struct ps_ctrl_device *)dev_get_drvdata(device);
	state = simple_strtol(buffer, NULL, 10);
	gpio_set_value(dev->gpio, state ? 1 : 0);
	return count;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ps_ctrl_early_suspend(struct early_suspend *es)
{
	struct ps_ctrl_device *dev;
	dev = container_of(es, struct ps_ctrl_device, early_suspend);
	gpio_set_value(dev->gpio, 1);
	return 0;
}

static void ps_ctrl_late_resume(struct early_suspend *es)
{
	struct ps_ctrl_device *dev;
	dev = container_of(es, struct ps_ctrl_device, early_suspend);
	gpio_set_value(dev->gpio, 1);
	return 0;
}
#endif

static int ps_ctrl_probe(struct platform_device *pdev)
{
	int ret;
	struct ps_ctrl_device *dev;

	dev = (struct ps_ctrl_device *)kzalloc(sizeof(struct ps_ctrl_device), GFP_KERNEL);
	if (dev == NULL) {
		return -ENOMEM;
	}

	dev->gpio = (int)pdev->dev.platform_data;
	if (dev->gpio <= 0) {
		pr_err(TAG "err_platform_data\n");
		goto err_platform_data;
	}
	ret = device_create_file(&pdev->dev, &dev_attr_state);
	if (ret < 0) {
		pr_err(TAG "err_create_sysfs\n");
		goto err_create_sysfs;
	}
	ret = gpio_request(dev->gpio, PQS_CTRL_DRV_NAME);
	if (ret < 0) {
		pr_err(TAG "err_request_gpio\n");
		goto err_request_gpio;
	}
	gpio_direction_output(dev->gpio, 1);

#ifdef CONFIG_HAS_EARLYSUSPEND
	dev->early_suspend.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING;
	dev->early_suspend.suspend = ps_ctrl_early_suspend;
	dev->early_suspend.resume = ps_ctrl_late_resume;
	register_early_suspend(&dev->early_suspend);
#endif

	platform_set_drvdata(pdev, (void*)dev);
	return 0;

err_request_gpio:
	device_remove_file(&pdev->dev, &dev_attr_state);
err_create_sysfs:
err_platform_data:
	kfree(dev);
	return -EFAULT;
}

static int ps_ctrl_remove(struct platform_device *pdev)
{
	struct ps_ctrl_device *dev;
	dev = (struct ps_ctrl_device *) platform_get_drvdata(pdev);
	unregister_early_suspend(&dev->early_suspend);
	gpio_direction_input(dev->gpio);
	gpio_free(dev->gpio);
	device_remove_file(&pdev->dev, &dev_attr_state);
	kfree(dev);
	return 0;
}

static int ps_ctrl_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct ps_ctrl_device *dev;
	dev = (struct ps_ctrl_device *)platform_get_drvdata(pdev);
	gpio_set_value(dev->gpio, 1);
	return 0;
}

static int ps_ctrl_resume(struct platform_device *pdev)
{
	struct ps_ctrl_device *dev;
	dev = (struct ps_ctrl_device *)platform_get_drvdata(pdev);
	gpio_set_value(dev->gpio, 1);
	return 0;
}

static struct platform_driver ps_ctrl_driver = {
	.probe = ps_ctrl_probe, 
	.remove = ps_ctrl_remove, 
	.suspend = ps_ctrl_suspend,
	.resume = ps_ctrl_resume, 
	.driver = {
		.name = PQS_CTRL_DRV_NAME, 
		.owner = THIS_MODULE, 
	},
};

static int ps_ctrl_init()
{
	return platform_driver_register(&ps_ctrl_driver);
}

static void ps_ctrl_exit()
{
	platform_driver_unregister(&ps_ctrl_driver);
}

module_init(ps_ctrl_init);
module_exit(ps_ctrl_exit);

