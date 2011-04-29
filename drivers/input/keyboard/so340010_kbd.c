/*
 * Filename    : android/kernel/driver/input/keyboard/so340010_gpio.c
 * Description : Button driver for SMBA1102. Four button is supported: HOME, 
 *					MENU, CANCEL, VOLUME_UP and VOLUME_DOWN
 * Athor       : LiuZheng <xmlz@malata.com>
 * Date        : 2010/06/28
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>

#include <nvodm_services.h>
#include <linux/so340010_kbd.h>

#define SO340010_I2C_TRY_COUNT			3

#define SO340010_TIMER_INTERVAL			2000

/*
 * TODO irq gpio number should be modify in SMBA1102 
 */
#define SO340010_IRQ_PORT				('v'-'a')
#define SO340010_IRQ_PIN				6
#define SO340010_GPIO_DEBOUNCE_TIME		10

#define SO340010_REG_GENERAL_CONFIG		0x0001
#define SO340010_REG_GPIO_STATE			0x0108
#define SO340010_REG_BUTTON_STATE		0x0109
#define SO340010_REG_NUM				74

#if (__SO340010_GENERIC_DEBUG__)
static ssize_t so340010_read_sysfs_debug(struct device *device, struct device_attribute *attr, char *buffer);
static ssize_t so340010_read_sysfs_intr(struct device *device, struct device_attribute *attr, char *buffer);
static ssize_t so340010_read_sysfs_i2c_snag(struct device *device, struct device_attribute *attr, char *buffer);
static ssize_t so340010_read_sysfs_reset(struct device *device, struct device_attribute *attr, char *buffer);
static ssize_t so340010_write_sysfs_reset(struct device *device, struct device_attribute *attr, const char *buffer, size_t count);
static ssize_t so340010_read_sysfs_pending_mask(struct device *device, struct device_attribute *attr, char *buffer);
#endif


struct so340010_kbd_dev {
	/* i2c */
	unsigned int i2c_instance;
	unsigned int i2c_address;
	unsigned int i2c_speed;
	unsigned int i2c_timeout;
	NvOdmServicesI2cHandle i2c;

	/* irq */
	unsigned int irq_port;
	unsigned int irq_pin;
	NvOdmServicesGpioHandle gpio_handle;
	NvOdmGpioPinHandle irq_pin_handle;
	NvOdmServicesGpioIntrHandle irq_handle;

	/* input dev */
	struct input_dev *input_dev;

	/* pending button */
	unsigned short pending_keys;


	/* early suspend */
	struct early_suspend	early_suspend; 

	/* work */
	struct work_struct work;

	/* workqueue */
	struct workqueue_struct *workqueue;

	/* timer */
	struct timer_list timer;

	int last_read;

#if (__I2C_SNAG_DETECTED__)
	unsigned int i2c_snag_no_slave;
	unsigned int i2c_snag_read_failed;
#endif

#if (__SO340010_GENERIC_DEBUG__)
	int last_i2c_error;
	int last_reset_error;
#endif

};

struct so340010_kbd_info {
	unsigned int 	key_mask;
	int 			key_code; 
};

static struct so340010_kbd_info key_table[] = {
#if defined(CONFIG_7373C_V20)
	{ 0x0008, KEY_SEARCH },
	{ 0x0004, KEY_MENU },
	{ 0x0002, KEY_BACK },
	{ 0x0001, KEY_HOME },
#else
	{ 0x0008, KEY_BACK },
	{ 0x0004, KEY_MENU },
	{ 0x0002, KEY_HOME },
	{ 0x0001, KEY_SEARCH },
#endif
};

static int key_num = sizeof(key_table)/sizeof(key_table[0]);

struct so340010_register {
	unsigned short address;
	unsigned char value[2];
};

