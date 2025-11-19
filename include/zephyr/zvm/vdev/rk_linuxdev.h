/*
 * Copyright 2024-2025 HNU-ESNL: Guoqi Xie, Yuhao Hu, Qingqiao Wang and etc.
 * Copyright 2024-2025 openEuler SIG-Zephyr
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_ZVM_VDEV_RK_LINUXDEV_H_
#define ZEPHYR_INCLUDE_ZVM_VDEV_RK_LINUXDEV_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <zephyr/sys/atomic.h>

#define RKLD_START_PHY_ADDR DT_REG_ADDR(DT_NODELABEL(linuxdev))
#define RKLD_SIZE DT_REG_SIZE(DT_NODELABEL(linuxdev))

#endif /* ZEPHYR_INCLUDE_ZVM_VDEV_RK_LINUXDEV_H_ */