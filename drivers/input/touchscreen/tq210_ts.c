/* linux/drivers/input/touchscreen/s3c-ts.c
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * Copyright (c) 2004 Arnaud Patard <arnaud.patard@rtp-net.org>
 * iPAQ H1940 touchscreen support
 *
 * ChangeLog
 *
 * 2004-09-05: Herbert Potzl <herbert@13thfloor.at>
 *	- added clock (de-)allocation code
 *
 * 2005-03-06: Arnaud Patard <arnaud.patard@rtp-net.org>
 *      - h1940_ -> s3c24xx (this driver is now also used on the n30
 *        machines :P)
 *      - Debug messages are now enabled with the config option
 *        TOUCHSCREEN_S3C_DEBUG
 *      - Changed the way the value are read
 *      - Input subsystem should now work
 *      - Use ioremap and readl/writel
 *
 * 2005-03-23: Arnaud Patard <arnaud.patard@rtp-net.org>
 *      - Make use of some undocumented features of the touchscreen
 *        controller
 *
 * 2006-09-05: Ryu Euiyoul <ryu.real@gmail.com>
 *      - added power management suspend and resume code
 *
 */

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/init.h>
#include <linux/serio.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif /* CONFIG_HAS_EARLYSUSPEND */
#include <asm/io.h>
#include <asm/irq.h>
#include <mach/hardware.h>

#include <mach/regs-adc.h>
#include <mach/ts-s3c.h>
#include <mach/irqs.h>
#include <mach/map.h>

#define CONFIG_TOUCHSCREEN_S3C_DEBUG
#undef CONFIG_TOUCHSCREEN_S3C_DEBUG
#define CONFIG_CPU_S5PV210_EVT1_abc


#ifdef CONFIG_CPU_S5PV210_EVT1_abc
#define        X_COOR_MIN      180
#define        X_COOR_MAX      4000
#define        X_COOR_FUZZ     32
#define        Y_COOR_MIN      300
#define        Y_COOR_MAX      3900
#define        Y_COOR_FUZZ     32
#endif /* CONFIG_CPU_S5PV210_EVT1_abc */

/* For ts->dev.id.version */
#define S3C_TSVERSION	 0x0101//0x010001 0x0101

#define WAIT4INT(x)  (((x)<<8) | \
		     S3C_ADCTSC_YM_SEN | S3C_ADCTSC_YP_SEN | S3C_ADCTSC_XP_SEN | \
		     S3C_ADCTSC_XY_PST(3))

#define AUTOPST	     (S3C_ADCTSC_YM_SEN | S3C_ADCTSC_YP_SEN | S3C_ADCTSC_XP_SEN | \
		     S3C_ADCTSC_AUTO_PST | S3C_ADCTSC_XY_PST(0))


#define DEBUG_LVL    KERN_DEBUG

#ifdef CONFIG_HAS_EARLYSUSPEND
void ts_early_suspend(struct early_suspend *h);
void ts_late_resume(struct early_suspend *h);
#endif /* CONFIG_HAS_EARLYSUSPEND */

/* Touchscreen default configuration */
struct s3c_ts_mach_info s3c_ts_default_cfg __initdata = {
		.delay =		10000,
		.presc = 		49,
		.oversampling_shift = 	8,
		.resol_bit = 		10
};

/*
 * Definitions & global arrays.
 */
static char *s3c_ts_name = "Resistance_ts";
static void __iomem 		*ts_base;
static struct resource		*ts_mem;
static struct resource		*ts_irq;
static struct clk		*ts_clock;
static struct s3c_ts_info 	*ts;

#ifdef CONFIG_TS1
static void __iomem 		*ts_base_1;
#endif

