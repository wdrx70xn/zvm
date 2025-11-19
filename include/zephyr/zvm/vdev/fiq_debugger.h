/*
 * Copyright 2023 HNU-ESNL
 * Copyright 2023 openEuler SIG-Zephyr
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_ZVM_VDEV_FIQ_DEBUGGER_H_
#define ZEPHYR_INCLUDE_ZVM_VDEV_FIQ_DEBUGGER_H_

#include <zephyr/device.h>
#include <zephyr/devicetree.h>

/**
 * @brief fiq debugger configuration.
 *
 * @param is_fiq Is it a fiq interrupt? if 'false', it is a irq interrupt
 * @param serial_id The serial that used to debugger system
 * @param baudrate baudrate of this serial
 */
struct fiq_debugger_device_config {
    bool is_fiq;
    uint8_t serial_id;

    uint32_t baudrate;

    void (*irq_config_func)(const struct device *dev);

};

/**
 * @brief Inject fiq debugger for rk3568 linux vm. Because
 * the hardware debugger irq is not triggered.
*/
void vm_debugger_softirq_inject(void *user_data);

/**
 * @brief init fiq debug for rk3568 borad.
*/
int vm_init_bdspecific_device(struct z_vm *vm);

#endif /* ZEPHYR_INCLUDE_ZVM_VDEV_FIQ_DEBUGGER_H_ */
