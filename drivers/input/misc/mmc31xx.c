/*
 * Copyright (C) 2010 MEMSIC, Inc.
 *
 * Initial Code:
 *	Robbie Cao
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/sysctl.h>
#include <asm/uaccess.h>

#include <linux/mmc31xx.h>
#include <nvodm_services.h>
#include <linux/platform_device.h>

#define TAG		MMC31XX_DEV_NAME

#define MAX_FAILURE_COUNT	3
#define MMC31XX_DELAY_TM	10	/* ms */
#define MMC31XX_DELAY_SET	10	/* ms */
#define MMC31XX_DELAY_RST	10	/* ms */
#define MMC31XX_DELAY_STDN	1	/* ms */

#define MMC31XX_RETRY_COUNT	3
#define MMC31XX_RESET_INTV	10

//MMC31XX_I2C_ADDR
#define MMC31XX_I2C_ADDRESS						(0x30 << 1)
#define MMC31XX_I2C_SPEED_KHZ					(100)
#define MMC31XX_I2C_TIMEOUT_MS					(1000)

static u32 read_idx = 0;

struct mmc31xx_device {
	unsigned int i2c_instance;
	unsigned int i2c_address;
	NvOdmServicesI2cHandle i2c;
};

static struct mmc31xx_device *this_client;

static int mmc31xx_i2c_rx_data(unsigned char *buf, int len)
{
	int i;
	char cmd;
	NvOdmI2cStatus I2cStatus;
	NvOdmI2cTransactionInfo I2cTransactionInfo[2];
	pr_debug(TAG "mmc31xx_i2c_rx_data\n");
	cmd = buf[0];
	I2cTransactionInfo[0].Flags = NVODM_I2C_IS_WRITE;
	I2cTransactionInfo[0].Address = this_client->i2c_address;
	I2cTransactionInfo[0].Buf = &cmd;
	I2cTransactionInfo[0].NumBytes = 1;
	I2cTransactionInfo[1].Flags = 0;
	I2cTransactionInfo[1].Address = this_client->i2c_address | 0x1;
	I2cTransactionInfo[1].Buf = buf;
	I2cTransactionInfo[1].NumBytes = len;

	for (i = 0; i < MMC31XX_RETRY_COUNT; i++) {
	I2cStatus = NvOdmI2cTransaction(this_client->i2c, I2cTransactionInfo, 2, 
			MMC31XX_I2C_SPEED_KHZ, MMC31XX_I2C_TIMEOUT_MS);
		if (I2cStatus == NvOdmI2cStatus_Success) {
			break;
		}
		mdelay(10);
	}
	if (i >= MMC31XX_RETRY_COUNT) {
		pr_debug(TAG "i2c_address = 0x%0x", this_client->i2c_address);
		pr_err("mmc31xx_i2c_write problem, err = %d\r\n", I2cStatus);
		return -EIO;
	}

#ifdef DEBUG
	for (i = 0; i < len; i++) {
		printk("cmd = %d, buf[%d] = %d\n", cmd, i, buf[i]);
	}
#endif

	return 0;
}

static int mmc31xx_i2c_tx_data(unsigned char *buf, int len) 
{
	unsigned char i;
	NvOdmI2cStatus I2cStatus;
	NvOdmI2cTransactionInfo I2cTransactionInfo;
	pr_debug(TAG "mmc31xx_i2c_tx_data\n");
	
	I2cTransactionInfo.Flags = NVODM_I2C_IS_WRITE;
	I2cTransactionInfo.Address = this_client->i2c_address;
	I2cTransactionInfo.Buf = buf;
	I2cTransactionInfo.NumBytes = len;
	for (i = 0; i < MMC31XX_RETRY_COUNT; i++) {
		I2cStatus = NvOdmI2cTransaction(this_client->i2c, &I2cTransactionInfo, 1, 
				MMC31XX_I2C_SPEED_KHZ, MMC31XX_I2C_TIMEOUT_MS);
		if (I2cStatus == NvOdmI2cStatus_Success) {
			break;
		}
		mdelay(10);
	}
	if (i >= MMC31XX_RETRY_COUNT) {
		pr_debug(TAG "i2c_address = 0x%0x", this_client->i2c_address);
		pr_err("mmc31xx_i2c_write problem, err = %d\r\n", I2cStatus);
		return -EIO;
	}

	return 0;
}