static struct so340010_register so340010_register_init_table[] = {
#if defined(CONFIG_7373C_V20)
	{ 0x0000, { 0x00, 0x07 } },
	{ 0x0001, { 0x00, 0x20 } },
	{ 0x0004, { 0x00, 0x0F } },
//  { 0x000E, { 0x01, 0x00 } },
	{ 0x0010, { 0xA0, 0xA0 } },
	{ 0x0011, { 0x00, 0xA0 } },
#else
	{ 0x0000, { 0x00, 0x07 } },
	{ 0x0001, { 0x00, 0x20 } },
	{ 0x0004, { 0x00, 0x0F } },
//	{ 0x000E, { 0x01, 0x00 } },
	{ 0x0010, { 0xA0, 0xA0 } },
	{ 0x0011, { 0xA0, 0xA0 } },
#endif
};

#if (__SO340010_GENERIC_DEBUG__)
static DEVICE_ATTR(debug, 0777, so340010_read_sysfs_debug, NULL);
static DEVICE_ATTR(intr, 0777, so340010_read_sysfs_intr, NULL);
static DEVICE_ATTR(i2c_snag, 0777, so340010_read_sysfs_i2c_snag, NULL);
static DEVICE_ATTR(reset, 0777, so340010_read_sysfs_reset, so340010_write_sysfs_reset);
static DEVICE_ATTR(pending_mask, 0777, so340010_read_sysfs_pending_mask, NULL);
#endif

static int so340010_i2c_write(struct so340010_kbd_dev *dev, unsigned short reg_start, unsigned char *buffer, unsigned int write_num)
{
	int i;
	unsigned char *write_buffer;
	NvOdmI2cStatus I2cStatus;
	NvOdmI2cTransactionInfo TransactionInfo;

	write_buffer = kmalloc(write_num + 2, GFP_KERNEL);
	if (write_buffer == NULL) {
		logd(TAG "so340010_i2c_write kmalloc nomem");
		return -ENOMEM;
	}

	write_buffer[0] = reg_start >> 8;
	write_buffer[1] = reg_start & 0xFF;
	for (i = 0; i < write_num; i++) {
		write_buffer[i+2] = buffer[i];
	}
	
	TransactionInfo.Flags = NVODM_I2C_IS_WRITE;
	TransactionInfo.Address = dev->i2c_address;
	TransactionInfo.Buf = write_buffer;
	TransactionInfo.NumBytes = write_num + 2;
	I2cStatus = NvOdmI2cTransaction(dev->i2c, &TransactionInfo, 
			1, dev->i2c_speed, dev->i2c_timeout);

	if (I2cStatus != NvOdmI2cStatus_Success) {
		logd(TAG "so340010_i2c_write failed(%d)\r\n", I2cStatus);
		kfree(write_buffer);
		return -EINVAL;
	}
	kfree(write_buffer);
	return 0;
}

static int so340010_i2c_read(struct so340010_kbd_dev *dev, unsigned short reg_start, unsigned char *buffer, unsigned int read_num)
{
	int i;
	unsigned char reg_buffer[2];
	NvOdmI2cStatus I2cStatus;
	NvOdmI2cTransactionInfo TransactionInfo[2];

	reg_buffer[0] = reg_start >> 8;
	reg_buffer[1] = reg_start & 0xFF;

	TransactionInfo[0].Flags = NVODM_I2C_IS_WRITE | NVODM_I2C_USE_REPEATED_START;
	TransactionInfo[0].Address = dev->i2c_address;
	TransactionInfo[0].Buf = reg_buffer;
	TransactionInfo[0].NumBytes = 2;
	TransactionInfo[1].Flags = 0;
	TransactionInfo[1].Address = dev->i2c_address;
	TransactionInfo[1].Buf = buffer;
	TransactionInfo[1].NumBytes = read_num;

	for (i = 0; i < SO340010_I2C_TRY_COUNT; i++) {
		I2cStatus = NvOdmI2cTransaction(dev->i2c, TransactionInfo,
				2, dev->i2c_speed, dev->i2c_timeout);
	}

	if (I2cStatus != NvOdmI2cStatus_Success) {
		logd(TAG "i2c_read failed(%d)\r\n", I2cStatus);
		return I2cStatus;
	}
	return 0;
}

static int so340010_i2c_write_word(struct so340010_kbd_dev *dev, unsigned short reg, unsigned short data) 
{
	unsigned char data_buffer[2];

	data_buffer[0] = data >> 8;
	data_buffer[1] = data & 0xFF;
	return so340010_i2c_write(dev, reg, data_buffer, 2);
}

