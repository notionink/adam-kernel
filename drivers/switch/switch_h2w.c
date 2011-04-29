/*
 *  driver/switch/switch_h2w.c
 * 
 * Copyright (C) 2010 Malta, Corp. 
 * Athor: LiuZheng <xmlz@malata.com>  
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/switch.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <nvodm_services.h>
#include <linux/switch_h2w.h>

#define TAG					"h2w: "
#define IRQ_DEBOUNCE		20
#define H2W_PRINT_NAME		"headphone"

struct h2w_switch_dev {
	struct switch_dev sdev;

	int		hp_det_active_low;
	int		have_dock_hp;
	int		dock_hp_det_active_low;

	NvOdmServicesGpioHandle gpio;
	NvOdmGpioPinHandle hp_det_pin;
	NvOdmServicesGpioIntrHandle hp_det_irq;
	NvOdmGpioPinHandle dock_hp_det_pin;
	NvOdmServicesGpioIntrHandle dock_hp_det_irq;

	struct workqueue_struct *workqueue;
	struct work_struct work;
};

static void h2w_switch_work(struct work_struct *work)
{
	int state, state2;
	struct h2w_switch_dev *hsdev;

	hsdev = container_of(work, struct h2w_switch_dev, work);
	NvOdmGpioGetState(hsdev->gpio, hsdev->hp_det_pin, &state);
	state = hsdev->hp_det_active_low ? !state : state;
	if (hsdev->have_dock_hp) {
		NvOdmGpioGetState(hsdev->gpio, hsdev->dock_hp_det_pin, &state2);
		state2 = hsdev->dock_hp_det_active_low ? !state2 : state2;
	} else {
		state2 = 0;
	}
	pr_debug(TAG "hp state %s\n", state ? "on" : "off");
	pr_debug(TAG "dock hp state %s\n", state2 ? "on" : "off");
	switch_set_state(&hsdev->sdev, ((state==1) || (state2==1)));
}

static void h2w_switch_irq_isr(void *args)
{
	struct h2w_switch_dev *hsdev;
	pr_debug(TAG "h2w_switch_irq_isr\n");
	hsdev = (struct h2w_switch_dev *)args;
	queue_work(hsdev->workqueue, &hsdev->work);
	NvOdmGpioInterruptDone(hsdev->hp_det_irq);
}

static void dock_h2w_switch_irq_isr(void *args)
{
	struct h2w_switch_dev *hsdev;
	pr_debug("dock_h2w_switch_irq_isr\r\n");
	hsdev = (struct h2w_switch_dev *)args;
	queue_work(hsdev->workqueue, &hsdev->work);
	NvOdmGpioInterruptDone(hsdev->dock_hp_det_irq);
}

static ssize_t h2w_switch_print_name(struct switch_dev *sdev, char *buf)
{
	return sprintf(buf, H2W_PRINT_NAME);
}

static ssize_t h2w_switch_print_state(struct switch_dev *sdev, char *buf)
{
	return sprintf(buf, (switch_get_state(sdev)) ? "2" : "0");
}

static int h2w_switch_probe(struct platform_device *pdev)
{
	struct h2w_switch_dev *hsdev;
	struct switch_h2w_platform_data *pdata;

	pr_debug(TAG "h2w_switch_probe\n");

	pdata = pdev->dev.platform_data;
	if (pdata == NULL) {
		pr_err(TAG "pdata=NULL\n");
		return -EFAULT;
	}

	pr_debug(TAG "hp_det_port = %d\n", pdata->hp_det_port);
	pr_debug(TAG "hp_det_pin = %d\n", pdata->hp_det_pin);

	hsdev = kzalloc(sizeof(struct h2w_switch_dev), GFP_KERNEL);
	if (!hsdev) {
		pr_err(TAG "err_alloc_hsdev\n");
		goto err_alloc_hsdev;
	}
	hsdev->have_dock_hp = pdata->have_dock_hp;
	hsdev->hp_det_active_low = pdata->hp_det_active_low;

	hsdev->gpio = NvOdmGpioOpen();
	if (!hsdev->gpio) {
		pr_err(TAG "err open gpio\n");
		goto err_open_gpio;
	}
	hsdev->hp_det_pin = NvOdmGpioAcquirePinHandle(hsdev->gpio, 
			pdata->hp_det_port, pdata->hp_det_pin);
	if (!hsdev->hp_det_pin) {
		pr_err(TAG "err acquire detect pin handle\n");
		goto err_acquire_det_pin;
	}
	NvOdmGpioConfig(hsdev->gpio, hsdev->hp_det_pin, NvOdmGpioPinMode_InputData);

	if (pdata->have_dock_hp) {
		hsdev->dock_hp_det_active_low = pdata->dock_hp_det_active_low;
		hsdev->dock_hp_det_pin = NvOdmGpioAcquirePinHandle(hsdev->gpio, 
				pdata->dock_hp_det_port, pdata->dock_hp_det_pin);
		if (!hsdev->dock_hp_det_pin) {
			pr_err(TAG "err acquire dock headphone detect pin handle\n");
			goto err_acquire_dock_det_pin;
		}
		NvOdmGpioConfig(hsdev->gpio, hsdev->dock_hp_det_pin, NvOdmGpioPinMode_InputData);
	}

	hsdev->sdev.name = H2W_SWITCH_DEV_NAME;
	hsdev->sdev.print_name = h2w_switch_print_name;
	hsdev->sdev.print_state = h2w_switch_print_state;
	if (switch_dev_register(&hsdev->sdev)) {
		pr_err(TAG "err register switch device\n");
		goto err_register_sdev;
	}
	
	hsdev->workqueue = create_singlethread_workqueue("h2w_switch");
	if (!hsdev->workqueue) {
		pr_err(TAG "create_singlethread_workqueue\n");
		goto err_create_workqueue;
	}
	INIT_WORK(&hsdev->work, h2w_switch_work);

	/* Enable the interrupt at last */
	if ((NvOdmGpioInterruptRegister(hsdev->gpio, &hsdev->hp_det_irq, hsdev->hp_det_pin,
			NvOdmGpioPinMode_InputInterruptAny, h2w_switch_irq_isr, hsdev, IRQ_DEBOUNCE) 
			== NV_FALSE) || (hsdev->hp_det_irq == NULL)) {
		pr_err(TAG "err register irq\n");
		goto err_register_irq;
	}

	/* Enable the dock hp detect interrupt */
	if (hsdev->have_dock_hp) {
		if ((NvOdmGpioInterruptRegister(hsdev->gpio, &hsdev->dock_hp_det_irq, hsdev->dock_hp_det_pin,
				NvOdmGpioPinMode_InputInterruptAny, dock_h2w_switch_irq_isr, hsdev, IRQ_DEBOUNCE)
				== NV_FALSE) || (hsdev->dock_hp_det_irq == NULL)) {
			pr_err(TAG "err register dock hp irq\n");
			goto err_register_dock_hp_irq;
		}
	}

	platform_set_drvdata(pdev, hsdev);

	/* After all we simulate a isr */
	queue_work(hsdev->workqueue, &hsdev->work);
	pr_debug(TAG "h2w_switch_probe success\n");
	return 0;

