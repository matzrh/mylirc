// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2017 Sean Young <sean@mess.org>
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio/consumer.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <media/rc-core.h>

#define DRIVER_NAME	"gpio-nocarrier-ir-tx"
#define DEVICE_NAME	"GPIO IR Bit Banging Transmitter without carrier"

struct gpio_ir {
	struct gpio_desc *gpio;
	/* we need a spinlock to hold the cpu while transmitting */
	spinlock_t lock;
};

static const struct of_device_id gpio_ir_tx_of_match[] = {
	{ .compatible = "gpio-nocarrier-ir-tx", },
	{ },
};
MODULE_DEVICE_TABLE(of, gpio_ir_tx_of_match);


static int gpio_ir_tx(struct rc_dev *dev, unsigned int *txbuf,
		      unsigned int count)
{
	struct gpio_ir *gpio_ir = dev->priv;
	unsigned long flags;
	ktime_t edge;
	/*
	 * delta should never exceed 0.5 seconds (IR_MAX_DURATION) and on
	 * m68k ndelay(s64) does not compile; so use s32 rather than s64.
	 */
	s32 delta;
	int i;


	spin_lock_irqsave(&gpio_ir->lock, flags);

	edge = ktime_get();

	for (i = 0; i < count; i++) {
		if (i % 2) {
			// space
			gpiod_set_value(gpio_ir->gpio,0);
			edge = ktime_add_us(edge, txbuf[i]);
			delta = ktime_us_delta(edge, ktime_get());
			if (delta > 10) {
				spin_unlock_irqrestore(&gpio_ir->lock, flags);
				usleep_range(delta, delta + 10);
				spin_lock_irqsave(&gpio_ir->lock, flags);
			} else if (delta > 0) {
				udelay(delta);
			}
		} else {
			// pulse
			gpiod_set_value(gpio_ir->gpio,1);
			edge = ktime_add_us(edge, txbuf[i]);
			delta = ktime_us_delta(edge, ktime_get());
			if (delta > 10) {
				spin_unlock_irqrestore(&gpio_ir->lock, flags);
				usleep_range(delta, delta + 10);
				spin_lock_irqsave(&gpio_ir->lock, flags);
			} else if (delta > 0) {
				udelay(delta);
			}
		}
	}
	gpiod_set_value(gpio_ir->gpio,0);

	spin_unlock_irqrestore(&gpio_ir->lock, flags);

	return count;
}

static int gpio_ir_tx_probe(struct platform_device *pdev)
{
	struct gpio_ir *gpio_ir;
	struct rc_dev *rcdev;
	int rc;

	gpio_ir = devm_kmalloc(&pdev->dev, sizeof(*gpio_ir), GFP_KERNEL);
	if (!gpio_ir)
		return -ENOMEM;

	rcdev = devm_rc_allocate_device(&pdev->dev, RC_DRIVER_IR_RAW_TX);
	if (!rcdev)
		return -ENOMEM;

	gpio_ir->gpio = devm_gpiod_get(&pdev->dev, NULL, GPIOD_OUT_LOW);
	if (IS_ERR(gpio_ir->gpio)) {
		if (PTR_ERR(gpio_ir->gpio) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Failed to get gpio (%ld)\n",
				PTR_ERR(gpio_ir->gpio));
		return PTR_ERR(gpio_ir->gpio);
	}

	rcdev->priv = gpio_ir;
	rcdev->driver_name = DRIVER_NAME;
	rcdev->device_name = DEVICE_NAME;
	rcdev->tx_ir = gpio_ir_tx;

	spin_lock_init(&gpio_ir->lock);

	rc = devm_rc_register_device(&pdev->dev, rcdev);
	if (rc < 0)
		dev_err(&pdev->dev, "failed to register rc device\n");

	return rc;
}

static struct platform_driver gpio_ir_tx_driver = {
	.probe	= gpio_ir_tx_probe,
	.driver = {
		.name	= DRIVER_NAME,
		.of_match_table = of_match_ptr(gpio_ir_tx_of_match),
	},
};
module_platform_driver(gpio_ir_tx_driver);

MODULE_DESCRIPTION("GPIO IR Bit Banging Transmitter");
MODULE_AUTHOR("Sean Young <sean@mess.org>");
MODULE_AUTHOR("Matthias Hoelling <mhoelling@gxxxx.com>");
MODULE_LICENSE("GPL");
