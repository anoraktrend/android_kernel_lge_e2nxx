/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/memory.h>
#include <linux/regulator/qpnp-regulator.h>
#include <linux/msm_tsens.h>
#include <asm/mach/map.h>
#include <asm/hardware/gic.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <mach/msm_iomap.h>
#include <mach/restart.h>
#ifdef CONFIG_ION_MSM
#include <mach/ion.h>
#endif
#include <mach/msm_memtypes.h>
#include <mach/socinfo.h>
#include <mach/board.h>
#include <mach/clk-provider.h>
#include <mach/msm_smd.h>
#include <mach/rpm-smd.h>
#include <mach/rpm-regulator-smd.h>
#include <mach/msm_smem.h>
#include <linux/msm_thermal.h>
#include "../board-dt.h"
#include "../clock.h"
#include "../platsmp.h"
#include "../spm.h"
#include "../pm.h"
#include "../modem_notifier.h"
#include <mach/board_lge.h>

#ifdef CONFIG_LGE_LCD_TUNING
#include "../../../../drivers/video/msm/mdss/mdss_dsi.h"
#include <linux/uaccess.h>

#define TUNING_BUFSIZE 8192
#define TUNING_REGSIZE 50
#define TUNING_REGNUM 160

struct tuning_buff{
	char buf[TUNING_REGNUM][TUNING_REGSIZE];
	int size;
	int idx;
};

extern struct mdss_panel_data *pdata_base;
char set_buff[TUNING_REGNUM][TUNING_REGSIZE];
int tun_lcd[128];

int lcd_set_values(int *tun_lcd_t)
{
	memset(tun_lcd,0,128*sizeof(int));
	memcpy(tun_lcd,tun_lcd_t,128*sizeof(int));
	printk("lcd_set_values ::: tun_lcd[0]=[%x], tun_lcd[1]=[%x], tun_lcd[2]=[%x] ......\n"
				,tun_lcd[0],tun_lcd[1],tun_lcd[2]);
	return 0;
}

static int lcd_get_values(int *tun_lcd_t)
{
	memset(tun_lcd_t,0,128*sizeof(int));
	memcpy(tun_lcd_t,tun_lcd,128*sizeof(int));
	printk("lcd_get_values\n");
	return 0;
}

static int tuning_read_regset(unsigned long tmp)
{
	struct tuning_buff *rbuf = (struct tuning_buff *)tmp;
	int i;
	int size;

	struct mdss_dsi_ctrl_pdata *ctrl = NULL;

	if(pdata_base == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl =  container_of(pdata_base, struct mdss_dsi_ctrl_pdata, panel_data);

	size = ctrl->on_cmds.cmd_cnt;
	printk(KERN_INFO "read_file\n");

	for(i = 0; i < size; i++){
		if(copy_to_user(rbuf->buf[i], ctrl->on_cmds.cmds[i].payload, ctrl->on_cmds.cmds[i].dchdr.dlen)){
			printk(KERN_ERR "read_file : error of copy_to_user_buff\n");
			return -EFAULT;
		}
	}

	if(put_user(size, &(rbuf->size))){
		printk(KERN_ERR "read_file : error of copy_to_user_buffsize\n");
		return -EFAULT;
	}

	return 0;
}

static int tuning_write_regset(unsigned long tmp)
{
	char *buff;
	struct mdss_dsi_ctrl_pdata *ctrl = NULL;
	struct tuning_buff *wbuf = (struct tuning_buff *)tmp;
	int i = wbuf->idx;

	printk(KERN_INFO "write file\n");

	if(pdata_base == NULL) {
		pr_err("%s: Invalid input data\n", __func__);
		return -EINVAL;
	}

	ctrl = container_of(pdata_base, struct mdss_dsi_ctrl_pdata, panel_data);

	buff = kmalloc(TUNING_BUFSIZE, GFP_KERNEL);
	if (copy_from_user(buff, wbuf->buf, wbuf->size)) {
		printk(KERN_ERR "write_file : error of copy_from_user\n");
		return -EFAULT;
	}

	memset(set_buff[i], 0x00, TUNING_REGSIZE);
	memcpy(set_buff[i], buff, wbuf->size);
	ctrl->on_cmds.cmds[i].payload = set_buff[i];
	ctrl->on_cmds.cmds[i].dchdr.dlen = wbuf->size;

	kfree(buff);
	return 0;
}

static struct lcd_platform_data lcd_pdata ={
	.set_values = lcd_set_values,
	.get_values = lcd_get_values,
	.read_regset = tuning_read_regset,
	.write_regset = tuning_write_regset,
};

static struct platform_device lcd_misc_device = {
	.name = "lcd_misc_msm",
	.dev = {
	.platform_data = &lcd_pdata,
	}
};

void __init lge_add_lcd_misc_devices(void)
{
	printk("%s : LCD_DEBUG \n", __func__);
	platform_device_register(&lcd_misc_device);
}
#endif

static struct memtype_reserve msm8226_reserve_table[] __initdata = {
	[MEMTYPE_SMI] = {
	},
	[MEMTYPE_EBI0] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
	[MEMTYPE_EBI1] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
};


static int msm8226_paddr_to_memtype(unsigned int paddr)
{
	return MEMTYPE_EBI1;
}

static struct of_dev_auxdata msm8226_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF9824000, \
			"msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF98A4000, \
			"msm_sdcc.2", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF9864000, \
			"msm_sdcc.3", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF9824900, \
			"msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF98A4900, \
			"msm_sdcc.2", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF9864900, \
			"msm_sdcc.3", NULL),
	{}
};