static void touch_timer_fire(unsigned long data)
{
	unsigned long data0;
	unsigned long data1;
	int updown;
	long x,y;
#ifdef CONFIG_ANDROID_1
	int x,y;
#ifndef CONFIG_CPU_S5PV210_EVT1_abc


	int a0,a1,a2,a3,a4,a5,a6;
#ifdef CONFIG_TOUCHSCREEN_NEW
	a0=11;
	a1=3004;
	a2=-9542528;
	a3=-4300;
	a4=4;
	a5=61817184;
	a6=65536;
#else /* CONFIG_TOUCHSCREEN_NEW */
	a0=3337;
	a1=-9;
	a2=-1000388;
	a3=-11;
	a4=-2190;
	a5=33470324;
	a6=65536;
#endif /* CONFIG_TOUCHSCREEN_NEW */
#endif /* !CONFIG_CPU_S5PV210_EVT1_abc */
#endif /* CONFIG_ANDROID_1 */

	data0 = readl(ts_base+S3C_ADCDAT0);
	data1 = readl(ts_base+S3C_ADCDAT1);
	

	updown = (!(data0 & S3C_ADCDAT0_UPDOWN)) && (!(data1 & S3C_ADCDAT1_UPDOWN));

	if (updown) {
		if (ts->count) {

#ifdef CONFIG_ANDROID_1	
#ifndef CONFIG_CPU_S5PV210_EVT1_abc
			x=(int) ts->xp;
			y=(int) ts->yp;

			ts->xp=(long) ((a2+(a0*x)+(a1*y))/a6);
			ts->yp=(long) ((a5+(a3*x)+(a4*y))/a6);
			//printk("x=%d, y=%d\n",(int) ts->xp,(int) ts->yp);

			if (ts->xp!=ts->xp_old || ts->yp!=ts->yp_old) {
				input_report_abs(ts->dev, ABS_X, ts->xp);
				input_report_abs(ts->dev, ABS_Y, ts->yp);
				input_report_abs(ts->dev, ABS_Z, 0);

				input_report_key(ts->dev, BTN_TOUCH, 1);
				input_report_abs(ts->dev, ABS_PRESSURE, 1);
				input_sync(ts->dev);
			}
			ts->xp_old=ts->xp;
			ts->yp_old=ts->yp;
#else /* !CONFIG_CPU_S5PV210_EVT1_abc */
			x=(int) ts->xp/ts->count;
			y=(int) ts->yp/ts->count;
#if defined(CONFIG_FB_S3C_LTE480WV)||defined(CONFIG_FB_S3C_LCD800480)||defined(CONFIG_FB_S3C_LCD800600)||defined(CONFIG_FB_S3C_VGA1024768)||defined(CONFIG_FB_S3C_LCD640480)||defined(CONFIG_FB_S3C_LCD480272)
			y = 4000 - y;
#endif /* CONFIG_FB_S3C_LTE480WV */
			input_report_abs(ts->dev, ABS_X, x);
			input_report_abs(ts->dev, ABS_Y, y);
			input_report_abs(ts->dev, ABS_Z, 0);
			input_report_key(ts->dev, BTN_TOUCH, 1);
			input_sync(ts->dev);
#endif /* !CONFIG_CPU_S5PV210_EVT1_abc */
#else /* CONFIG_ANDROID_1 */
			x=(long) ts->xp;//
			y=(long) ts->yp;
			#if defined(CONFIG_TOUCHSCREEN_S3C_DEBUG)
			printk("touch_timer_fire: x=%d,y=%d\n",(long) x,(long) y);
			#endif
			
			input_report_abs(ts->dev, ABS_X, x);
			input_report_abs(ts->dev, ABS_Y, y);

			input_report_key(ts->dev, BTN_TOUCH, 1);
			input_report_abs(ts->dev, ABS_PRESSURE, 1);
			input_sync(ts->dev);
#endif /* CONFIG_ANDROID_1 */
		}

		ts->xp = 0;
		ts->yp = 0;
		ts->count = 0; 
		//udelay(500);
		//udelay(500);
		writel(S3C_ADCTSC_PULL_UP_DISABLE | AUTOPST, ts_base+S3C_ADCTSC);
		writel(readl(ts_base+S3C_ADCCON) | S3C_ADCCON_ENABLE_START, ts_base+S3C_ADCCON);
	} else {
		ts->count = 0;
#ifdef CONFIG_ANDROID_1
#ifdef CONFIG_CPU_S5PV210_EVT1_abc
		input_report_abs(ts->dev, ABS_X, ts->xp);
		input_report_abs(ts->dev, ABS_Y, ts->yp);
#else /* CONFIG_CPU_S5PV210_EVT1_abc */
		input_report_abs(ts->dev, ABS_X, ts->xp_old);
		input_report_abs(ts->dev, ABS_Y, ts->yp_old);
#endif /* CONFIG_CPU_S5PV210_EVT1_abc */
		input_report_abs(ts->dev, ABS_Z, 0);
#endif /* CONFIG_ANDROID_1 */
		input_report_key(ts->dev, BTN_TOUCH, 0);
#ifndef CONFIG_ANDROID_1
		input_report_abs(ts->dev, ABS_PRESSURE, 0);
#endif /* !CONFIG_ANDROID_1 */
		input_sync(ts->dev);

		writel(WAIT4INT(0), ts_base+S3C_ADCTSC);
	}
}

