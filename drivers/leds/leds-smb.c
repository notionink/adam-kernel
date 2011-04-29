/*
 * leds-smb.c - SMB led
 * 
 * Copyright (C) 2010 Malata Corp.
 * 
 * Athor: LiuZheng <xmlz@malata.com>
 * 
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/leds_smb.h>
#include <linux/platform_device.h>

static ssize_t 
smb_show_flash_mode(struct device *device, struct device_attribute *attr, char *buffer);
static ssize_t
smb_store_flash_mode(struct device *device, struct device_attribute *attr, const char *buffer, size_t count);

static ssize_t
smb_show_flash_on(struct device *device, struct device_attribute *attr, char *buffer);
static ssize_t
smb_store_flash_on(struct device *device, struct device_attribute *attr, const char *buffer, size_t count);

static ssize_t
smb_show_flash_off(struct device *device, struct device_attribute *attr, char *buffer);
static ssize_t
smb_store_flash_off(struct device *device, struct device_attribute *attr, const char *buffer, size_t count);

static ssize_t 
smb_show_color(struct device *device, struct device_attribute *attr, char *buffer);
static ssize_t
smb_store_color(struct device *device, struct device_attribute *attr, const char *buffer, ssize_t count);

static DEVICE_ATTR(flash_mode, 0777, smb_show_flash_mode, smb_store_flash_mode); 
static DEVICE_ATTR(flash_on, 0777, smb_show_flash_on, smb_store_flash_on);
static DEVICE_ATTR(flash_off, 0777, smb_show_flash_off, smb_store_flash_off);
static DEVICE_ATTR(color, 0777, smb_show_color, smb_store_color);

static ssize_t 
smb_show_flash_mode(struct device *device, struct device_attribute *attr, char *buffer)
{
	unsigned int mode;
	struct smb_led_device *smb;

	smb = (struct smb_led_device *) dev_get_drvdata(device);
	if (smb->get_flash_mode && (*smb->get_flash_mode)(smb, &mode) == 0) {
		if (mode == LED_FLASH_NONE || mode == LED_FLASH_TIMED ||
				mode == LED_FLASH_HARDWARE) {
			return sprintf(buffer, "%d", mode);
		}
	}
	return 0;
}

static ssize_t
smb_store_flash_mode(struct device *device, struct device_attribute *attr, const char *buffer, size_t count)
{
	unsigned int mode;
	struct smb_led_device *smb;

	smb = (struct smb_led_device *) dev_get_drvdata(device);
	if (count > 2 && (strncmp(buffer, "0x", 2) == 0 || strncmp(buffer, "0X", 2) == 0)) {
		mode = simple_strtol(buffer, NULL, 16);
	} else {
		mode = simple_strtol(buffer, NULL, 10);
	}
	switch (mode) {
		case LED_FLASH_NONE:
		case LED_FLASH_TIMED:
		case LED_FLASH_HARDWARE:
			if (smb->set_flash_mode&& (*smb->set_flash_mode)(smb, mode) == 0)
				return count;
			break;
		default:
			return -EINVAL;
	}

	return 0;
}

static ssize_t
smb_show_flash_on(struct device *device, struct device_attribute *attr, char *buffer)
{
	unsigned int on;
	struct smb_led_device *smb;

	smb = (struct smb_led_device *) dev_get_drvdata(device);
	if (smb->get_flash_on && (*smb->get_flash_on)(smb, &on) == 0)
		return sprintf(buffer, "%d", on);
	return 0;
}

static ssize_t
smb_store_flash_on(struct device *device, struct device_attribute *attr, const char *buffer, size_t count)
{
	unsigned int on;
	struct smb_led_device *smb;

	smb = (struct smb_led_device *) dev_get_drvdata(device);
	if (count > 2 && (strncmp(buffer, "0x", 2) == 0 || strncmp(buffer, "0X", 2) == 0)) {
		on = simple_strtol(buffer, NULL, 16);
	} else {
		on = simple_strtol(buffer, NULL, 10);
	}
	if (smb->set_flash_on && (*smb->set_flash_on)(smb, on) < 0) 
		return -EFAULT;
	return count;
}

static ssize_t
smb_show_flash_off(struct device *device, struct device_attribute *attr, char *buffer)
{
	unsigned int off;
	struct smb_led_device *smb;

	smb = (struct smb_led_device *) dev_get_drvdata(device);
	if (smb->get_flash_off && (*smb->get_flash_off)(smb, &off) == 0)
		return sprintf(buffer, "%d", off);
	return 0;
}

static ssize_t
smb_store_flash_off(struct device *device, struct device_attribute *attr, const char *buffer, size_t count)
{
	unsigned int off;
	struct smb_led_device *smb;

	smb = (struct smb_led_device *) dev_get_drvdata(device);

	if (count >2 && (strncmp(buffer, "0x", 2) == 0 || strncmp(buffer, "0X", 2) == 0)) {
		off = simple_strtol(buffer + 2, NULL, 16);
	} else {
		off = simple_strtol(buffer, NULL, 10);
	}
	if (smb->set_flash_off && (*smb->set_flash_off)(smb, off) < 0)
		return -EFAULT;
	return count;
}

static ssize_t 
smb_show_color(struct device *device, struct device_attribute *attr, char *buffer)
{
	unsigned int color;
	struct smb_led_device *smb;

	smb = (struct smb_led_device *) dev_get_drvdata(device);
	if (smb->get_color && (*smb->get_color)(smb, &color) == 0) {
		return sprintf(buffer, "%0x80x", color);
	}
	return 0;
}

static ssize_t
smb_store_color(struct device *device, struct device_attribute *attr, const char *buffer, ssize_t count)
{
	unsigned int color;
	struct smb_led_device *smb;

	smb = (struct smb_led_device *) dev_get_drvdata(device);
	if (count > 2 && (strncmp(buffer, "0x", 2) == 0 || strncmp(buffer, "0X", 2) == 0)) {
		color = simple_strtol(buffer, NULL, 16);
	} else {
		color = simple_strtol(buffer, NULL, 10);
	}
	if (smb->set_color && (*smb->set_color)(smb, color) < 0)
		return -EFAULT;
	return count;
}

static int smb_set_brightness(struct led_classdev *led_cdev, enum led_brightness brightness)
{
	struct smb_led_device *smb;
	smb = (struct smb_led_device *) dev_get_drvdata(led_cdev->dev);
	switch (brightness) {
		case LED_HALF:
			if (smb->set_color) (*smb->set_color)(smb, COLOR_WHITE);
			break;
		case LED_FULL:
			if (smb->set_color) (*smb->set_color)(smb, COLOR_WHITE);
			break;
		case LED_OFF:
			if (smb->set_color) (*smb->set_color)(smb, COLOR_BLACK);
		default:
			return -EINVAL;
	}
	return 0;
}

static enum led_brightness smb_get_brightness(struct led_classdev *led_cdev)
{
	struct smb_led_device *smb;
	smb = (struct smb_led_device *) dev_get_drvdata(led_cdev->dev);
	if (smb->get_color) {
		//if ((*smb->get_color)(smb, &color) < 0) 
		//	return LED_OFF;
	}
	return LED_OFF;
}

int smb_led_device_register(struct device *parent, struct smb_led_device *device)
{
	int ret;

	device->led_cdev.name = device->name;
	device->led_cdev.default_trigger = NULL;
	device->led_cdev.brightness_set = smb_set_brightness;
	device->led_cdev.brightness_get = smb_get_brightness;
	ret = led_classdev_register(parent, &device->led_cdev);
	if (ret < 0) 
		goto err_register_led_classdev;

	dev_set_drvdata(device->led_cdev.dev, device);

	ret = device_create_file(device->led_cdev.dev, &dev_attr_flash_mode);
	if (ret < 0)
		goto err_create_sysfs_flash_mode;

	ret = device_create_file(device->led_cdev.dev, &dev_attr_flash_on);
	if (ret < 0) 
		goto err_create_sysfs_flash_on;

	ret = device_create_file(device->led_cdev.dev, &dev_attr_flash_off);
	if (ret < 0)
		goto err_create_sysfs_flash_off;

	ret = device_create_file(device->led_cdev.dev, &dev_attr_color);
	if (ret < 0)
		goto err_create_sysfs_color;

	return 0;

err_create_sysfs_color:
	device_remove_file(device->led_cdev.dev, &dev_attr_flash_off);
err_create_sysfs_flash_off:
	device_remove_file(device->led_cdev.dev, &dev_attr_flash_on);
err_create_sysfs_flash_on:
	device_remove_file(device->led_cdev.dev, &dev_attr_flash_mode);
err_create_sysfs_flash_mode:
	led_classdev_unregister(&device->led_cdev);
err_register_led_classdev:
	return -1;
}
EXPORT_SYMBOL_GPL(smb_led_device_register);

void smb_led_device_unregister(struct smb_led_device *device)
{
	device_remove_file(device->led_cdev.dev, &dev_attr_flash_off);
	device_remove_file(device->led_cdev.dev, &dev_attr_flash_on);
	device_remove_file(device->led_cdev.dev, &dev_attr_flash_mode);
	led_classdev_unregister(&device->led_cdev);
}
EXPORT_SYMBOL_GPL(smb_led_device_unregister);
