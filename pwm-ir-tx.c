/*
 * Copyright (C) 2017 Sean Young <sean@mess.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pwm.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <media/rc-core.h>

#define DRIVER_NAME	"pwm-ir-tx"
#define DEVICE_NAME	"PWM IR Transmitter"

struct pwm_ir {
	struct pwm_device *pwm;
	unsigned int carrier;
	unsigned int duty_cycle;
};

static const struct of_device_id pwm_ir_of_match[] = {
	{ .compatible = "pwm-ir-tx", },
	{ },
};
MODULE_DEVICE_TABLE(of, pwm_ir_of_match);

static int pwm_ir_set_duty_cycle(struct rc_dev *dev, u32 duty_cycle)
{
	struct pwm_ir *pwm_ir = dev->priv;

	pwm_ir->duty_cycle = duty_cycle;

	return 0;
}

static int pwm_ir_set_carrier(struct rc_dev *dev, u32 carrier)
{
	struct pwm_ir *pwm_ir = dev->priv;

	if (!carrier)
		return -EINVAL;

	pwm_ir->carrier = carrier;

	return 0;
}

static int pwm_ir_tx(struct rc_dev *dev, unsigned int *txbuf,
		     unsigned int count)
{
	struct pwm_ir *pwm_ir = dev->priv;
	struct pwm_device *pwm = pwm_ir->pwm;
	struct pwm_state state;
	int i, duty, period;
	ktime_t edge;
	long delta;

	period = DIV_ROUND_CLOSEST(NSEC_PER_SEC, pwm_ir->carrier);
	duty = DIV_ROUND_CLOSEST(pwm_ir->duty_cycle * period, 100);

	pwm_get_state(pwm,&state);
	state.period=period;
	state.duty_cycle=duty;

	edge = ktime_get();

	for (i = 0; i < count; i++) {
		if (i % 2) // space
			state.enabled=false;
		else
			state.enabled=true;
		pwm_apply_state(pwm,&state);

		edge = ktime_add_us(edge, txbuf[i]);
		delta = ktime_us_delta(edge, ktime_get());
		if (delta > 0)
			usleep_range(delta, delta + 10);
	}
	
	state.duty_cycle=0; //set duty cycle to 0 to have defined output
	state.enabled=false;
	pwm_apply_state(pwm,&state);

	return count;
}


static int pwm_ir_probe(struct platform_device *pdev)
{
	struct pwm_ir *pwm_ir;
	struct rc_dev *rcdev;
	int rc;


	printk(KERN_INFO "starting pwm ir probe...\n");
	pwm_ir = devm_kmalloc(&pdev->dev, sizeof(*pwm_ir), GFP_KERNEL);
	if (!pwm_ir)
		return -ENOMEM;

	pwm_ir->pwm = devm_pwm_get(&pdev->dev, NULL);
	printk(KERN_INFO "got a pwm device\n");
	if (IS_ERR(pwm_ir->pwm))
		return PTR_ERR(pwm_ir->pwm);
	
	pwm_adjust_config(pwm_ir->pwm);
	
	pwm_ir->carrier = 38000;
	pwm_ir->duty_cycle = 50;

	rcdev = devm_rc_allocate_device(&pdev->dev, RC_DRIVER_IR_RAW_TX);
	if (!rcdev)
		return -ENOMEM;

	rcdev->priv = pwm_ir;
	rcdev->driver_name = DRIVER_NAME;
	rcdev->device_name = DEVICE_NAME;
	rcdev->tx_ir = pwm_ir_tx;
	rcdev->s_tx_duty_cycle = pwm_ir_set_duty_cycle;
	rcdev->s_tx_carrier = pwm_ir_set_carrier;

	rc = devm_rc_register_device(&pdev->dev, rcdev);
	if (rc < 0)
		dev_err(&pdev->dev, "failed to register rc device\n");

	return rc;
}

static struct platform_driver pwm_ir_driver = {
	.probe = pwm_ir_probe,
	.driver = {
		.name	= DRIVER_NAME,
		.of_match_table = of_match_ptr(pwm_ir_of_match),
	},
};
module_platform_driver(pwm_ir_driver);

MODULE_DESCRIPTION("PWM IR Transmitter");
MODULE_AUTHOR("Sean Young <sean@mess.org>");
MODULE_AUTHOR("Matthias Hoelling <mhoelling@gxxxx.com>");
MODULE_LICENSE("GPL");