static struct timer_list touch_timer =
		TIMER_INITIALIZER(touch_timer_fire, 0, 0);

static irqreturn_t stylus_updown(int irqno, void *param)
{
	unsigned long data0;
	unsigned long data1;
	int updown;

	data0 = readl(ts_base+S3C_ADCDAT0);
	data1 = readl(ts_base+S3C_ADCDAT1);
	updown = (!(data0 & S3C_ADCDAT0_UPDOWN)) && (!(data1 & S3C_ADCDAT1_UPDOWN));

	/* TODO we should never get an interrupt with updown set while
	 * the timer is running, but maybe we ought to verify that the
	 * timer isn't running anyways. */

	if (updown)
		touch_timer_fire(0);

	if (ts->s3c_adc_con == ADC_TYPE_2) {
		__raw_writel(0x0, ts_base+S3C_ADCCLRWK);
		__raw_writel(0x0, ts_base+S3C_ADCCLRINT);
	}

	return IRQ_HANDLED;
}

static irqreturn_t stylus_action(int irqno, void *param)
{
	unsigned long data0;
	unsigned long data1;

	//udelay(500);

	data0 = readl(ts_base+S3C_ADCDAT0);
	data1 = readl(ts_base+S3C_ADCDAT1);
	if (ts->resol_bit == 12) {
#if defined(CONFIG_TOUCHSCREEN_NEW)
		ts->yp += S3C_ADCDAT0_XPDATA_MASK_12BIT - (data0 & S3C_ADCDAT0_XPDATA_MASK_12BIT);
		ts->xp += S3C_ADCDAT1_YPDATA_MASK_12BIT - (data1 & S3C_ADCDAT1_YPDATA_MASK_12BIT);
#else /* CONFIG_TOUCHSCREEN_NEW */
#ifndef CONFIG_CPU_S5PV210_EVT1_abc
		ts->xp += data0 & S3C_ADCDAT0_XPDATA_MASK_12BIT;
#else /* !CONFIG_CPU_S5PV210_EVT1_abc */
		ts->xp += (S3C_ADCDAT0_XPDATA_MASK_12BIT -(data0 & S3C_ADCDAT0_XPDATA_MASK_12BIT));// 
#endif /* !CONFIG_CPU_S5PV210_EVT1_abc */
		ts->yp += data1 & S3C_ADCDAT1_YPDATA_MASK_12BIT;
#endif /* CONFIG_TOUCHSCREEN_NEW */
	} else {
#if defined(CONFIG_TOUCHSCREEN_NEW)
		ts->yp += S3C_ADCDAT0_XPDATA_MASK - (data0 & S3C_ADCDAT0_XPDATA_MASK);
		ts->xp += S3C_ADCDAT1_YPDATA_MASK - (data1 & S3C_ADCDAT1_YPDATA_MASK);
#else /* CONFIG_TOUCHSCREEN_NEW */
		ts->xp += data0 & S3C_ADCDAT0_XPDATA_MASK;
		ts->yp += data1 & S3C_ADCDAT1_YPDATA_MASK;
#endif /* CONFIG_TOUCHSCREEN_NEW */
	}

	ts->count++;
	
	if (ts->count < (1<<ts->shift)) {
		//X Y 自动转换
		writel(S3C_ADCTSC_PULL_UP_DISABLE | AUTOPST, ts_base+S3C_ADCTSC);
		writel(readl(ts_base+S3C_ADCCON) | S3C_ADCCON_ENABLE_START, ts_base+S3C_ADCCON);
	} else {
		mod_timer(&touch_timer, jiffies+10);//20
		writel(WAIT4INT(1), ts_base+S3C_ADCTSC);
	}

	if (ts->s3c_adc_con == ADC_TYPE_2) {
		__raw_writel(0x0, ts_base+S3C_ADCCLRWK);
		__raw_writel(0x0, ts_base+S3C_ADCCLRINT);
	}
	return IRQ_HANDLED;
}


static struct s3c_ts_mach_info *s3c_ts_get_platdata(struct device *dev)
{
	if (dev->platform_data != NULL)
		return (struct s3c_ts_mach_info *)dev->platform_data;

	return &s3c_ts_default_cfg;
}