static struct reserve_info msm8226_reserve_info __initdata = {
	.memtype_reserve_table = msm8226_reserve_table,
	.paddr_to_memtype = msm8226_paddr_to_memtype,
};

static void __init msm8226_early_memory(void)
{
	reserve_info = &msm8226_reserve_info;
	of_scan_flat_dt(dt_scan_for_memory_hole, msm8226_reserve_table);
}

static void __init msm8226_reserve(void)
{
	reserve_info = &msm8226_reserve_info;
	of_scan_flat_dt(dt_scan_for_memory_reserve, msm8226_reserve_table);
#ifdef CONFIG_MACH_LGE
	of_scan_flat_dt(lge_init_dt_scan_chosen, NULL);
#endif
	msm_reserve();
#if defined(CONFIG_ANDROID_RAM_CONSOLE)
	lge_reserve();
#endif
}

#if defined(CONFIG_PRE_SELF_DIAGNOSIS)
int pre_selfd_set_values(int kcal_r, int kcal_g, int kcal_b)
{
	return 0;
}

static int pre_selfd_get_values(int *kcal_r, int *kcal_g, int *kcal_b)
{
	return 0;
}

static struct pre_selfd_platform_data pre_selfd_pdata = {
	.set_values = pre_selfd_set_values,
	.get_values = pre_selfd_get_values,
};


static struct platform_device pre_selfd_platrom_device = {
	.name   = "pre_selfd_ctrl",
	.dev = {
		.platform_data = &pre_selfd_pdata,
	}
};

void __init lge_add_pre_selfd_devices(void)
{
	pr_info(" PRE_SELFD_DEBUG : %s\n", __func__);
	platform_device_register(&pre_selfd_platrom_device);
}
#endif /* CONFIG_PRE_SELF_DIAGNOSIS */

/*
 * Used to satisfy dependencies for devices that need to be
 * run early or in a particular order. Most likely your device doesn't fall
 * into this category, and thus the driver should not be added here. The
 * EPROBE_DEFER can satisfy most dependency problems.
 */
void __init msm8226_add_drivers(void)
{
	msm_smem_init();
	msm_init_modem_notifier_list();
	msm_smd_init();
	msm_rpm_driver_init();
	msm_spm_device_init();
	msm_pm_sleep_status_init();
	rpm_regulator_smd_driver_init();
	qpnp_regulator_init();
	if (of_board_is_rumi())
		msm_clock_init(&msm8226_rumi_clock_init_data);
	else
		msm_clock_init(&msm8226_clock_init_data);
	tsens_tm_init_driver();
	msm_thermal_device_init();
#if defined(CONFIG_ANDROID_RAM_CONSOLE)
	lge_add_persistent_device();
#endif
#ifdef CONFIG_USB_G_LGE_ANDROID
    lge_android_usb_init();
#endif
#ifdef CONFIG_LGE_DIAG_ENABLE_SYSFS
	lge_add_diag_devices();
#endif
#if defined(CONFIG_LGE_LCD_TUNING)
	lge_add_lcd_misc_devices();
#endif

#ifdef CONFIG_LGE_QFPROM_INTERFACE
	lge_add_qfprom_devices();
#endif

#ifdef CONFIG_LGE_ENABLE_MMC_STRENGTH_CONTROL
    lge_add_mmc_strength_devices();
#endif
#if defined(CONFIG_PRE_SELF_DIAGNOSIS)
	lge_add_pre_selfd_devices();
#endif
}

void __init msm8226_init(void)
{
	struct of_dev_auxdata *adata = msm8226_auxdata_lookup;

	if (socinfo_init() < 0)
		pr_err("%s: socinfo_init() failed\n", __func__);

	msm8226_init_gpiomux();
	board_dt_populate(adata);
	msm8226_add_drivers();
}

static const char *msm8226_dt_match[] __initconst = {
	"qcom,msm8226",
	"qcom,msm8926",
	"qcom,apq8026",
	NULL
};

DT_MACHINE_START(MSM8226_DT, "Qualcomm MSM 8226 (Flattened Device Tree)")
	.map_io = msm_map_msm8226_io,
	.init_irq = msm_dt_init_irq,
	.init_machine = msm8226_init,
	.handle_irq = gic_handle_irq,
	.timer = &msm_dt_timer,
	.dt_compat = msm8226_dt_match,
	.reserve = msm8226_reserve,
	.init_very_early = msm8226_early_memory,
	.restart = msm_restart,
	.smp = &arm_smp_ops,
MACHINE_END
