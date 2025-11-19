/*
 * Copyright 2023-2024 HNU-ESNL
 * Copyright 2023-2024 openEuler SIG-Zephyr
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Provide a common clock controller for each vm to
 * operate system clock.
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <string.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel_structs.h>
#include <zephyr/init.h>
#include <zephyr/zvm/zvm.h>
#include <zephyr/zvm/vdev/virt_clock_syscon.h>
#include <zephyr/zvm/vm_device.h>

LOG_MODULE_DECLARE(ZVM_MODULE_NAME);

#define VM_CLKCON_NAME			vm_clock_syscon

#define DEV_DATA(dev) \
	((struct virt_device_data *)(dev)->data)

struct virt_clk_syscon_list overall_clock_syscons;

static const struct virtual_device_instance *clk_virtual_device_instance;

static bool first_entry_flag = false;

int z_info_sys_clkcon(const char *name, uint32_t addr_base,
                              uint32_t addr_size, void *priv)
{
    ARG_UNUSED(priv);
	k_spinlock_key_t key;
	struct virt_clk_syscon *clk_syscon;

	clk_syscon = (struct virt_clk_syscon *)k_malloc(sizeof(struct virt_clk_syscon));
    if (!clk_syscon) {
        ZVM_LOG_ERR("Allocate memory for clk_syscon Error.\n");
        return -ENOMEM;
    }
	memset(clk_syscon, 0 ,sizeof(struct virt_clk_syscon));

	clk_syscon->reg_base = addr_base;
	clk_syscon->reg_size = addr_size;
	clk_syscon->name = name;

	/* Add clock syscon info to overall lists. */
	key = k_spin_lock(&overall_clock_syscons.vclk_lock);
	/*dlist must be init to address to itself.*/
	if(!first_entry_flag){
		sys_dlist_init(&overall_clock_syscons.clk_syscon_list);
		first_entry_flag = true;
	}
	sys_dlist_append(&overall_clock_syscons.clk_syscon_list, &clk_syscon->clk_node);
	overall_clock_syscons.clk_num++;
	k_spin_unlock(&overall_clock_syscons.vclk_lock, key);

	return 0;
}

/**
 * @brief: Get the virtual clock controller, and set a
 * pre_kernel configuration for the virtual clock.
*/
static int virt_clock_syscon_init(void)
{
	int i;

	for (i = 0; i < zvm_virtual_devices_count_get(); i++) {
		const struct virtual_device_instance *virtual_device = zvm_virtual_device_get(i);
		if(strcmp(virtual_device->name, TOSTRING(VM_CLKCON_NAME))){
			continue;
		}
		DEV_DATA(virtual_device)->vdevice_type |= VM_DEVICE_PRE_KERNEL_1;
		clk_virtual_device_instance = virtual_device;
		ZVM_LOG_INFO("Init virt clock syscon for ZVM successful.\n");
		break;
	}
	return 0;
}

static struct virt_device_config virt_clock_syscon_cfg = {
	.hirq_num = VM_DEVICE_INVALID_VIRQ,
};

static struct virt_device_data virt_clock_syscon_data_port = {
	.device_data = NULL,
};

/**
 * @brief init vm clock syscon device for each vm.
*/
static int vm_clock_syscons_init(const struct device *dev, struct z_vm *vm, struct z_virt_dev *vdev_desc)
{
	ARG_UNUSED(dev);
	struct z_virt_dev *vm_dev;
	struct virt_clk_syscon *virt_clk_dev;
	struct  _dnode *d_node, *ds_node;

	/* scan the system clock list and get the clock device */
	SYS_DLIST_FOR_EACH_NODE_SAFE(&overall_clock_syscons.clk_syscon_list, d_node, ds_node) {
		virt_clk_dev = CONTAINER_OF(d_node, struct virt_clk_syscon, clk_node);
		vm_dev = vm_virt_dev_add(vm, virt_clk_dev->name, false, false,
								virt_clk_dev->reg_base, virt_clk_dev->reg_base,
								virt_clk_dev->reg_size, VM_DEVICE_INVALID_VIRQ,
								VM_DEVICE_INVALID_VIRQ);
		if(!vm_dev){
        	return -ENODEV;
		}
		vm_dev->priv_data = clk_virtual_device_instance;
	}

	return 0;
}

/**
 * @brief deinit vm clock syscon device for each vm.
*/
static int vm_clock_syscons_deinit(const struct device *dev, struct z_vm *vm, struct z_virt_dev *vdev_desc)
{
	ARG_UNUSED(dev);
	vdev_desc->priv_data = NULL;
	vdev_desc->priv_vdev = NULL;
	vm_virt_dev_remove(vm, vdev_desc);
	return 0;
}

static int clk_syscon_vdev_mem_read(struct z_virt_dev *vdev, uint64_t addr, uint64_t *value, uint16_t size)
{
	uint32_t read_value;
	read_value = sys_read32(addr);
	*(uint32_t *)value = read_value;

	// ZVM_LOG_INFO("%s Read:addr is %llx, value is %x\n", vdev->name, addr, read_value);

	return 0;
}

static int clk_syscon_vdev_mem_write(struct z_virt_dev *vdev, uint64_t addr, uint64_t *value, uint16_t size)
{
	// uint32_t be_write_value, write_value, af_write_value;

	// write_value = *(uint32_t *)value;
	// be_write_value = sys_read32(addr);
	// sys_write32(write_value, addr);
	// af_write_value = sys_read32(addr);

	// ZVM_LOG_INFO("%s Write: addr is %llx, be_value is %x, ne_value is %x, af_value is %x\n",
	// 		 vdev->name, addr, be_write_value, write_value, af_write_value);

	return 0;
}

static const struct virt_device_api virt_clock_syscon_api = {
	.init_fn = vm_clock_syscons_init,
	.deinit_fn = vm_clock_syscons_deinit,
	.virt_device_read = clk_syscon_vdev_mem_read,
	.virt_device_write = clk_syscon_vdev_mem_write,
};

/* Init after soc's clock syscon. */
ZVM_VIRTUAL_DEVICE_DEFINE(virt_clock_syscon_init,
			POST_KERNEL, CONFIG_VM_CLOCK_SYSTEM_CONTROLLER_INIT_PRIORITY,
			VM_CLKCON_NAME,
			virt_clock_syscon_data_port,
			virt_clock_syscon_cfg,
			virt_clock_syscon_api);