/*
 * The functions for inserting/removing us as a module.
 */
static int __init s3c_ts_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct device *dev;
	struct input_dev *input_dev;
	struct s3c_ts_mach_info *s3c_ts_cfg;
	int ret, size, err;
	int irq_flags = 0;
	unsigned long ts_clk;
	//unsigned int test_reg=0;

	dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(dev, "no memory resource specified\n");
		return -ENOENT;
	}

	size = (res->end - res->start) + 1;
	ts_mem = request_mem_region(res->start, size, pdev->name);
	if (ts_mem == NULL) {
		dev_err(dev, "failed to get memory region\n");
		ret = -ENOENT;
		goto err_req;
	}

	ts_base = ioremap(res->start, size);
	if (ts_base == NULL) {
		dev_err(dev, "failed to ioremap() region\n");
		ret = -EINVAL;
		goto err_map;
	}

	ts_clock = clk_get(&pdev->dev, "adc");
	if (IS_ERR(ts_clock)) {
		dev_err(dev, "failed to find watchdog clock source\n");
		ret = PTR_ERR(ts_clock);
		goto err_clk;
	}

	clk_enable(ts_clock);
	ts_clk = clk_get_rate(ts_clock);

	s3c_ts_cfg = s3c_ts_get_platdata(&pdev->dev);
	
	#if defined(CONFIG_TS1)
		ts_base_1 = ioremap(S3C_PA_ADC, 4);
		writel((1<<17),ts_base_1+S3C_ADCCON);
		writel((1<<17),ts_base+S3C_ADCCON);
	#endif	
	#if defined(CONFIG_TS1)
		if ((s3c_ts_cfg->presc&0xff) > 0)
		{
			writel(readl(ts_base+S3C_ADCCON)|S3C_ADCCON_PRSCEN | S3C_ADCCON_PRSCVL(s3c_ts_cfg->presc&0xFF),\
			ts_base+S3C_ADCCON);
		}
			
		else
			writel(readl(ts_base+S3C_ADCCON)&((0x1<<17)), ts_base+S3C_ADCCON);
	#else
		if ((s3c_ts_cfg->presc&0xff) > 0)
		{
			writel(S3C_ADCCON_PRSCEN | S3C_ADCCON_PRSCVL(s3c_ts_cfg->presc&0xFF),\
				ts_base+S3C_ADCCON);
		}
		else
			writel(0, ts_base+S3C_ADCCON);
	#endif

	/* Initialise registers */
	if ((s3c_ts_cfg->delay&0xffff) > 0)
		writel(s3c_ts_cfg->delay & 0xffff, ts_base+S3C_ADCDLY);

	if (s3c_ts_cfg->resol_bit == 12) {
		switch (s3c_ts_cfg->s3c_adc_con) {
		case ADC_TYPE_2:
			writel(readl(ts_base+S3C_ADCCON)|S3C_ADCCON_RESSEL_12BIT, ts_base+S3C_ADCCON);
			break;

		case ADC_TYPE_1:
			writel(readl(ts_base+S3C_ADCCON)|S3C_ADCCON_RESSEL_12BIT_1, ts_base+S3C_ADCCON);
			break;

		default:
			dev_err(dev, "Touchscreen over this type of AP isn't supported !\n");
			break;
		}
	}

	writel(WAIT4INT(0), ts_base+S3C_ADCTSC);


	ts = kzalloc(sizeof(struct s3c_ts_info), GFP_KERNEL);

	input_dev = input_allocate_device();
	if (!input_dev) {
		ret = -ENOMEM;
		goto err_alloc;
	}

	ts->dev = input_dev;

	ts->dev->evbit[0] = ts->dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	ts->dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);

	if (s3c_ts_cfg->resol_bit==12) {
#ifdef CONFIG_ANDROID_1
#ifndef CONFIG_CPU_S5PV210_EVT1_abc
		input_set_abs_params(ts->dev, ABS_X, 0, 800, 0, 0);
		input_set_abs_params(ts->dev, ABS_Y, 0, 480, 0, 0);
		//  input_set_abs_params(ts->dev, ABS_Z, 0, 0, 0, 0);

		set_bit(0,ts->dev->evbit);
		set_bit(1,ts->dev->evbit);
		set_bit(2,ts->dev->evbit);
		set_bit(3,ts->dev->evbit);
		set_bit(5,ts->dev->evbit);

		set_bit(0,ts->dev->relbit);
		set_bit(1,ts->dev->relbit);

		set_bit(0,ts->dev->absbit);
		set_bit(1,ts->dev->absbit);
		set_bit(2,ts->dev->absbit);

		set_bit(0,ts->dev->swbit);

		for (err=0; err < 512; err++)
			set_bit(err,ts->dev->keybit);

		input_event(ts->dev,5,0,1);
#else /* !CONFIG_CPU_S5PV210_EVT1_abc */
		input_set_abs_params(ts->dev, ABS_X, X_COOR_MIN, X_COOR_MAX, X_COOR_FUZZ, 0);
		input_set_abs_params(ts->dev, ABS_Y, Y_COOR_MIN, Y_COOR_MAX, Y_COOR_FUZZ, 0);
#endif /* !CONFIG_CPU_S5PV210_EVT1_abc */
#else /* CONFIG_ANDROID_1 */
		input_set_abs_params(ts->dev, ABS_X, 0, 0xFFF, 0, 0);
		input_set_abs_params(ts->dev, ABS_Y, 0, 0xFFF, 0, 0);
#endif /* CONFIG_ANDROID_1 */
	} else {
		input_set_abs_params(ts->dev, ABS_X, 0, 0x3FF, 0, 0);
		input_set_abs_params(ts->dev, ABS_Y, 0, 0x3FF, 0, 0);
	}

	input_set_abs_params(ts->dev, ABS_PRESSURE, 0, 1, 0, 0);

	sprintf(ts->phys, "input(ts)");

	ts->dev->name = s3c_ts_name;
	ts->dev->phys = ts->phys;
	ts->dev->id.bustype = BUS_RS232;
	ts->dev->id.vendor = 0xDEAD;
	ts->dev->id.product = 0xBEEF;
	ts->dev->id.version = S3C_TSVERSION;

	ts->shift = s3c_ts_cfg->oversampling_shift;
	ts->resol_bit = s3c_ts_cfg->resol_bit;
	ts->s3c_adc_con = s3c_ts_cfg->s3c_adc_con;

