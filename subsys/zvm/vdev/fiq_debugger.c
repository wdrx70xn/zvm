/*
 * Copyright 2023 HNU-ESNL
 * Copyright 2023 openEuler SIG-Zephyr
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/arch/cpu.h>
#include <zephyr/init.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/zvm/zvm.h>
#include <zephyr/zvm/vm_device.h>
#include <zephyr/zvm/vdev/fiq_debugger.h>
#include <zephyr/zvm/vm_irq.h>
#include <zephyr/zvm/vdev/vgic_common.h>

LOG_MODULE_DECLARE(ZVM_MODULE_NAME);

extern const struct device __device_start[];
extern const struct device __device_end[];

#define DEV_CFG(dev) \
	((const struct virt_device_config * const)(dev)->config)
#define DEV_DATA(dev) \
	((struct virt_device_data *)(dev)->data)

void vm_debugger_softirq_inject(void *user_data)
{
    uint32_t virq;
    int err = 0;
	const struct device *dev;
    const struct z_virt_dev *vdev = (const struct z_virt_dev *)user_data;

	if(vdev->vm->os->info.os_type != OS_TYPE_LINUX){
		return;
	}

    /* scan the host dts and get the device list */
    // Special Log Here
	// for (dev = __device_start; dev != __device_end; dev++) {
    STRUCT_SECTION_FOREACH(device, dev) {
        /**
         * Inject virq to vm by soft irq.
         */
		if (!strcmp(dev->name, DEVICE_DT_NAME(DT_ALIAS(linuxdebugger)))) {
			virq = DEV_CFG(dev)->hirq_num;
			err = set_virq_to_vm(vdev->vm, virq);
			if (err < 0) {
				ZVM_LOG_WARN("Send virq to vm error!");
			}
		}
	}

}

static int vm_fiq_debugger_init(const struct device *dev, struct z_vm *vm, struct z_virt_dev *vdev_desc)
{
	ARG_UNUSED(vdev_desc);
    uint16_t name_len;
	struct z_virt_dev *vdev;

    vdev = (struct z_virt_dev *)k_malloc(sizeof(struct z_virt_dev));
	if (!vdev) {
        ZVM_LOG_ERR("Allocate memory for vm device error!\n");
        return -ENOMEM;
    }
    name_len = strlen(dev->name);
    name_len = name_len > VIRT_DEV_NAME_LENGTH ? VIRT_DEV_NAME_LENGTH : name_len;
    strncpy(vdev->name, dev->name, name_len);
    vdev->name[name_len] = '\0';

    /* directly map irq */
    vdev->dev_pt_flag = true;
    vdev->virq = DEV_CFG(dev)->hirq_num;
    vdev->hirq = DEV_CFG(dev)->hirq_num;
    vdev->vm = vm;

    sys_dlist_append(&vm->vdev_list, &vdev->vdev_node);

    vm_device_irq_init(vm, vdev);

	vdev_irq_callback_user_data_set(dev, vm_device_callback_func, vdev);

	return 0;
}

static void virt_fiq_debugger_irq_callback_set(const struct device *dev,
						void *cb, void *user_data)
{
	/* Binding to the user's callback function and get the user data. */
	DEV_DATA(dev)->irq_cb = cb;
	DEV_DATA(dev)->irq_cb_data = user_data;
}

static const struct virt_device_api virt_fiq_debugger_api = {
	.init_fn = vm_fiq_debugger_init,
	.device_driver_api = NULL,
#ifdef CONFIG_VIRT_DEVICE_INTERRUPT_DRIVEN
    .virt_irq_callback_set = virt_fiq_debugger_irq_callback_set,
#endif
};

void virt_fiq_debugger_isr(const struct device *dev)
{
	struct virt_device_data *data = DEV_DATA(dev);
	/* Verify if the callback has been registered */
	if (data->irq_cb) {
		data->irq_cb(dev, data->irq_cb, data->irq_cb_data);
	}
}

static void fiq_debugger_irq_config_func(const struct device *dev)
{
	IRQ_CONNECT(DT_IRQN(DT_ALIAS(linuxdebugger)),
		    DT_IRQ(DT_ALIAS(linuxdebugger), priority),
		    virt_fiq_debugger_isr,
		    DEVICE_DT_GET(DT_ALIAS(linuxdebugger)),
		    0);
	irq_enable(DT_IRQN(DT_ALIAS(linuxdebugger)));
}

static struct virt_device_data virt_fiq_debugger_data_port = {
#ifdef CONFIG_VIRT_DEVICE_INTERRUPT_DRIVEN
    .irq_cb = NULL,
    .irq_cb_data = NULL,
#endif
};

static struct fiq_debugger_device_config fiq_debugger_cfg_port = {
	.irq_config_func = fiq_debugger_irq_config_func,
};

static struct virt_device_config virt_fiq_debugger_cfg = {
	.hirq_num = DT_IRQN(DT_ALIAS(linuxdebugger)),
	.device_config = &fiq_debugger_cfg_port,
};

int vm_init_bdspecific_device(struct z_vm *vm)
{
	const struct device *dev;

    /* scan the host dts and get the device list */
    // Special Log Here
	// for (dev = __device_start; dev != __device_end; dev++) {
    STRUCT_SECTION_FOREACH(device, dev) {
        /**
         * through the `init_res` to judge whether the device is
         *  ready to allocate to vm.
         */
		if (!strcmp(dev->name, DEVICE_DT_NAME(DT_ALIAS(linuxdebugger)))) {
			return vm_fiq_debugger_init(dev, vm, NULL);
		}
	}
	return -1;
}


static int fiq_debugger_init(const struct device *dev)
{
	dev->state->init_res = VM_DEVICE_INIT_RES;
	ZVM_LOG_INFO("** Ready to init fiq debugger, dev name is: %s. \n", dev->name);
	return 0;
}

DEVICE_DT_DEFINE(DT_ALIAS(linuxdebugger),
            &fiq_debugger_init,
            NULL,
            &virt_fiq_debugger_data_port,
            &virt_fiq_debugger_cfg, POST_KERNEL,
            CONFIG_VM_FIQ_DEBUGGER_INIT_PRIORITY,
            &virt_fiq_debugger_api);
