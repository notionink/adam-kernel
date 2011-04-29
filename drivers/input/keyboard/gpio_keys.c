/*
 * Driver for keys on GPIO lines capable of generating interrupts.
 *
 * Copyright 2005 Phil Blundell
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/gpio_keys.h>
#include <linux/workqueue.h>
#include <linux/kernel.h>
#include <linux/wakelock.h>

#include <asm/gpio.h>

struct gpio_button_data {
	struct gpio_keys_button *button;
	struct input_dev *input;
	struct timer_list timer;
	struct work_struct work;
};

struct gpio_keys_drvdata {
	struct input_dev *input;
	struct gpio_button_data data[0];
};

static unsigned int f4_send_state = 0;
struct wake_lock f4_wake_lock;

//*****************************************************************************************//
//Set for recovery-key press driver interface
int RecoveryKeyValue = 0;
static ssize_t tegra_touch_show_recoverykey(struct device *dev, struct device_attribute *attr, char *buf)
{
	//printk("==tegra_touch_show_recoverykey begin ! == \n");	
	struct gpio_keys_platform_data *pdata = (struct gpio_keys_platform_data *)dev_get_drvdata(dev);

	if(1 == RecoveryKeyValue){
		return sprintf(buf, "recoverykey=%d\n", RecoveryKeyValue);
	}else{
		return sprintf(buf, "%d\n", RecoveryKeyValue);
	}
}

//set Version interface
static DEVICE_ATTR(recoverykey, S_IRWXUGO, tegra_touch_show_recoverykey, NULL);

//*****************************************************************************************//

static void gpio_keys_report_event(struct work_struct *work)
{
	struct gpio_button_data *bdata =
		container_of(work, struct gpio_button_data, work);
	struct gpio_keys_button *button = bdata->button;
	struct input_dev *input = bdata->input;
	unsigned int type = button->type ?: EV_KEY;
	int state = (gpio_get_value(button->gpio) ? 1 : 0) ^ button->active_low;
	
	if(button->code!=KEY_F4) {
		input_event(input, type, button->code, !!state);
	} else {
	
	#if 1
		if(f4_send_state == 1) { /* Special state for just Wake-Up from LP1 */
            printk("Send a simulate KEY_F4 **** \n");
            input_event(input, type, button->code, !!state);
            input_sync(input);
            mdelay(80);
            input_event(input, type, button->code, !!!state);
            input_sync(input);

            f4_send_state = 0;
                                                
            mdelay(300);

            input_event(input, type, button->code, !!state);
            input_sync(input);
            mdelay(80);
            input_event(input, type, button->code, !!!state);
            input_sync(input);

            input_event(input, type, KEY_BACK, !!state);
            input_sync(input);
            mdelay(80);
            input_event(input, type, KEY_BACK, !!!state);
        } else if (f4_send_state == 2) {
                f4_send_state = 0;

                input_event(input, type, KEY_BACK, !!state);
                input_sync(input);
                mdelay(80);
                input_event(input, type, KEY_BACK, !!!state);
        } else if (f4_send_state == 3) { 
                f4_send_state = 0;
                input_event(input, type, button->code, !!!state);
        } else {        /* Normal state */
                input_event(input, type, button->code, !!state);
        }
	}
	#else
	if(state)
	{

	input_event(input, type, button->code, 0);
	int delay=20;
	while(delay--)input_event(input, type, button->code, !!state);
	}
	#endif
	
	input_sync(input);
}

static void gpio_keys_timer(unsigned long _data)
{
	struct gpio_button_data *data = (struct gpio_button_data *)_data;

	schedule_work(&data->work);
}

struct gpio_button_data *bdata_F4 =NULL;

void F4_Deal(unsigned int wake_up_flag)
{
	if(bdata_F4 !=NULL)
	{
		struct gpio_button_data *bdata = bdata_F4;
		struct gpio_keys_button *button = bdata->button;

                f4_send_state = wake_up_flag;

                if(wake_up_flag == 1) {
                        wake_lock_timeout(&f4_wake_lock, (HZ * 5));
                }

		if (button->debounce_interval)
			mod_timer(&bdata->timer,
				jiffies + msecs_to_jiffies(button->debounce_interval));
		else
			schedule_work(&bdata->work);
	}
}

