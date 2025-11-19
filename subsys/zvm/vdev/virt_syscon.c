/*
 * Copyright 2024 HNU-ESNL
 * Copyright 2024 openEuler SIG-Zephyr
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Provide a common system io configuration.
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <string.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel_structs.h>
#include <zephyr/init.h>
#include <zephyr/zvm/zvm.h>
#include <zephyr/zvm/vm_device.h>
#include <zephyr/zvm/vdev/virt_syscon.h>


LOG_MODULE_DECLARE(ZVM_MODULE_NAME);

#define VIRT_SYSCON_NAME			vm_syscon

struct virt_syscon_list overall_syscons;

static bool first_entry_flag = false;
static const struct virtual_device_instance *syscon_virtual_device_instance;

#define DEV_DATA(dev) \
	((struct virt_device_data *)(dev)->data)

int z_info_syscon(const char *name, uint32_t addr_base,
                              uint32_t addr_size, void *priv)
{
    ARG_UNUSED(priv);
	k_spinlock_key_t key;
	struct virt_syscon *syscon;

	syscon = (struct virt_syscon *)k_malloc(sizeof(struct virt_syscon));
    if (!syscon) {
        ZVM_LOG_ERR("Allocate memory for virt syscon Error.\n");
        return -ENOMEM;
    }
	memset(syscon, 0, sizeof(struct virt_syscon));

	syscon->reg_base = addr_base;
	syscon->reg_size = addr_size;
	syscon->name = name;

	/* Add clock syscon info to overall lists. */
	key = k_spin_lock(&overall_syscons.vsyscon_lock);
	if(!first_entry_flag){
		sys_dlist_init(&overall_syscons.syscon_list);
		first_entry_flag = true;
	}
	sys_dlist_append(&overall_syscons.syscon_list, &syscon->syscon_node);
	overall_syscons.syscon_num++;
	k_spin_unlock(&overall_syscons.vsyscon_lock, key);

	return 0;
}

/**
 * @brief: Get the virtual io configuration controller.
*/
static int virt_syscon_init(void)
{
	int i;

	for (i = 0; i < zvm_virtual_devices_count_get(); i++) {
		const struct virtual_device_instance *virtual_device = zvm_virtual_device_get(i);
		if(strcmp(virtual_device->name, TOSTRING(VIRT_SYSCON_NAME))){
			continue;
		}
		DEV_DATA(virtual_device)->vdevice_type |= VM_DEVICE_PRE_KERNEL_1;
		syscon_virtual_device_instance = virtual_device;
		ZVM_LOG_INFO("Init virt syscon for ZVM successful.\n");
		break;
	}
	return 0;
}


static int syscon_vdev_mem_read(struct z_virt_dev *vdev, uint64_t addr, uint64_t *value, uint16_t size)
{
	uint32_t read_value;
	read_value = sys_read32(addr);
	*(uint32_t *)value = read_value;

	return 0;
}

static int syscon_vdev_mem_write(struct z_virt_dev *vdev, uint64_t addr, uint64_t *value, uint16_t size)
{
	uint32_t be_write_value, write_value, af_write_value;

	write_value = *(uint32_t *)value;
	be_write_value = sys_read32(addr);
//	sys_write32(write_value, addr);
	af_write_value = sys_read32(addr);

	// ZVM_LOG_INFO("%s Write: addr is %llx, be_value is %x, wt_value is %x, af_value is %x\n",
	// 		 vdev->name, addr, be_write_value, write_value, af_write_value);

	return 0;
}

/**
 * @brief init vm syscon for each vm.
*/
static int vm_syscons_init(const struct device *dev, struct z_vm *vm, struct z_virt_dev *vdev_desc)
{
	ARG_UNUSED(dev);
	struct z_virt_dev *vm_dev;
	struct virt_syscon *virt_syscon_dev;
	struct  _dnode *d_node, *ds_node;

	/* scan the system clock list and get the clock device */
	SYS_DLIST_FOR_EACH_NODE_SAFE(&overall_syscons.syscon_list, d_node, ds_node){
		virt_syscon_dev = CONTAINER_OF(d_node, struct virt_syscon, syscon_node);
		vm_dev = vm_virt_dev_add(vm, virt_syscon_dev->name, false, false,
								virt_syscon_dev->reg_base, virt_syscon_dev->reg_base,
								virt_syscon_dev->reg_size, VM_DEVICE_INVALID_VIRQ,
								VM_DEVICE_INVALID_VIRQ);
		if(!vm_dev){
        	return -ENODEV;
		}
		vm_dev->priv_data = syscon_virtual_device_instance;
	}

	return 0;
}

/**
 * @brief deinit vm syscon for each vm.
*/
static int vm_syscons_deinit(const struct device *dev, struct z_vm *vm, struct z_virt_dev *vdev_desc)
{
	ARG_UNUSED(dev);
	vdev_desc->priv_data = NULL;
	vdev_desc->priv_vdev = NULL;
	vm_virt_dev_remove(vm, vdev_desc);
	return 0;
}

static struct virt_device_config virt_syscon_cfg = {
	.hirq_num = VM_DEVICE_INVALID_VIRQ,
};

static struct virt_device_data virt_syscon_data_port = {
	.device_data = NULL,
};

static const struct virt_device_api virt_syscon_api = {
	.init_fn = vm_syscons_init,
	.deinit_fn = vm_syscons_deinit,
	.virt_device_read = syscon_vdev_mem_read,
	.virt_device_write = syscon_vdev_mem_write,
};


/* Init after soc's configuration syscon.*/
ZVM_VIRTUAL_DEVICE_DEFINE(virt_syscon_init,
			POST_KERNEL, CONFIG_VM_SYSCON_CONTROLLER_INIT_PRIORITY,
			VIRT_SYSCON_NAME,
			virt_syscon_data_port,
			virt_syscon_cfg,
			virt_syscon_api);