err_register_dock_hp_irq:
	NvOdmGpioInterruptUnregister(hsdev->gpio, hsdev->hp_det_pin, hsdev->hp_det_irq);
err_register_irq:
	destroy_workqueue(hsdev->workqueue);
err_create_workqueue:
	switch_dev_unregister(&hsdev->sdev);
err_register_sdev:
	NvOdmGpioReleasePinHandle(hsdev->gpio, hsdev->dock_hp_det_pin);
err_acquire_dock_det_pin:
	NvOdmGpioReleasePinHandle(hsdev->gpio, hsdev->hp_det_pin);
err_acquire_det_pin:
	NvOdmGpioClose(hsdev->gpio);
err_open_gpio:
	kfree(hsdev);
err_alloc_hsdev:
	pr_err(TAG "h2w_switch_probe failed\n");
	return -1;
}

static int h2w_switch_remove(struct platform_device *pdev)
{
	struct h2w_switch_dev *hsdev;

	hsdev = (struct h2w_switch_dev *)platform_get_drvdata(pdev);
	NvOdmGpioInterruptUnregister(hsdev->gpio, hsdev->hp_det_pin, hsdev->hp_det_irq);
	NvOdmGpioReleasePinHandle(hsdev->gpio, hsdev->hp_det_pin);
	if (hsdev->have_dock_hp) {
		NvOdmGpioInterruptUnregister(hsdev->gpio, hsdev->dock_hp_det_pin, hsdev->dock_hp_det_irq);
		NvOdmGpioReleasePinHandle(hsdev->gpio, hsdev->dock_hp_det_pin);
	}
	NvOdmGpioClose(hsdev->gpio);
	destroy_workqueue(hsdev->workqueue);
	switch_dev_unregister(&hsdev->sdev);
	kfree(hsdev);
	return 0;
}

static int h2w_switch_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct h2w_switch_dev *hsdev;
	
	hsdev = (struct h2w_switch_dev *)platform_get_drvdata(pdev);
	if (hsdev != NULL) {
		NvOdmGpioInterruptMask(hsdev->hp_det_irq, NV_TRUE);
		cancel_work_sync(&hsdev->work);
	}
	return 0;
}

static int h2w_switch_resume(struct platform_device *pdev)
{
	struct h2w_switch_dev *hsdev;
	hsdev = (struct h2w_switch_dev *)platform_get_drvdata(pdev);
	if (hsdev != NULL) {
		queue_work(hsdev->workqueue, &hsdev->work);
		NvOdmGpioInterruptMask(hsdev->hp_det_irq, NV_FALSE);
	}
	return 0;
}


static struct platform_driver h2w_switch_driver = {
	.probe = h2w_switch_probe, 
	.remove = h2w_switch_remove,
	.resume = h2w_switch_resume, 
	.suspend = h2w_switch_suspend,
	.driver = {
		.name = H2W_SWITCH_DEV_NAME, 
		.owner = THIS_MODULE, 
	},
};

static int __init h2w_switch_init()
{
	int ret;
	pr_debug(TAG "h2w_switch_init\n");
	ret = platform_driver_register(&h2w_switch_driver);
	return ret;
}

static void __exit h2w_switch_exit()
{
	platform_driver_register(&h2w_switch_driver);
}

module_init(h2w_switch_init);
module_exit(h2w_switch_exit);
