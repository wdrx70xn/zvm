/*
 * Copyright 2023-2024 HNU-ESNL
 * Copyright 2024 openEuler SIG-Zephyr
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_ZVM_VDEV_VIRT_SYSCON_H_
#define ZEPHYR_INCLUDE_ZVM_VDEV_VIRT_SYSCON_H_

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/spinlock.h>


/* The clock syscon node */
struct virt_syscon {
	const char *name;

	uint32_t reg_base;
	uint32_t reg_size;

	sys_dnode_t syscon_node;
};

/* The clock syscon list */
struct virt_syscon_list {
	struct k_spinlock vsyscon_lock;

	uint16_t syscon_num;
	sys_dlist_t syscon_list;
};

#endif /* ZEPHYR_INCLUDE_ZVM_VDEV_VIRT_SYSCON_H_ */