#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = ts_early_suspend;
	ts->early_suspend.resume =ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif /* CONFIG_HAS_EARLYSUSPEND */

	/* For IRQ_PENDUP */
	ts_irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (ts_irq == NULL) {
		dev_err(dev, "no irq resource specified\n");
		ret = -ENOENT;
		goto err_irq;
	}

	ret = request_irq(ts_irq->start, stylus_updown, irq_flags, "s3c_updown", ts);
	if (ret != 0) {
		dev_err(dev, "s3c_ts.c: Could not allocate ts IRQ_PENDN !\n");
		ret = -EIO;
		goto err_irq;
	}

	/* For IRQ_ADC */
	ts_irq = platform_get_resource(pdev, IORESOURCE_IRQ, 1);
	if (ts_irq == NULL) {
		dev_err(dev, "no irq resource specified\n");
		ret = -ENOENT;
		goto err_irq;
	}

	ret = request_irq(ts_irq->start, stylus_action, irq_flags, "s3c_action", ts);
	if (ret != 0) {
		dev_err(dev, "s3c_ts.c: Could not allocate ts IRQ_ADC !\n");
		ret =  -EIO;
		goto err_irq;
	}

	printk(KERN_INFO "%s got loaded successfully : %d bits\n", s3c_ts_name, s3c_ts_cfg->resol_bit);

	/* All went ok, so register to the input system */
	ret = input_register_device(ts->dev);
	if (ret) {
		dev_err(dev, "s3c_ts.c: Could not register input device(touchscreen)!\n");
		ret = -EIO;
		goto fail;
	}
	
	
	#if defined(CONFIG_TS1)
	printk(KERN_INFO "TSSEL : %d\n", 0x01 & (readl(ts_base_1+S3C_ADCCON)>>17));
	#elif defined(CONFIG_TS0)
	printk(KERN_INFO "TSSEL : %d\n", 0x01 & (readl(ts_base+S3C_ADCCON)>>17));
	#endif

	return 0;

fail:
	free_irq(ts_irq->start, ts->dev);
	free_irq(ts_irq->end, ts->dev);

err_irq:
	input_free_device(input_dev);
	kfree(ts);

err_alloc:
	clk_disable(ts_clock);
	clk_put(ts_clock);

err_clk:
	iounmap(ts_base);
	#if defined(CONFIG_TS1)
	iounmap(ts_base_1);
	#endif

err_map:
	release_resource(ts_mem);
	kfree(ts_mem);