static int so340010_i2c_read_word(struct so340010_kbd_dev *dev, unsigned short reg, unsigned short *data) 
{
	int ret;
	unsigned char buffer[2];
	ret = so340010_i2c_read(dev, reg, buffer, 2);
	if (ret) return ret;
	*data = (buffer[0] << 8) | buffer[1];
	return 0;
}

#if (__SO340010_GENERIC_DEBUG__)
static void dump(struct so340010_kbd_dev *dev)
{
	int i;
	unsigned char buffer[SO340010_REG_NUM];

	if (so340010_i2c_read(dev, 0, buffer, SO340010_REG_NUM)) {
		logd( TAG "dump() failed\r\n");
		return ;
	}

	for (i = 0; i < SO340010_REG_NUM/2; i++) {
		logd(TAG "0x%08x = 0x%04x  0x%04x\r\n", i, buffer[i*2], buffer[i*2+1]);
	}
}
#endif

#if (__SO340010_GENERIC_DEBUG__)
static ssize_t so340010_read_sysfs_debug(struct device *device, struct device_attribute *attr, char *buffer)
{
	int i, ret;
	unsigned char read_buffer[SO340010_REG_NUM] = {0};
	char *cursor;
	struct so340010_kbd_dev *dev;

	dev = (struct so340010_kbd_dev *)dev_get_drvdata(device);

	ret = so340010_i2c_read(dev, 0, read_buffer, SO340010_REG_NUM);
	if (ret) {
		return sprintf(buffer, "i2c read failed(%d)", ret);
	}

	cursor = buffer;
	for (i = 0; i < SO340010_REG_NUM; i++) {
		cursor += sprintf(cursor, "R0x%04x=0x%04x \r\n", i, read_buffer[i]);
	}

	return cursor - buffer;
}

static ssize_t so340010_read_sysfs_intr(struct device *device, struct device_attribute *attr, char *buffer)
{
	int pin_state;
	struct so340010_kbd_dev *dev;

	dev = (struct so340010_kbd_dev *)dev_get_drvdata(device);
	NvOdmGpioGetState(dev->gpio_handle, dev->irq_pin_handle, &pin_state);
	return sprintf(buffer, pin_state ? "1" : "0");
}

static ssize_t so340010_read_sysfs_i2c_snag(struct device *device, struct device_attribute *attr, char *buffer)
{
	struct so340010_kbd_dev *dev;
	dev = (struct so340010_kbd_dev *)dev_get_drvdata(device);
	return sprintf(buffer, "%d", dev->last_i2c_error);
}

static int so340010_reset(struct so340010_kbd_dev *dev);
static ssize_t so340010_write_sysfs_reset(struct device *device, struct device_attribute *attr, const char *buffer, size_t count) 
{
	struct so340010_kbd_dev *dev;
	dev = (struct so340010_kbd_dev *)dev_get_drvdata(device);
	dev->last_reset_error = so340010_reset(dev);
	return count;
}

static ssize_t so340010_read_sysfs_reset(struct device *device, struct device_attribute *attr, char *buffer)
{
	struct so340010_kbd_dev *dev;
	dev = (struct so340010_kbd_dev *)dev_get_drvdata(device);
	return sprintf(buffer, "%s", dev->last_reset_error ? "failed" : "success");
}

static ssize_t so340010_read_sysfs_pending_mask(struct device *device, struct device_attribute *attr, char *buffer)
{
	struct so340010_kbd_dev *dev;
	dev = (struct so340010_kbd_dev *)dev_get_drvdata(device);
	return sprintf(buffer, "0x%04d", dev->pending_keys);
}

#endif

static int so340010_reset(struct so340010_kbd_dev *dev)
{
	int i;
	unsigned short reg_val;

	for (i = 0; i < sizeof(so340010_register_init_table)/sizeof(so340010_register_init_table[0]); i++) {
		if (so340010_i2c_write(dev, so340010_register_init_table[i].address, 
				so340010_register_init_table[i].value, 2)) {
			goto failed;
		}
	}
	if (so340010_i2c_read_word(dev, SO340010_REG_GPIO_STATE, &reg_val)
		|| so340010_i2c_read_word(dev, SO340010_REG_BUTTON_STATE, &reg_val)) {
		goto failed;
	}
	dev->pending_keys = 0;
	dev->last_read = jiffies_to_msecs(jiffies);
	return 0;
failed:
	return -EINVAL;
}