static int mmc31xx_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}

static int mmc31xx_release(struct inode *inode, struct file *file)
{
	return 0;
}

static int mmc31xx_ioctl(struct inode *inode, struct file *file, 
	unsigned int cmd, unsigned long arg)
{
	void __user *pa = (void __user *)arg;
	unsigned char data[16] = {0};
	int vec[3] = {0};

	switch (cmd) {
	case MMC31XX_IOC_TM:
		pr_debug(TAG "MMC31XX_IOC_TM\n");
		data[0] = MMC31XX_REG_CTRL;
		data[1] = MMC31XX_CTRL_TM;
		if (mmc31xx_i2c_tx_data(data, 2) < 0) {
			return -EFAULT;
		}
		/* wait TM done for coming data read */
		msleep(MMC31XX_DELAY_TM);
		break;
	case MMC31XX_IOC_SET:
		pr_debug(TAG "MMC31XX_IOC_SET\n");
		data[0] = MMC31XX_REG_CTRL;
		data[1] = MMC31XX_CTRL_SET;
		if (mmc31xx_i2c_tx_data(data, 2) < 0) {
			return -EFAULT;
		}
		/* wait external capacitor charging done for next SET/RESET */
		msleep(MMC31XX_DELAY_SET);
		break;
	case MMC31XX_IOC_RESET:
		pr_debug(TAG "IOC_RESET\n");
		data[0] = MMC31XX_REG_CTRL;
		data[1] = MMC31XX_CTRL_RST;
		if (mmc31xx_i2c_tx_data(data, 2) < 0) {
			return -EFAULT;
		}
		/* wait external capacitor charging done for next SET/RESET */
		msleep(MMC31XX_DELAY_RST);
		break;
	case MMC31XX_IOC_READ:
		data[0] = MMC31XX_REG_DATA;
		if (mmc31xx_i2c_rx_data(data, 6) < 0) {
			return -EFAULT;
		}
		vec[0] = data[0] << 8 | data[1];
		vec[1] = data[2] << 8 | data[3];
		vec[2] = data[4] << 8 | data[5];
		pr_debug(TAG "[X - %04x] [Y - %04x] [Z - %04x]\n", 
			vec[0], vec[1], vec[2]);
		if (copy_to_user(pa, vec, sizeof(vec))) {
			return -EFAULT;
		}
		break;
	case MMC31XX_IOC_READXYZ:
		/* do RESET/SET every MMC31XX_RESET_INTV times read */
		if (!(read_idx % MMC31XX_RESET_INTV)) {
			/* RESET */
			data[0] = MMC31XX_REG_CTRL;
			data[1] = MMC31XX_CTRL_RST;
			/* not check return value here, assume it always OK */
			mmc31xx_i2c_tx_data(data, 2);
			/* wait external capacitor charging done for next SET/RESET */
			msleep(MMC31XX_DELAY_SET);
			/* SET */
			data[0] = MMC31XX_REG_CTRL;
			data[1] = MMC31XX_CTRL_SET;
			/* not check return value here, assume it always OK */
			mmc31xx_i2c_tx_data(data, 2);
			msleep(MMC31XX_DELAY_STDN);
		}
		/* send TM cmd before read */
		data[0] = MMC31XX_REG_CTRL;
		data[1] = MMC31XX_CTRL_TM;
		/* not check return value here, assume it always OK */
		mmc31xx_i2c_tx_data(data, 2);
		/* wait TM done for coming data read */
		msleep(MMC31XX_DELAY_TM);
		/* read xyz raw data */
		read_idx++;
		data[0] = MMC31XX_REG_DATA;
		if (mmc31xx_i2c_rx_data(data, 6) < 0) {
			return -EFAULT;
		}
		vec[0] = data[0] << 8 | data[1];
		vec[1] = data[2] << 8 | data[3];
		vec[2] = data[4] << 8 | data[5];
		pr_debug("[X - %04x] [Y - %04x] [Z - %04x]\n", 
			vec[0], vec[1], vec[2]);
		if (copy_to_user(pa, vec, sizeof(vec))) {
			return -EFAULT;
		}
		break;
	default:
		break;
	}

	return 0;
}

static ssize_t mmc31xx_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t ret = 0;

	sprintf(buf, "MMC31XX");
	ret = strlen(buf) + 1;

	return ret;
}

