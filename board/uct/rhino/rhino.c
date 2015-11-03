/*
 * am3517evm.c - board file for TI's AM3517 family of devices.
 *
 * Author: Vaibhav Hiremath <hvaibhav@ti.com>
 *
 * Based on ti/evm/evm.c
 *
 * Copyright (C) 2010
 * Texas Instruments Incorporated - http://www.ti.com/
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <asm/omap_musb.h>
#include <asm/arch/am35x_def.h>
#include <asm/arch/mem.h>
#include <asm/arch/mux.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/mmc_host_def.h>
#include <asm/arch/musb.h>
#include <asm/mach-types.h>
#include <asm/errno.h>
#include <asm/gpio.h>
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/usb/musb.h>
#include <spartan3.h>
#include <spi.h>
#include <i2c.h>
#include <netdev.h>
#include "rhino.h"

DECLARE_GLOBAL_DATA_PTR;

#if defined(CONFIG_FPGA) && !defined(CONFIG_SPL_BUILD)

/* Define FPGA_DEBUG to get debug printf's */
#ifdef  FPGA_DEBUG
#define PRINTF(fmt,args...)     printf (fmt ,##args)
#else
#define PRINTF(fmt,args...)
#endif

/* Timing definitions for FPGA */
static const u32 gpmc_fpga_cfg[] = {
	GPMC_FPGA_CONFIG1,
	GPMC_FPGA_CONFIG2,
	GPMC_FPGA_CONFIG3,
	GPMC_FPGA_CONFIG4,
	GPMC_FPGA_CONFIG5,
	GPMC_FPGA_CONFIG6,
};

//Declare Spi Variable.
struct spi_slave *spi;

int fpga_pre_fn(int cookie)
{
	PRINTF("%s:%d: FPGA pre-configuration\n", __func__, __LINE__);

	gpio_request(FPGA_VCCINT_EN, "FPGA_VCCINT_EN");
	gpio_direction_output(FPGA_VCCINT_EN, 1);
	gpio_request(FPGA_VCCO_AUX_EN, "FPGA_VCCO_AUX_EN");
	gpio_direction_output(FPGA_VCCO_AUX_EN, 1);
	gpio_request(FPGA_VCCMGT_EN, "FPGA_VCCMGT_EN");
	gpio_direction_output(FPGA_VCCMGT_EN, 1);

	//	gpio_request(FPGA_CCLK, "FPGA_CCLK");
	//	gpio_direction_output(FPGA_CCLK, 0);
	//	gpio_request(FPGA_DIN, "FPGA_DIN");
	//	gpio_direction_output(FPGA_DIN, 0);
	gpio_request(FPGA_PROG, "FPGA_PROG");
	gpio_direction_output(FPGA_PROG, 1);
	gpio_request(FPGA_INIT_B, "FPGA_INIT_B");
	gpio_direction_input(FPGA_INIT_B);
	gpio_request(FPGA_DONE, "FPGA_DONE");
	gpio_direction_input(FPGA_DONE);
	gpio_request(FPGA_INIT_B_DIR, "FPGA_INIT_B_DIR");
	gpio_direction_output(FPGA_INIT_B_DIR, 0);

	//Setup SPI
	PRINTF("Starting SPI Setup\n");
	spi = spi_setup_slave(1,		//bus
			0,		//CS
			48000000,		//Max Hz
			0			//Mode
			);
	PRINTF("SPI Setup Compelte\n");
	PRINTF("Claiming SPI Bus\n");
	spi_claim_bus(spi);
	PRINTF("Bus Claimed!\n");
	spi_xfer(spi, 0, NULL, NULL,SPI_XFER_BEGIN);//send spi transfer begin flags

	return 0;
}

int fpga_pgm_fn(int nassert, int nflush, int cookie)
{
	PRINTF("%s:%d: FPGA PROGRAM cookie=%d nassert=%d\n", __func__, __LINE__, cookie, nassert);

	gpio_set_value(FPGA_PROG, !nassert);

	return nassert;
}


