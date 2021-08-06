/*
 * Copyright 2018 NXP
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <malloc.h>
#include <errno.h>
#include <asm/io.h>
#include <miiphy.h>
#include <netdev.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm-generic/gpio.h>
#include <fsl_esdhc.h>
#include <mmc.h>
#include <asm/arch/imx8mq_pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/mach-imx/gpio.h>
#include <asm/mach-imx/mxc_i2c.h>
#include <asm/arch/clock.h>
#include <asm/mach-imx/video.h>
#include <asm/arch/video_common.h>
#include <spl.h>
#include <power/pmic.h>
#include <power/pfuze100_pmic.h>
#include "../../freescale/common/pfuze.h"

DECLARE_GLOBAL_DATA_PTR;

#define WDOG_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_HYS | PAD_CTL_PUE)

static iomux_v3_cfg_t const wdog_pads[] = {
	IMX8MQ_PAD_GPIO1_IO02__WDOG1_WDOG_B | MUX_PAD_CTRL(WDOG_PAD_CTRL),
};

#define UART_PAD_CTRL	(PAD_CTL_DSE6 | PAD_CTL_FSEL1)

static iomux_v3_cfg_t const uart_pads[] = {
	IMX8MQ_PAD_UART3_RXD__UART3_RX | MUX_PAD_CTRL(UART_PAD_CTRL),
	IMX8MQ_PAD_UART3_TXD__UART3_TX | MUX_PAD_CTRL(UART_PAD_CTRL),
};

int board_early_init_f(void)
{
	struct wdog_regs *wdog = (struct wdog_regs *)WDOG1_BASE_ADDR;

	imx_iomux_v3_setup_multiple_pads(wdog_pads, ARRAY_SIZE(wdog_pads));

	set_wdog_reset(wdog);

	imx_iomux_v3_setup_multiple_pads(uart_pads, ARRAY_SIZE(uart_pads));

	return 0;
}

#ifdef CONFIG_BOARD_POSTCLK_INIT
int board_postclk_init(void)
{
	/* TODO */
	return 0;
}
#endif

static phys_size_t imx8_ddr_size(void)
{
    unsigned long mem = 0x3d400000;
    unsigned long value = readl(mem+0x200);
    phys_size_t dram_size = 0x40000000;;

    switch (value) {
    case 0x1f:
        dram_size = 0x40000000;
        break;
    case 0x16:
        dram_size = 0x80000000;
        break;
    case 0x17:
        /* dram_size = 0x100000000;*/
        /* reports 3G only, if reports above then gets crashed */
        dram_size = 0xc0000000;
        break;
    default:
        break;
    };
    return dram_size;
}

int dram_init(void)
{
    gd->ram_size = imx8_ddr_size();
    return 0;
}

void _dram_init_banksize(void)

{
    gd->bd->bi_dram[0].start = PHYS_SDRAM;
    gd->bd->bi_dram[0].size = imx8_ddr_size();
}

phys_size_t get_effective_memsize(void)
{
    phys_size_t dram_size = imx8_ddr_size();
    if (dram_size > 0x80000000)
        return 0x80000000;

    return dram_size;
}

#ifdef CONFIG_OF_BOARD_SETUP
int ft_board_setup(void *blob, bd_t *bd)
{
	return 0;
}
#endif

static iomux_v3_cfg_t const usbmux_pads[] = {
	IMX8MQ_PAD_GPIO1_IO04__GPIO1_IO4 | MUX_PAD_CTRL(NO_PAD_CTRL),
};

static void setup_iomux_usbmux(void)
{
	imx_iomux_v3_setup_multiple_pads(usbmux_pads, ARRAY_SIZE(usbmux_pads));

	gpio_request(IMX_GPIO_NR(1, 4), "usb_mux");
	gpio_direction_output(IMX_GPIO_NR(1, 4), 0);
}

static void setup_usbmux(void)
{
	setup_iomux_usbmux();
}

int board_init(void)
{
	setup_usbmux();

	return 0;
}

int board_mmc_get_env_dev(int devno)
{
	const char *s = env_get("atp");
	if (s != NULL) {
		printf("ATP Mode: Save environmet on eMMC\n");
		return CONFIG_SYS_MMC_ENV_DEV;
	}
	return devno;
}

int board_late_init(void)
{

#ifdef CONFIG_ENV_IS_IN_MMC
	board_late_mmc_env_init();
#endif

	return 0;
}

#ifdef CONFIG_FSL_FASTBOOT
#ifdef CONFIG_ANDROID_RECOVERY
int is_recovery_key_pressing(void)
{
	return 0; /*TODO*/
}
#endif /*CONFIG_ANDROID_RECOVERY*/
#endif /*CONFIG_FSL_FASTBOOT*/