static int so340010_sleep(struct so340010_kbd_dev *dev, bool sleep) 
{
#define SO340010_SLEEP		((unsigned short)(0x0020))
#define SO340010_AWAKE		((unsigned short)(0x00A0))
	if (sleep) {
		return so340010_i2c_write_word(dev, SO340010_REG_GENERAL_CONFIG, SO340010_SLEEP);
	} else {
		return so340010_i2c_write_word(dev, SO340010_REG_GENERAL_CONFIG, SO340010_AWAKE);
	}
}

static void so340010_timer_func(unsigned long __dev) 
{
	int pin_state;
	struct so340010_kbd_dev *dev;
	
	dev = (struct so340010_kbd_dev *)__dev;

	NvOdmGpioGetState(dev->gpio_handle, dev->irq_pin_handle, &pin_state);

	if (pin_state == 0) { 
		if ((jiffies_to_msecs(jiffies) - dev->last_read) >= 2000) {
			queue_work(dev->workqueue, &dev->work);
		}
	}
	mod_timer(&dev->timer, jiffies + msecs_to_jiffies(SO340010_TIMER_INTERVAL));
}

static void so340010_work_func(struct work_struct *work)
{
	int i, ret;
	unsigned int gpio_val, button_val;
	struct so340010_kbd_dev *dev;

	dev = (struct so340010_kbd_dev *)container_of(work, struct so340010_kbd_dev, work);
	
	if ((ret = so340010_i2c_read_word(dev, SO340010_REG_GPIO_STATE, &gpio_val) != 0)
		|| (ret = so340010_i2c_read_word(dev, SO340010_REG_BUTTON_STATE, &button_val) != 0)) {	
		goto i2c_snag;
	}
	
	logd(TAG "gpio_val=0x%04x, button_val = 0x%04x\r\n", gpio_val, button_val);
	
	for (i = 0; i < key_num; i++) {
		if (button_val & key_table[i].key_mask) {
			dev->pending_keys |= key_table[i].key_mask;
			input_report_key(dev->input_dev, key_table[i].key_code, 1);
		} else {
			if (dev->pending_keys & key_table[i].key_mask) {
				input_report_key(dev->input_dev, key_table[i].key_code, 0);
				dev->pending_keys &= ~(key_table[i].key_mask);
			}
		}
	}

	dev->last_read = jiffies_to_msecs(jiffies);
	return;

i2c_snag:
#if (__SO340010_GENERIC_DEBUG__)
	dev->last_i2c_error = ret;
#endif
	switch (ret) {
		case NvOdmI2cStatus_SlaveNotFound:
		case NvOdmI2cStatus_Timeout:
		case NvOdmI2cStatus_ReadFailed:
			so340010_reset(dev);
			break;
	}
}

