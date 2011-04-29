/*
 ***********************************************************************
 * File name 	: kenerl/driver/input/misc/dummy_sensor.c
 * Description 	: 
 * Athor 		: LiuZheng <xmlz@malata.com>
 * Date 		: 2010/06/13
 ***********************************************************************
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#define __DUMMY_SENSOR_GENERIC_DEBUG__			0

#if (__DUMMY_SENSOR_GENERIC_DEBUG__)
#define logd(x...)		do {printk(x);} while(0)
#else
#define logd(x)			do {} while(0)
#endif

static ssize_t
dummy_sensor_write_sysfs_sync_config(struct device *dev, 
		struct device_attribute *attr, 	const char *buffer, size_t count)
{
	struct input_dev *input_dev = (struct input_dev *)dev_get_drvdata(dev);
	if (input_dev) {
		input_event(input_dev, EV_SYN, SYN_CONFIG, 0);	
	}
	return count;
}

static DEVICE_ATTR(sync_config, 0777, NULL, dummy_sensor_write_sysfs_sync_config);

static int dummy_sensor_probe(struct platform_device *pdev)
{
	struct input_dev *input_dev;

	/* alloc an input device*/
	input_dev = input_allocate_device();
	if (!input_dev) {
		return -EINVAL;
	}

	/* init input_device */
	set_bit(EV_SYN, input_dev->evbit);
	input_dev->name = "dummy_sensor";

	/* register input device*/
	if (input_register_device(input_dev)) {
		goto failed_register_input;
	}

	/* keep the handle of this input device */
	dev_set_drvdata(&pdev->dev, input_dev);

	/* add sysfs entry */
	if (device_create_file(&pdev->dev, &dev_attr_sync_config)) {
		goto failed_create_sysfs;
	}

	logd("dummy_sensor: probe success\r\n");

	return 0;
failed_create_sysfs:
	input_unregister_device(input_dev);
failed_register_input:
	input_free_device(input_dev);

	logd("dummy_sensor: probe failed\r\n");

	return -EINVAL;
}

static int dummy_sensor_remove(struct platform_device *pdev)
{
	struct input_dev *input_dev;

	/* remove sysfs entry */
	device_remove_file(&pdev->dev, &dev_attr_sync_config);

	/* unregister input device */
	input_dev = (struct input_dev *)dev_get_drvdata(&pdev->dev);
	input_unregister_device(input_dev);

	return 0;
}

struct platform_driver dummy_sensor_driver = {
	.probe = dummy_sensor_probe,
	.remove = dummy_sensor_remove, 
	.driver = {
		.name = "dummy_sensor",
		.owner = THIS_MODULE,
	},
};

static int dummy_sensor_init(void)
{
	return platform_driver_register(&dummy_sensor_driver);
}

static void dummy_sensor_exit(void)
{
	platform_driver_unregister(&dummy_sensor_driver);
}

module_init(dummy_sensor_init);
module_exit(dummy_sensor_exit);