static DEVICE_ATTR(mmc31xx, S_IRUGO, mmc31xx_show, NULL);

static struct file_operations mmc31xx_fops = {
	.owner		= THIS_MODULE,
	.open		= mmc31xx_open,
	.release	= mmc31xx_release,
	.ioctl		= mmc31xx_ioctl,
};

static struct miscdevice mmc31xx_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = MMC31XX_DEV_NAME,
	.fops = &mmc31xx_fops,
};

static int mmc31xx_probe(struct platform_device *pdev) 
{
	unsigned char data[16] = {0};
	int res = 0;
	struct mmc31xx_device *client;
	NvU32 NumI2cConfigs;
	const NvU32 *pI2cConfigs;
	struct mmc31xx_platform_data *pdata;

	client = (struct mmc31xx_device*)kzalloc(sizeof(struct mmc31xx_device), GFP_KERNEL);
	if (client == NULL) {
		return -ENOMEM;
	}
	pdata = pdev->dev.platform_data;
	if (pdata == NULL) {
		pr_err(TAG "no platform_data specified\n");
		return -EFAULT;
	}
	client->i2c_instance = pdata->i2c_instance;
	client->i2c_address = pdata->i2c_address;
	
	NvOdmQueryPinMux(NvOdmIoModule_I2c, &pI2cConfigs, &NumI2cConfigs);
	if (client->i2c_instance >= NumI2cConfigs) {
		pr_err("mmc31xx_probe...NvOdmQueryPinMux failed \r\n");
		return -EINVAL;
	}
	if (pI2cConfigs[client->i2c_instance] == NvOdmI2cPinMap_Multiplexed) {
		pr_debug(TAG "i2c instance multiplexed\n");
		client->i2c = NvOdmI2cPinMuxOpen(NvOdmIoModule_I2c, client->i2c_instance, NvOdmI2cPinMap_Config2);
	} else {
		client->i2c = NvOdmI2cOpen(NvOdmIoModule_I2c, client->i2c_instance);
	}
	if (!client->i2c) {
		pr_err(TAG "can't open i2c\n");
		goto out_open_i2c;
	}
	this_client = client;

	res = misc_register(&mmc31xx_device);
	if (res) {
		pr_err(TAG "%s: mmc31xx_device register failed\n", __FUNCTION__);
		goto out;
	}
	res = device_create_file(&pdev->dev, &dev_attr_mmc31xx);
	if (res) {
		pr_err(TAG "%s: device_create_file failed\n", __FUNCTION__);
		goto out_deregister;
	}

	/* send ST cmd to mag sensor first of all */
	data[0] = MMC31XX_REG_CTRL;
	data[1] = MMC31XX_CTRL_SET;
	if (mmc31xx_i2c_tx_data(data, 2) < 0) {
		/* assume SET always success */
	}
	/* wait external capacitor charging done for next SET/RESET */
	msleep(MMC31XX_DELAY_SET);
	pr_debug(TAG "mmc31xx_probe successed\n");

	return 0;
	
out_deregister:
	misc_deregister(&mmc31xx_device);
out_open_i2c:
	kfree(client);
out:
	pr_debug(TAG "mmc31xx_probe failed\n");

	return res;
}

static int mmc31xx_remove(struct platform_device *pdev)
{
	NvOdmI2cClose(this_client->i2c);
	device_remove_file(&pdev->dev, &dev_attr_mmc31xx);
	misc_deregister(&mmc31xx_device);
	kfree(this_client);
	return 0;
}

static struct platform_driver mmc31xx_driver = {
	.probe = mmc31xx_probe, 
	.remove = mmc31xx_remove, 
	.driver = {
		.name = "mmc31xx", 
		.owner = THIS_MODULE,
	}, 
};

static int __init mmc31xx_init(void)
{
	pr_info("mmc31xx driver: init\n");
	return platform_driver_register(&mmc31xx_driver);
}

static void __exit mmc31xx_exit(void)
{
	pr_info("mmc31xx driver: exit\n");
	platform_driver_unregister(&mmc31xx_driver);
}

module_init(mmc31xx_init);
module_exit(mmc31xx_exit);

MODULE_AUTHOR("Robbie Cao<hjcao@memsic.com>");
MODULE_DESCRIPTION("MEMSIC MMC31XX Magnetic Sensor Driver");
MODULE_LICENSE("GPL");