static irqreturn_t gpio_keys_isr(int irq, void *dev_id)
{
	struct gpio_button_data *bdata = dev_id;
	struct gpio_keys_button *button = bdata->button;

	BUG_ON(irq != gpio_to_irq(button->gpio));

	if (button->debounce_interval)
		mod_timer(&bdata->timer,
			jiffies + msecs_to_jiffies(button->debounce_interval));
	else
		schedule_work(&bdata->work);

	return IRQ_HANDLED;
}

extern void Tps6586x_Resume_Isr_Register(void); 

static int __devinit gpio_keys_probe(struct platform_device *pdev)
{
	struct gpio_keys_platform_data *pdata = pdev->dev.platform_data;
	struct gpio_keys_drvdata *ddata;
	struct input_dev *input;
	int i, error;
	int wakeup = 0;

	int state = 0;
	int err;

	ddata = kzalloc(sizeof(struct gpio_keys_drvdata) +
			pdata->nbuttons * sizeof(struct gpio_button_data),
			GFP_KERNEL);
	input = input_allocate_device();
	if (!ddata || !input) {
		error = -ENOMEM;
		goto fail1;
	}

	platform_set_drvdata(pdev, ddata);

	input->name = pdev->name;
	input->phys = "gpio-keys/input0";
	input->dev.parent = &pdev->dev;

	input->id.bustype = BUS_HOST;
	input->id.vendor = 0x0001;
	input->id.product = 0x0001;
	input->id.version = 0x0100;

	/* Enable auto repeat feature of Linux input subsystem */
	if (pdata->rep)
		__set_bit(EV_REP, input->evbit);

	ddata->input = input;

	for (i = 0; i < pdata->nbuttons; i++) {
		struct gpio_keys_button *button = &pdata->buttons[i];
		struct gpio_button_data *bdata = &ddata->data[i];
		int irq;
		unsigned int type = button->type ?: EV_KEY;

		bdata->input = input;
		bdata->button = button;
		setup_timer(&bdata->timer,
			    gpio_keys_timer, (unsigned long)bdata);
		INIT_WORK(&bdata->work, gpio_keys_report_event);

		
		printk("gpio_keys_report_event=%d\n",button->code);
		if(button->code==KEY_F4)
		{
			printk("gpio_keys_probe KEY_F4=%d\n",KEY_F4);
			bdata_F4 = bdata;
                        wake_lock_init(&f4_wake_lock, WAKE_LOCK_SUSPEND, "gpio_f4_wake");
			//Tps6586x_Resume_Isr_Register();
		}
		else
		{
		error = gpio_request(button->gpio, button->desc ?: "gpio_keys");
		if (error < 0) {
			pr_err("gpio-keys: failed to request GPIO %d,"
				" error %d\n", button->gpio, error);
			goto fail2;
		}

		error = gpio_direction_input(button->gpio);
		if (error < 0) {
			pr_err("gpio-keys: failed to configure input"
				" direction for GPIO %d, error %d\n",
				button->gpio, error);
			gpio_free(button->gpio);
			goto fail2;
		}

		irq = gpio_to_irq(button->gpio);
		if (irq < 0) {
			error = irq;
			pr_err("gpio-keys: Unable to get irq number"
				" for GPIO %d, error %d\n",
				button->gpio, error);
			gpio_free(button->gpio);
			goto fail2;
		}
		error = request_irq(irq, gpio_keys_isr,
				    IRQF_SHARED |
				    IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				    button->desc ? button->desc : "gpio_keys",
				    bdata);
		}

		if (error) {
			pr_err("gpio-keys: Unable to claim irq %d; error %d\n",
				irq, error);
			gpio_free(button->gpio);
			goto fail2;
		}

		if (button->wakeup)
			wakeup = 1;
		
		if(button->code==KEY_VOLUMEUP){
			//printk("gpio_keys_probe KEY_VOLUMEUP=%d\n",KEY_VOLUMEUP);
			state = (gpio_get_value(button->gpio) ? 1 : 0) ^ button->active_low;
			//printk("gpio_keys_probe KEY_VOLUMEUP state is %d \n", !!state);
			if(0 != (!!state)){
				RecoveryKeyValue = 1;
			}else{
				RecoveryKeyValue = 0;
			}
		}

		input_set_capability(input, type, button->code);
	}

	error = input_register_device(input);
	if (error) {
		pr_err("gpio-keys: Unable to register input device, "
			"error: %d\n", error);
		goto fail2;
	}

	//create recoverykey file,  path  ->   /sys/devices/platform/gpio-keys.3/recoverykey
	err = device_create_file(&pdev->dev, &dev_attr_recoverykey);
	if (err) {
		pr_err("tegra_touch_probe : device_create_file recoverykey failed\n");
		goto fail2;
	}
	
	device_init_wakeup(&pdev->dev, wakeup);

	return 0;

 fail2:
	while (--i >= 0) {
		free_irq(gpio_to_irq(pdata->buttons[i].gpio), &ddata->data[i]);
		if (pdata->buttons[i].debounce_interval)
			del_timer_sync(&ddata->data[i].timer);
		cancel_work_sync(&ddata->data[i].work);
		gpio_free(pdata->buttons[i].gpio);
	}

	platform_set_drvdata(pdev, NULL);
 fail1:
	input_free_device(input);
	kfree(ddata);

	return error;
}