err_req:
	return ret;
}

static int s3c_ts_remove(struct platform_device *dev)
{
	printk(KERN_INFO "s3c_ts_remove() of TS called !\n");

	
	#ifdef CONFIG_TS1
	disable_irq(IRQ_ADC1);
	disable_irq(IRQ_PENDN1);
	free_irq(IRQ_PENDN1, ts->dev);
	free_irq(IRQ_ADC1, ts->dev);
	#endif
	#ifdef CONFIG_TS0
	disable_irq(IRQ_ADC);
	disable_irq(IRQ_PENDN);
	free_irq(IRQ_PENDN, ts->dev);
	free_irq(IRQ_ADC, ts->dev);
	#endif

	if (ts_clock) {
		clk_disable(ts_clock);
		clk_put(ts_clock);
		ts_clock = NULL;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
     unregister_early_suspend(&ts->early_suspend);
#endif /* CONFIG_HAS_EARLYSUSPEND */

	input_unregister_device(ts->dev);
	iounmap(ts_base);
	#if defined(CONFIG_TS1)
	iounmap(ts_base_1);
	//iounmap(ts_base_test);
	#endif

	return 0;
}

#ifdef CONFIG_PM
static unsigned int adccon, adctsc, adcdly;

static int s3c_ts_suspend(struct platform_device *dev, pm_message_t state)
{
	adccon = readl(ts_base+S3C_ADCCON);
	adctsc = readl(ts_base+S3C_ADCTSC);
	adcdly = readl(ts_base+S3C_ADCDLY);

	
	#ifdef CONFIG_TS1
	disable_irq(IRQ_ADC1);
	disable_irq(IRQ_PENDN1);
	//free_irq(IRQ_PENDN1, ts->dev);
	//free_irq(IRQ_ADC1, ts->dev);
	#endif
	#ifdef CONFIG_TS0
	disable_irq(IRQ_ADC);
	disable_irq(IRQ_PENDN);
	//free_irq(IRQ_PENDN, ts->dev);
	//free_irq(IRQ_ADC, ts->dev);
	#endif

	clk_disable(ts_clock);

	return 0;
}

static int s3c_ts_resume(struct platform_device *pdev)
{
	clk_enable(ts_clock);

	writel(adccon, ts_base+S3C_ADCCON);
	writel(adctsc, ts_base+S3C_ADCTSC);
	writel(adcdly, ts_base+S3C_ADCDLY);
	writel(WAIT4INT(0), ts_base+S3C_ADCTSC);

	#ifdef CONFIG_TS1
	enable_irq(IRQ_ADC1);
	enable_irq(IRQ_PENDN1);
	#endif
	#ifdef CONFIG_TS0
	enable_irq(IRQ_ADC);
	enable_irq(IRQ_PENDN);
	#endif

	return 0;
}
#else
#define s3c_ts_suspend NULL
#define s3c_ts_resume  NULL
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
void ts_early_suspend(struct early_suspend *h)
{
	struct s3c_ts_info *ts;
	ts = container_of(h, struct s3c_ts_info, early_suspend);
	s3c_ts_suspend(NULL, PMSG_SUSPEND); // platform_device is now used
}

void ts_late_resume(struct early_suspend *h)
{
	struct s3c_ts_info *ts;
	ts = container_of(h, struct s3c_ts_info, early_suspend);
	s3c_ts_resume(NULL); // platform_device is now used
}
#endif /* CONFIG_HAS_EARLYSUSPEND */

static struct platform_driver s3c_ts_driver = {
       .probe          = s3c_ts_probe,
       .remove         = s3c_ts_remove,
       .suspend        = s3c_ts_suspend,
       .resume         = s3c_ts_resume,
       .driver		= {
		.owner	= THIS_MODULE,
		.name	= "s3c_ts",
	},
};

static char banner[] __initdata = KERN_INFO "S5P Touchscreen driver, (c) 2008 Samsung Electronics\n";

static int __init s3c_ts_init(void)
{
	printk(banner);
	return platform_driver_register(&s3c_ts_driver);
}

static void __exit s3c_ts_exit(void)
{
	platform_driver_unregister(&s3c_ts_driver);
}

module_init(s3c_ts_init);
module_exit(s3c_ts_exit);

MODULE_AUTHOR("Samsung AP");
MODULE_DESCRIPTION("S5P touchscreen driver");
MODULE_LICENSE("GPL");
