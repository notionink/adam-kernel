/*
 * Copyright (C) 2010 Malata, Corp. All Right Reserved.
 *
 * Athor: LiuZheng <xmlz@malata.com>
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

#include <linux/switch_hdmi.h>

#define __SWITCH_HDMI_GENERIC_DEBUG__		0

#define TAG				"switch-hdmi:  "

#if __SWITCH_HDMI_GENERIC_DEBUG__
#define logd(x...)		do { printk(TAG x); } while(0)
#else
#define logd(x...)		do { } while(0)
#endif

#define loge(x...)		do { printk(TAG x); } while(0)

struct hdmi_switch_device {
	struct  switch_dev sdev;
	int		state;
	struct work_struct work;
};

static struct hdmi_switch_device *s_hdmi_switch = NULL;

static ssize_t
hdmi_show_interface(struct device *device, struct device_attribute *attr, char *buffer)
{
	struct hdmi_switch_device *hdmi_switch;
	hdmi_switch = s_hdmi_switch;
	return sprintf(buffer, "%d", hdmi_switch->state);
}

static ssize_t
hdmi_store_interface(struct device *device, struct device_attribute *attr, const char *buffer, ssize_t count)
{
	int state;
	struct hdmi_switch_device *hdmi_switch;
	hdmi_switch = s_hdmi_switch;
	state = simple_strtol(buffer, NULL, 10);
	hdmi_switch->state = (state) ? 1 : 0;
	schedule_work(&hdmi_switch->work);
	return count;
}

static DEVICE_ATTR(interface, 0777, hdmi_show_interface, hdmi_store_interface);

static void hdmi_switch_work_func(struct work_struct *work) 
{
	struct hdmi_switch_device *hdmi_switch  = container_of(work, 
			struct hdmi_switch_device, work);
	switch_set_state(&hdmi_switch->sdev, hdmi_switch->state);
}

static ssize_t hdmi_switch_print_state(struct switch_dev *sdev, char *buffer)
{
	struct hdmi_switch_device *hdmi_switch = container_of(sdev, 
			struct hdmi_switch_device, sdev);
	return sprintf(buffer, "%d", hdmi_switch->state);
}

static int hdmi_switch_probe(struct platform_device *pdev)
{
	int ret;
	struct hdmi_switch_device *hdmi_switch;
	hdmi_switch = kzalloc(sizeof(struct hdmi_switch_device), GFP_KERNEL);
	if (!hdmi_switch)
		return -ENOMEM;

	INIT_WORK(&hdmi_switch->work, hdmi_switch_work_func);
		

	hdmi_switch->sdev.name = HDMI_SWITCH_NAME;
	hdmi_switch->sdev.print_state = hdmi_switch_print_state;
	ret = switch_dev_register(&hdmi_switch->sdev);
	if (ret < 0) {
		logd("err_register_switch\n");
		goto err_register_switch;
	}

	ret = device_create_file(hdmi_switch->sdev.dev, &dev_attr_interface);
	if (ret < 0) {
		goto err_create_sysfs_file;
	}

	platform_set_drvdata(pdev, hdmi_switch);
	s_hdmi_switch = hdmi_switch;

	logd("hdmi_switch_probe() successed\n");

	return 0;

err_create_sysfs_file:
	switch_dev_unregister(&hdmi_switch->sdev);
err_register_switch:
	kfree(hdmi_switch);
	s_hdmi_switch = NULL;
	loge("hdmi_switch_probe() failed\n");
	return ret;
}

static int hdmi_switch_remove(struct platform_device *pdev)
{
	struct hdmi_switch_device *hdmi_switch = platform_get_drvdata(pdev);
	device_remove_file(hdmi_switch->sdev.dev, &dev_attr_interface);
	switch_dev_unregister(&hdmi_switch->sdev);
	kfree(hdmi_switch);
	s_hdmi_switch = NULL;
	return 0;
}

static int hdmi_switch_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct hdmi_switch_device *hdmi_switch = platform_get_drvdata(pdev);
	cancel_work_sync(&hdmi_switch->work);
	return 0;
}

static int hdmi_switch_resume(struct platform_device *pdev)
{
	struct hdmi_switch_device *hdmi_switch = platform_get_drvdata(pdev);
	schedule_work(&hdmi_switch->work);
	return 0;
}

static struct platform_driver hdmi_switch_platform_driver = {
	.probe = hdmi_switch_probe,
	.remove = hdmi_switch_remove, 
	.suspend = hdmi_switch_suspend, 
	.resume = hdmi_switch_resume, 
	.driver = {
		.name = HDMI_SWITCH_NAME,
		.owner = THIS_MODULE, 
	}, 
};

static int hdmi_switch_init(void)
{
	logd("hdmi_switch_init \n");
	return platform_driver_register(&hdmi_switch_platform_driver);
}

static void hdmi_switch_exit(void)
{
	logd("hdmi_switch_exit \n");
	platform_driver_unregister(&hdmi_switch_platform_driver);
}

module_init(hdmi_switch_init);
module_exit(hdmi_switch_exit);
