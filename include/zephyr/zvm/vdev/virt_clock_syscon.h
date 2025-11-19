/*
 * Copyright 2023-2024 HNU-ESNL
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_ZVM_VDEV_VIRT_CLOCK_SYSCON_H_
#define ZEPHYR_INCLUDE_ZVM_VDEV_VIRT_CLOCK_SYSCON_H_

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/spinlock.h>


/* The clock syscon node */
struct virt_clk_syscon {
	const char *name;

	uint32_t reg_base;
	uint32_t reg_size;
	void *clk_syscon;

	sys_dnode_t clk_node;
};

/* The clock syscon list */
struct virt_clk_syscon_list {
	struct k_spinlock vclk_lock;

	uint16_t clk_num;
	sys_dlist_t clk_syscon_list;
};

#endif /* ZEPHYR_INCLUDE_ZVM_VDEV_VIRT_CLOCK_SYSCON_H_ */
