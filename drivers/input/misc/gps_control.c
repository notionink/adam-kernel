/*
 * Filename    : /android/kernel/driver/input/misc/gps_control.c
 * Description : Drivder for GPS module(MALATA_SMBA1102).
 * Athor       : LiuZheng <xmlz@malata.com>
 * Date        : 2010/07/10
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include "nvodm_services.h"

#define __GPS_CONTROL_GENERIC_DEBUG__		1

#if (__GPS_CONTROL_GENERIC_DEBUG__)
#define logd(x...) 		do{ printk(x); } while(0)
#else
#define logd(x...)		do {} while (0);
#endif

#define TAG		"gps_control:	"

#define GPS_ENABLE_GPIO_PORT			('v' - 'a')
#define GPS_ENABLE_GPIO_PIN				3

struct gps_control_dev {
	int		enable;
	bool	reenable;
	struct	semaphore sem;
	struct	input_dev *input_dev;

	NvOdmServicesGpioHandle gpio_handle;
	NvOdmGpioPinHandle pin_handle;
};

static struct gps_control_dev s_gps_control_dev;

/* Forward reference */
static int gps_control_enable(struct gps_control_dev *dev, bool enable);

static ssize_t
gps_control_read_sysfs_enable(struct device *dev,
		struct device_attribute *attr, char *buffer)
{
	struct gps_control_dev *gps_control;

	gps_control = (struct gps_control_dev *)dev_get_drvdata(dev);
	return sprintf(buffer, "%d", gps_control->enable);
}

static ssize_t 
gps_control_write_sysfs_enable(struct device *dev, 
		struct device_attribute *attr, char *buffer, size_t count)
{
	int value;
	struct gps_control_dev *gps_control;

	gps_control = (struct gps_control_dev *)dev_get_drvdata(dev);
	value = simple_strtol(buffer, NULL, 10);

	logd(TAG "gps_control_write_sysfs_enable() enable=%d\r\n", value);

#ifdef CONFIG_7379Y_V11
	/* Do notiong here */
#else
	down(&gps_control->sem);
	gps_control_enable(gps_control, value?true:false);
	up(&gps_control->sem);
#endif
	return count;
}

static DEVICE_ATTR(enable, 0777, gps_control_read_sysfs_enable, gps_control_write_sysfs_enable);

static int gps_control_enable(struct gps_control_dev *dev, bool enable)
{
	if (dev->enable == enable) {
		return 0;;
	}
	dev->enable = enable;
	NvOdmGpioSetState(dev->gpio_handle, dev->pin_handle, enable);
	return 0;
}

static int gps_control_probe(struct platform_device *pdev)
{
	struct gps_control_dev *dev = &s_gps_control_dev;

	logd(TAG "gps_control_probe\r\n");
	
	memset(dev, 0, sizeof(struct gps_control_dev));

	dev->input_dev = input_allocate_device();
	if (dev->input_dev == NULL) {
		goto failed;
	}
	set_bit(EV_SYN, dev->input_dev->evbit);
	dev->input_dev->name = "gps_control";  
	if (input_register_device(dev->input_dev) != 0) {
		goto failed_register_input;
	}

	dev->gpio_handle = NvOdmGpioOpen();
	if (!dev->gpio_handle) {
		goto failed_open_gpio;
	}
	dev->pin_handle = NvOdmGpioAcquirePinHandle(dev->gpio_handle, GPS_ENABLE_GPIO_PORT, GPS_ENABLE_GPIO_PIN);
	if (!dev->pin_handle) {
		goto failed_acquire_pin;
	}
	NvOdmGpioConfig(dev->gpio_handle, dev->pin_handle, NvOdmGpioPinMode_Output);

#ifdef CONFIG_7379Y_V11
	/*
	 * GPS and Magnetic sensor use the sampe power pin on 7379Y_V11, so we
	 * need to open this power while booting.
	 */
	NvOdmGpioSetState(dev->gpio_handle, dev->pin_handle, 1);
#else
	NvOdmGpioSetState(dev->gpio_handle, dev->pin_handle, 0);
#endif

	init_MUTEX(&dev->sem);
	platform_set_drvdata(pdev, dev);
	if (device_create_file(&pdev->dev, &dev_attr_enable) != 0) {
		goto failed_add_sysfs;
	}

	logd(TAG "gps_control_probe success\r\n");
	
	return 0;

failed_add_sysfs:
	platform_set_drvdata(pdev, NULL);
	NvOdmGpioReleasePinHandle(dev->gpio_handle, dev->pin_handle);
failed_acquire_pin:
	NvOdmGpioClose(dev->gpio_handle);
failed_open_gpio:
	input_unregister_device(dev->input_dev);
failed_register_input:
	input_free_device(dev->input_dev);
failed:
	logd(TAG "gps_control_probe failed");
	
	return -1;
}

static int gps_control_remove(struct platform_device *pdev)
{
	struct gps_control_dev *gps_control;

	gps_control = (struct gps_control_dev *)platform_get_drvdata(pdev);
	if (gps_control != NULL) {
		NvOdmGpioReleasePinHandle(gps_control->gpio_handle, gps_control->pin_handle);
		gps_control->pin_handle = 0;
		NvOdmGpioClose(gps_control->gpio_handle);
		gps_control->gpio_handle = 0;
		if (gps_control->input_dev) {
			input_unregister_device(gps_control->input_dev);
			input_free_device(gps_control->input_dev);
			gps_control->input_dev = 0;
		}
	}

	return 0;
}

static int gps_control_suspend(struct platform_device *pdev, pm_message_t state)
{
	/* What should we do ? disable the gps module? */
	struct gps_control_dev *gps_control;

	gps_control = (struct gps_control_dev *)platform_get_drvdata(pdev);
	if (gps_control != NULL) {
		gps_control->reenable = gps_control->enable;
		if (gps_control->enable) {
			gps_control_enable(gps_control, false);
		}
	}
	return 0;
}

static int gps_control_resume(struct platform_device *pdev)
{
	struct gps_control_dev *gps_control;

	gps_control = (struct gps_control_dev *)platform_get_drvdata(pdev);
	
	if (gps_control && gps_control->reenable) {
		gps_control->reenable = false;
		gps_control_enable(gps_control, true);
	}
	return 0;
}

static struct platform_driver gps_control_driver = {
	.probe = gps_control_probe,
	.remove = gps_control_remove,
	.suspend = gps_control_suspend,
	.resume = gps_control_resume,
	.driver = {
		.name = "gps_control",
		.owner = THIS_MODULE,
	},
};

static int gps_control_init(void)
{
	logd(TAG "gps_control_init\r\n");
	return platform_driver_register(&gps_control_driver);
}

static void gps_control_exit(void)
{
	platform_driver_unregister(&gps_control_driver);
}

module_init(gps_control_init);
module_exit(gps_control_exit);