int fpga_clk_fn(int assert_clk, int flush, int cookie)
{
	gpio_set_value(FPGA_CCLK, assert_clk);

	return assert_clk;
}

int fpga_init_fn(int cookie)
{
	PRINTF("%s:%d: FPGA INIT CHECK cookie=%d FPGA_INIT_B=%d\n", __func__, __LINE__, cookie, gpio_get_value(FPGA_INIT_B));

	return !gpio_get_value(FPGA_INIT_B);
}

int fpga_done_fn(int cookie)
{
	PRINTF("%s:%d: FPGA DONE CHECK cookie=%d FPGA_DONE_B=%d\n", __func__, __LINE__, cookie, gpio_get_value(FPGA_DONE));

	return gpio_get_value(FPGA_DONE);
}

int fpga_wr_fn(int nassert_write, int flush, int cookie)
{
	gpio_set_value(FPGA_DIN, nassert_write);

	return nassert_write;
}

int fpga_post_fn(int cookie)
{
	PRINTF("%s:%d: FPGA post-configuration\n", __func__, __LINE__);

	return 0;
}

int fpga_bwr_fn(const void *buf, size_t bsize, int flush, int cookie)
{
	PRINTF("%s:%d: FPGA BLOCK WRITE cookie=%d\n", __func__, __LINE__, cookie);

	size_t blocksize = 16184;

	unsigned int *data_ptr = (unsigned int*) buf;

	size_t wordcount = 0;
	size_t wsize = 0;
	size_t num_words = 0;

	//Prevent truncation
	if(bsize % 4 == 0)
	{
		wsize = bsize/4;
	}
	else
	{
		wsize = (size_t)((bsize/4) + 1);
	}

	while (wordcount < wsize)
	{
		num_words = wordcount + blocksize;

		//Check for incomplete block
		if(num_words > wsize)
		{
			num_words = wsize;
		}

		while (wordcount < num_words)
		{
			//Write to SPI	
			spi_xfer(spi, 32, data_ptr++, NULL, 0);
			wordcount++;
		}
#ifdef CONFIG_SYS_FPGA_PROG_FEEDBACK
		putc ('.');             /* let them know we are alive */
#endif
	}

	return 1;
}

xilinx_spartan3_slave_serial_fns rhino_fpga_fns = {
	fpga_pre_fn,
	fpga_pgm_fn,
	fpga_clk_fn,
	fpga_init_fn,
	fpga_done_fn,
	fpga_wr_fn,
	fpga_post_fn,
	fpga_bwr_fn,
};

xilinx_desc fpga = XILINX_XC6SLX4_DESC(slave_serial,
		(void *)&rhino_fpga_fns, 0);
#endif

/*
 * Routine: board_init
 * Description: Early hardware init.
 */
int board_init(void)
{
	gpmc_init(); /* in SRAM or SDRAM, finish GPMC */

#if defined(CONFIG_FPGA) && !defined(CONFIG_SPL_BUILD)
	/* Configure GPMC for FPGA memory accesses */
	enable_gpmc_cs_config(gpmc_fpga_cfg, &gpmc_cfg->cs[1], FPGA_CS1_BASE, GPMC_SIZE_64M);
	enable_gpmc_cs_config(gpmc_fpga_cfg, &gpmc_cfg->cs[2], FPGA_CS2_BASE, GPMC_SIZE_128M);
	enable_gpmc_cs_config(gpmc_fpga_cfg, &gpmc_cfg->cs[3], FPGA_CS3_BASE, GPMC_SIZE_128M);
	enable_gpmc_cs_config(gpmc_fpga_cfg, &gpmc_cfg->cs[4], FPGA_CS4_BASE, GPMC_SIZE_128M);
	enable_gpmc_cs_config(gpmc_fpga_cfg, &gpmc_cfg->cs[5], FPGA_CS5_BASE, GPMC_SIZE_128M);
	enable_gpmc_cs_config(gpmc_fpga_cfg, &gpmc_cfg->cs[6], FPGA_CS6_BASE, GPMC_SIZE_128M);
	enable_gpmc_cs_config(gpmc_fpga_cfg, &gpmc_cfg->cs[7], FPGA_CS7_BASE, GPMC_SIZE_128M);

	//fpga_init();
	//fpga_add(fpga_xilinx, &fpga);

	//gpio_request(FPGA_DONE, "FPGA_DONE");
	//gpio_direction_input(FPGA_DONE);

#endif

	/* board id for Linux */
	gd->bd->bi_arch_number = MACH_TYPE_RHINO;
	/* boot param addr */
	gd->bd->bi_boot_params = (OMAP34XX_SDRC_CS0 + 0x100);

	return 0;
}