static void so340010_irq_callback(void *args)
{
	struct so340010_kbd_dev *dev;
	dev = (struct so340010_kbd_dev*)args;
	queue_work(dev->workqueue, &dev->work);
	NvOdmGpioInterruptDone(dev->irq_handle);
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void so340010_kbd_early_suspend(struct early_suspend *es)
{
	struct so340010_kbd_dev *dev;

	logd(TAG "so340010_kbd_early_suspend() IN\r\n");
	
	dev = (struct so340010_kbd_dev *)container_of(es, struct so340010_kbd_dev, early_suspend);
	NvOdmGpioInterruptMask(dev->irq_handle, NV_TRUE);
	cancel_work_sync(&dev->work);
	so340010_sleep(dev, true);
	
	logd(TAG "so340010_kbd_early_suspend() OUT\r\n");
}

static void so340010_kbd_late_resume(struct early_suspend *es)
{
	struct so340010_kbd_dev *dev;

	dev = (struct so340010_kbd_dev *)container_of(es, struct so340010_kbd_dev, early_suspend);
	dev->pending_keys = 0;
	so340010_sleep(dev, false);
	if (so340010_reset(dev)) {
		logd(TAG "so340010_reset_failed\r\n");
	}
	NvOdmGpioInterruptMask(dev->irq_handle, NV_FALSE);
}
#endif

static int so340010_kbd_probe(struct platform_device *pdev) 
{
	int i;
	struct so340010_kbd_dev *dev;
	NvU32 NumI2cConfigs;
	struct so340010_kbd_platform_data *pdata;
	const NvU32 *pI2cConfigs;

	logd(TAG "so340010_kbd_probe\r\n");

	dev = kzalloc(sizeof(struct so340010_kbd_dev), GFP_KERNEL);
	if (!dev) {
		logd(TAG "so340010_kbd_probe kmalloc fail \r\n");
		goto failed_alloc_dev;
	}
	platform_set_drvdata(pdev, dev);

	pdata = pdev->dev.platform_data;
	if(pdata==NULL){
		printk(TAG "platform_get_drvdata == NULL \n");
	}

	/* open i2c */
	dev->i2c_instance = pdata->i2c_instance;
	dev->i2c_address = pdata->i2c_address;
	dev->i2c_speed = pdata->i2c_speed;
	dev->i2c_timeout = pdata->i2c_timeout;

	NvOdmQueryPinMux(NvOdmIoModule_I2c, &pI2cConfigs, &NumI2cConfigs);
	if (dev->i2c_instance >= NumI2cConfigs) {
	    printk(TAG "NvOdmQueryPinMux failed \r\n");
	    return -EINVAL;
	}

	if (pI2cConfigs[dev->i2c_instance] == NvOdmI2cPinMap_Multiplexed) {
	    logd(TAG "i2c config multiplexed\n");
	    dev->i2c = NvOdmI2cPinMuxOpen(NvOdmIoModule_I2c, dev->i2c_instance, NvOdmI2cPinMap_Config2);
	} else {
	    dev->i2c = NvOdmI2cOpen(NvOdmIoModule_I2c, dev->i2c_instance);
	}

	if (!dev->i2c) {
		logd(TAG "so340010_kbd_probe NvOdmI2cOpen fail \r\n");
		goto failed_open_i2c;
	}

	/* open gpio */
	dev->irq_port = SO340010_IRQ_PORT;
	dev->irq_pin = SO340010_IRQ_PIN;
	dev->gpio_handle = NvOdmGpioOpen();
	if (!dev->gpio_handle) {
		logd(TAG "so340010_kbd_probe NvOdmGpioOpen fail \r\n");
		goto failed_open_gpio;
	}
	dev->irq_pin_handle = NvOdmGpioAcquirePinHandle(dev->gpio_handle, dev->irq_port, dev->irq_pin);
	if (!dev->irq_pin_handle) {
		logd(TAG "so340010_kbd_probe NvOdmGpioAcquirePinHandle fail \r\n");
		goto failed_acquire_pin;
	}
	NvOdmGpioConfig(dev->gpio_handle, dev->irq_pin_handle, NvOdmGpioPinMode_InputData);

	/* register input device */
	dev->input_dev = input_allocate_device();
	if (!dev->input_dev) {
		logd(TAG "so340010_kbd_probe input_allocate_device fail \r\n");
		goto failed_alloc_input;
	}
	dev->input_dev->name = "so340010_kbd";
	set_bit(EV_KEY, dev->input_dev->evbit);
	for (i = 0; i < key_num; i++) {
		set_bit(key_table[i].key_code, dev->input_dev->keybit);
	}
	if (input_register_device(dev->input_dev)) {
		goto failed_register_input;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	dev->early_suspend.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING;
	dev->early_suspend.suspend = so340010_kbd_early_suspend;
	dev->early_suspend.resume = so340010_kbd_late_resume;
	register_early_suspend(&dev->early_suspend);
#endif

	dev->workqueue = create_singlethread_workqueue("so340010_kbc");
	if (!dev->workqueue) {
		goto failed_create_workqueue;
	}

	INIT_WORK(&dev->work, so340010_work_func);

	if ((NvOdmGpioInterruptRegister(dev->gpio_handle, &dev->irq_handle, dev->irq_pin_handle, 
			NvOdmGpioPinMode_InputInterruptFallingEdge, so340010_irq_callback, 
			dev, SO340010_GPIO_DEBOUNCE_TIME) == NV_FALSE) || (dev->irq_handle == NULL)) {
		logd(TAG "so340010_kbd_probe NvOdmGpioInterruptRegister fail \r\n");
		goto failed_enable_irq;
	}

	if (so340010_reset(dev)) {
		logd(TAG "so340010_kbd_probe so340010_reset fail \r\n");
		goto failed_reset_hardware;
	}

#if (__SO340010_GENERIC_DEBUG__)
	if (device_create_file(&pdev->dev, &dev_attr_debug)
		|| device_create_file(&pdev->dev, &dev_attr_intr)
		|| device_create_file(&pdev->dev, &dev_attr_i2c_snag)
		|| device_create_file(&pdev->dev, &dev_attr_reset)
		|| device_create_file(&pdev->dev, &dev_attr_pending_mask)) {
		goto failed_add_sysfs;
	}
#endif

	init_timer(&dev->timer);
	dev->timer.function = so340010_timer_func;
	dev->timer.data = dev;
	dev->last_read = jiffies_to_msecs(jiffies);
	mod_timer(&dev->timer, jiffies + msecs_to_jiffies(SO340010_TIMER_INTERVAL));

	return 0;
#if (__SO340010_GENERIC_DEBUG__)
failed_add_sysfs:
#endif
failed_reset_hardware:
	NvOdmGpioInterruptUnregister(dev->gpio_handle, dev->irq_pin_handle, dev->irq_handle);
failed_enable_irq:
	input_unregister_device(dev->input_dev);
	unregister_early_suspend(&dev->early_suspend);
failed_register_input:
	destroy_workqueue(dev->workqueue);
failed_create_workqueue:
	input_free_device(dev->input_dev);	
failed_alloc_input:
	NvOdmGpioReleasePinHandle(dev->gpio_handle, dev->irq_pin_handle);
failed_acquire_pin:
	NvOdmGpioClose(dev->gpio_handle);
failed_open_gpio:
	NvOdmI2cClose(dev->i2c);
failed_open_i2c:
	kfree(dev);
failed_alloc_dev:
	logd(TAG "so34001_kbd_probe failed\r\n");
	return -1;
}

static int so340010_kbd_remove(struct platform_device *pdev)
{
	struct so340010_kbd_dev *dev;

	dev = (struct so340010_kbd_dev *)platform_get_drvdata(pdev);
#if (__SO340010_GENERIC_DEBUG__)
	device_remove_file(&pdev->dev, &dev_attr_debug);
	device_remove_file(&pdev->dev, &dev_attr_intr);
	device_remove_file(&pdev->dev, &dev_attr_i2c_snag);
	device_remove_file(&pdev->dev, &dev_attr_reset);
	device_remove_file(&pdev->dev, &dev_attr_pending_mask);
#endif
	NvOdmGpioInterruptUnregister(dev->gpio_handle, dev->irq_pin_handle, dev->irq_handle);
	input_unregister_device(dev->input_dev);
	unregister_early_suspend(&dev->early_suspend);
	destroy_workqueue(dev->workqueue);
	input_free_device(dev->input_dev);
	NvOdmGpioReleasePinHandle(dev->gpio_handle, dev->irq_pin_handle);
	NvOdmGpioClose(dev->gpio_handle);
	NvOdmI2cClose(dev->i2c);
	kfree(dev);
	return 0;
}

struct platform_driver so340010_kbd_driver = {
	.driver = {
		.name = "so340010_kbd",
		.owner = THIS_MODULE,
	},
	.probe = so340010_kbd_probe,
	.remove = so340010_kbd_remove,
};

static int __init so340010_kbd_init(void)
{
	logd(TAG "so340010_kbd_init\r\n");
	return platform_driver_register(&so340010_kbd_driver);
}

static void __exit so340010_kbd_exit(void) 
{
	platform_driver_unregister(&so340010_kbd_driver);
}

module_init(so340010_kbd_init);
module_exit(so340010_kbd_exit);