static int __devexit gpio_keys_remove(struct platform_device *pdev)
{
	struct gpio_keys_platform_data *pdata = pdev->dev.platform_data;
	struct gpio_keys_drvdata *ddata = platform_get_drvdata(pdev);
	struct input_dev *input = ddata->input;
	int i;

	device_init_wakeup(&pdev->dev, 0);

	for (i = 0; i < pdata->nbuttons; i++) {
		int irq = gpio_to_irq(pdata->buttons[i].gpio);
		free_irq(irq, &ddata->data[i]);
		if (pdata->buttons[i].debounce_interval)
			del_timer_sync(&ddata->data[i].timer);
		cancel_work_sync(&ddata->data[i].work);
		gpio_free(pdata->buttons[i].gpio);
	}

	input_unregister_device(input);

        wake_lock_destroy(&f4_wake_lock);
        
	return 0;
}


#ifdef CONFIG_PM
static int gpio_keys_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct gpio_keys_platform_data *pdata = pdev->dev.platform_data;
	int i;

	if (device_may_wakeup(&pdev->dev)) {
		for (i = 0; i < pdata->nbuttons; i++) {
			struct gpio_keys_button *button = &pdata->buttons[i];
			if (button->wakeup) {
				int irq = gpio_to_irq(button->gpio);
				enable_irq_wake(irq);
			}
		}
	}

	return 0;
}

static int gpio_keys_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct gpio_keys_platform_data *pdata = pdev->dev.platform_data;
	int i;

        flush_scheduled_work();

	if (device_may_wakeup(&pdev->dev)) {
		for (i = 0; i < pdata->nbuttons; i++) {
			struct gpio_keys_button *button = &pdata->buttons[i];
			if (button->wakeup) {
				int irq = gpio_to_irq(button->gpio);
				disable_irq_wake(irq);
			}
		}
	}

        /* F4_Deal */
        F4_Deal(2);

        flush_scheduled_work();

	return 0;
}

static const struct dev_pm_ops gpio_keys_pm_ops = {
	.suspend	= gpio_keys_suspend,
	.resume		= gpio_keys_resume,
};
#endif

static struct platform_driver gpio_keys_device_driver = {
	.probe		= gpio_keys_probe,
	.remove		= __devexit_p(gpio_keys_remove),
	.driver		= {
		.name	= "gpio-keys",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &gpio_keys_pm_ops,
#endif
	}
};

static int __init gpio_keys_init(void)
{
	return platform_driver_register(&gpio_keys_device_driver);
}

static void __exit gpio_keys_exit(void)
{
	platform_driver_unregister(&gpio_keys_device_driver);
}

module_init(gpio_keys_init);
module_exit(gpio_keys_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Phil Blundell <pb@handhelds.org>");
MODULE_DESCRIPTION("Keyboard driver for CPU GPIOs");
MODULE_ALIAS("platform:gpio-keys");