#ifdef CONFIG_USB_MUSB_AM35X
static struct musb_hdrc_config musb_config = {
	.multipoint     = 1,
	.dyn_fifo       = 1,
	.num_eps        = 16,
	.ram_bits       = 12,
};

static struct omap_musb_board_data musb_board_data = {
	.set_phy_power		= am35x_musb_phy_power,
	.clear_irq		= am35x_musb_clear_irq,
	.reset			= am35x_musb_reset,
};

static struct musb_hdrc_platform_data musb_plat = {
#if defined(CONFIG_MUSB_HOST)
	.mode           = MUSB_HOST,
#elif defined(CONFIG_MUSB_GADGET)
	.mode		= MUSB_PERIPHERAL,
#else
#error "Please define either CONFIG_MUSB_HOST or CONFIG_MUSB_GADGET"
#endif
	.config         = &musb_config,
	.power          = 250,
	.platform_ops	= &am35x_ops,
	.board_data	= &musb_board_data,
};

static void am3517_evm_musb_init(void)
{
	/*
	 * Set up USB clock/mode in the DEVCONF2 register.
	 * USB2.0 PHY reference clock is 13 MHz
	 */
	clrsetbits_le32(&am35x_scm_general_regs->devconf2,
			CONF2_REFFREQ | CONF2_OTGMODE | CONF2_PHY_GPIOMODE,
			CONF2_REFFREQ_13MHZ | CONF2_SESENDEN |
			CONF2_VBDTCTEN | CONF2_DATPOL);

	musb_register(&musb_plat, &musb_board_data,
			(void *)AM35XX_IPSS_USBOTGSS_BASE);
}
#else
#define am3517_evm_musb_init() do {} while (0)
#endif

/*
 * Routine: misc_init_r
 * Description: Init i2c, ethernet, etc... (done here so udelay works)
 */
int misc_init_r(void)
{
	volatile unsigned int ctr;

#ifdef CONFIG_SYS_I2C_OMAP34XX
	i2c_init(CONFIG_SYS_OMAP24_I2C_SPEED, CONFIG_SYS_OMAP24_I2C_SLAVE);
#endif

	dieid_num_r();

	am3517_evm_musb_init();

	/* activate PHY reset */
	//gpio_direction_output(65, 0);
	//gpio_set_value(65, 0);

	ctr  = 0;
	do {
		udelay(1000);
		ctr++;
	} while (ctr < 300);

	/* deactivate PHY reset */
	//gpio_set_value(65, 1);

	/* allow the PHY to stabilize and settle down */
	ctr = 0;
	do {
		udelay(1000);
		ctr++;
	} while (ctr < 300);

	return 0;
}

/*
 * Routine: set_muxconf_regs
 * Description: Setting up the configuration Mux registers specific to the
 *		hardware. Many pins need to be moved from protect to primary
 *		mode.
 */
void set_muxconf_regs(void)
{
	MUX_RHINO();
}

#if defined(CONFIG_GENERIC_MMC) && !defined(CONFIG_SPL_BUILD)
int board_mmc_init(bd_t *bis)
{
	return omap_mmc_init(1, 0, 0, -1, -1);
}
#endif

#if defined(CONFIG_USB_ETHER) && defined(CONFIG_MUSB_GADGET)
int board_eth_init(bd_t *bis)
{
	int rv, n = 0;

	rv = cpu_eth_init(bis);
	if (rv > 0)
		n += rv;

	rv = usb_eth_initialize(bis);
	if (rv > 0)
		n += rv;

	return n;
}
#endif
