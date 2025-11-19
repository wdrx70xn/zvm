/*
 * Copyright 2023 HNU-ESNL
 * Copyright 2023 openEuler SIG-Zephyr
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/arch/cpu.h>
#include <zephyr/init.h>
#include <getopt.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/zvm/zvm.h>
#include <zephyr/zvm/vm_device.h>
#include <zephyr/zvm/vdev/vgic_common.h>
#include <zephyr/zvm/vdev/rk_linuxdev.h>
#include <zephyr/timing/timing.h>

LOG_MODULE_DECLARE(ZVM_MODULE_NAME);

#define DEV_DATA(dev) \
	((struct virt_device_data *)(dev)->data)
	#define DEV_CFG(dev) \
	((const struct virt_device_config *const)(dev)->config)

static const struct virtual_device_instance *rkld_device_instance;

static int vm_ld_dev_init(const struct device *dev, struct z_vm *vm, struct z_virt_dev *vdev_desc)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(vdev_desc);
	struct z_virt_dev *virt_dev;

	virt_dev = vm_virt_dev_add(
		vm, TOSTRING(linux_dev), false, false,
		RKLD_START_PHY_ADDR, RKLD_START_PHY_ADDR, RKLD_SIZE,
		VM_DEVICE_INVALID_VIRQ, VM_DEVICE_INVALID_VIRQ
	);

	if(!virt_dev) {
		ZVM_LOG_ERR("Allocat memory for linux_dev error \n");
		return -ENODEV;
	}

	strncat(virt_dev->name,"/", sizeof(vm->vm_name));
	strncat(virt_dev->name,vm->vm_name, sizeof(vm->vm_name));
	virt_dev->priv_data = rkld_device_instance;
	vm_device_irq_init(vm, virt_dev);

	return 0;
}

static int vm_ld_dev_deinit(const struct device *dev, struct z_vm *vm, struct z_virt_dev *vdev_desc)
{
	ARG_UNUSED(dev);
	vdev_desc->priv_data = NULL;
	vdev_desc->priv_vdev = NULL;
	vm_virt_dev_remove(vm, vdev_desc);
	return 0;
}

static int ld_vdev_mem_write(struct z_virt_dev *vdev, uint64_t addr, uint64_t *value, uint16_t size)
{
    return 0;
}

static int ld_vdev_mem_read(struct z_virt_dev *vdev, uint64_t addr, uint64_t *value, uint16_t size)
{
    return 0;
}

static int ld_dev_init(void)
{
	int i;

	for (i = 0; i < zvm_virtual_devices_count_get(); i++) {
		const struct virtual_device_instance *virtual_device = zvm_virtual_device_get(i);
		if(strcmp(virtual_device->name, TOSTRING(linux_dev))) {
			continue;
		}
		DEV_DATA(virtual_device)->vdevice_type |= VM_DEVICE_PRE_KERNEL_1;
		rkld_device_instance = virtual_device;
		break;
	}
	return 0;
}

static struct virt_device_config ld_dev_cfg = {
	.hirq_num = 0,
	.device_config = NULL,
};

static struct virt_device_data ld_dev_data_port = {
	.device_data = NULL,
};

static const struct virt_device_api ld_dev_api = {
	.init_fn = vm_ld_dev_init,
	.deinit_fn = vm_ld_dev_deinit,
	.virt_device_read = ld_vdev_mem_read,
	.virt_device_write = ld_vdev_mem_write,
};

ZVM_VIRTUAL_DEVICE_DEFINE(ld_dev_init,
			POST_KERNEL, CONFIG_VIRT_RK_LINUXDEV_INIT_PRIORITY,
			linux_dev,
			ld_dev_data_port,
			ld_dev_cfg,
			ld_dev_api);