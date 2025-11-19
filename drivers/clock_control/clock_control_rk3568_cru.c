/*
 * Copyright 2023 HNU-ESNL
 * Copyright 2023 openEuler SIG-Zephyr
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <errno.h>
#include <zephyr/sys/util.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/dt-bindings/clock/rk3568_clock.h>
#include <zephyr/zvm/vm_device.h>

#define DEV_CFG(dev) \
	((const struct rk3568_clk_syscon * const)(dev)->config)

/**
 * @brief: init clock syscon(cru) for rk3568 soc.
*/
static int rk3568_cru_clk_init(const struct device *dev)
{
	int ret;
	ret = z_info_sys_clkcon(dev->name, DEV_CFG(dev)->reg_base, DEV_CFG(dev)->reg_size, NULL);

	return 0;
}

static int rk3568_cru_clk_on(const struct device *dev,
			      clock_control_subsys_t sub_system)
{
	return 0;
}

static int rk3568_cru_clk_off(const struct device *dev,
			       clock_control_subsys_t sub_system)
{
	return 0;
}

static const struct clock_control_driver_api rk3568_cru_api = {
	.on = rk3568_cru_clk_on,
	.off = rk3568_cru_clk_off,
};

static struct rk3568_clk_syscon rk3568_cru_cfg = {
	.reg_base = DT_REG_ADDR(DT_NODELABEL(cru)),
	.reg_size = DT_REG_SIZE(DT_NODELABEL(cru)),
	.priv_data = NULL,
};

DEVICE_DT_DEFINE(DT_NODELABEL(cru),
		    &rk3568_cru_clk_init,
		    NULL,
		    NULL, &rk3568_cru_cfg,
		    PRE_KERNEL_1, CONFIG_RK3568_CLOCK_CONTROL_INIT_PRIORITY,
		    &rk3568_cru_api);
