
#include <zephyr/kernel.h>
#include <zephyr/arch/cpu.h>
#include <zephyr/init.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/zvm/zvm.h>
#include <zephyr/zvm/vm.h>
#include <zephyr/zvm/vm_device.h>
#include <zephyr/zvm/vm_irq.h>
#include <zephyr/zvm/vdev/pt_device.h>

LOG_MODULE_DECLARE(ZVM_MODULE_NAME);

void __weak vm_debugger_softirq_inject(void *user_data)
{
	ARG_UNUSED(user_data);
}

#define DEV_CFG(dev) \
	((const struct virt_device_config * const) \
    (dev)->config)
#define DEV_DATA(dev) \
	((struct virt_device_data *)(dev)->data)

#define PTDEV_CFG(dev) \
	((const struct pass_through_device_config * const) \
	(DEV_CFG(dev)->device_config))

/*Device init function when system bootup. */
static int pass_through_device_init(const struct device *dev)
{
	/*set device type to res.*/
	dev->state->init_res |= VM_DEVICE_INIT_RES;

	/*init the configuration for interrupt.*/
	if(PTDEV_CFG(dev)->irq_config_func){
		PTDEV_CFG(dev)->irq_config_func(dev);
	}

	ZVM_LOG_INFO("PT-DEVICE: Initialized pass-through device: %s. \n", dev->name);
	ZVM_LOG_INFO("Paddr: %x, size: %x, hirq: %d. \n",
		DEV_CFG(dev)->reg_base, DEV_CFG(dev)->reg_size, DEV_CFG(dev)->hirq_num);
	return 0;
}

static int vm_ptdevice_init(const struct device *dev, struct z_vm *vm, struct z_virt_dev *vdev_desc)
{
	struct z_virt_dev *vdev;

    vdev = allocate_device_to_vm(dev, vm, vdev_desc, true, false);
	vdev->priv_data = dev;
	if(!vdev){
		ZVM_LOG_ERR("Init virt pass-through device error\n");
        return -ENODEV;
	}

	DEV_DATA(dev)->device_data = vdev;

	/*set special function for vm device init*/
	if(PTDEV_CFG(dev)->ptdev_spec_init_func){
		PTDEV_CFG(dev)->ptdev_spec_init_func(vdev);
	}

	return 0;
}

static int vm_ptdevice_deinit(const struct device *dev, struct z_vm *vm, struct z_virt_dev *vdev_desc)
{
    vm_virt_dev_remove(vm,vdev_desc);

	return 0;
}

static void pass_through_device_isr(const struct device *dev)
{
	/*irq handler.*/
	if(DEV_DATA(dev)->device_data){
		vm_device_callback_func(dev, NULL, DEV_DATA(dev)->device_data);
	} else{
		ZVM_LOG_ERR("irq handle error, vdev is NULL, please check the device: %s\n", dev->name);
	}

    /*set special function for vm device irq route*/
	if(PTDEV_CFG(dev)->ptdev_spec_irq_func) {
		PTDEV_CFG(dev)->ptdev_spec_irq_func(DEV_DATA(dev)->device_data);
	}
}

static const struct virt_device_api virt_ptdevice_api = {
	.init_fn = vm_ptdevice_init,
};


/**
 * Add pass-through instance for each device. just need edit below
 * function.
 */

/*---------------------Date for each pt device---------------------------*/
/*irq exist? set this configuration.*/
static void ptdevice_irq_config_func_3(const struct device *dev)
{
	IRQ_CONNECT(DT_IRQN(DT_ALIAS(ptdevice3)),
		    DT_IRQ(DT_ALIAS(ptdevice3), priority),
		    pass_through_device_isr,
		    DEVICE_DT_GET(DT_ALIAS(ptdevice3)),
		    0);
}
/*irq exist? if not, set function NULL.*/
static struct pass_through_device_config ptdevice_cfg_port_3 = {
	.irq_config_func = ptdevice_irq_config_func_3,
	.ptdev_spec_init_func = NULL,
	.ptdev_spec_irq_func = NULL,
};

static struct virt_device_config virt_ptdevice_cfg_3 = {
	.reg_base = DT_REG_ADDR(DT_ALIAS(ptdevice3)),
	.reg_size = DT_REG_SIZE(DT_ALIAS(ptdevice3)),
	.hirq_num = DT_IRQN(DT_ALIAS(ptdevice3)),
	.device_config = &ptdevice_cfg_port_3,
};

static struct virt_device_data virt_ptdevice_data_port_3 = {
	.device_data = NULL,
};

DEVICE_DT_DEFINE(DT_ALIAS(ptdevice3),
            &pass_through_device_init,
            NULL, &virt_ptdevice_data_port_3, &virt_ptdevice_cfg_3,
			POST_KERNEL,
            CONFIG_SERIAL_INIT_PRIORITY,
            &virt_ptdevice_api);
/*------------------------cut line---------------------------------------*/

/*---------------------Date for each pt device---------------------------*/
/*irq exist? set this configuration.*/
#ifndef CONFIG_VM_VSERIAL
static void ptdevice_irq_config_func_2(const struct device *dev)
{
	IRQ_CONNECT(DT_IRQN(DT_ALIAS(ptdevice2)),
		    DT_IRQ(DT_ALIAS(ptdevice2), priority),
		    pass_through_device_isr,
		    DEVICE_DT_GET(DT_ALIAS(ptdevice2)),
		    0);
}
/*irq exist? if not, set function NULL.*/
static struct pass_through_device_config ptdevice_cfg_port_2 = {
	.irq_config_func = ptdevice_irq_config_func_2,
	.ptdev_spec_init_func = NULL,
	.ptdev_spec_irq_func = NULL,
};

static struct virt_device_config virt_ptdevice_cfg_2 = {
	.reg_base = DT_REG_ADDR(DT_ALIAS(ptdevice2)),
	.reg_size = DT_REG_SIZE(DT_ALIAS(ptdevice2)),
	.hirq_num = DT_IRQN(DT_ALIAS(ptdevice2)),
	.device_config = &ptdevice_cfg_port_2,
};

static struct virt_device_data virt_ptdevice_data_port_2 = {
	.device_data = NULL,
};

DEVICE_DT_DEFINE(DT_ALIAS(ptdevice2),
            &pass_through_device_init,
            NULL, &virt_ptdevice_data_port_2, &virt_ptdevice_cfg_2,
			POST_KERNEL,
            CONFIG_SERIAL_INIT_PRIORITY,
            &virt_ptdevice_api);
/*------------------------cut line---------------------------------------*/

/*---------------------Date for each pt device---------------------------*/
/*irq exist? set this configuration.*/
static void ptdevice_irq_config_func_1(const struct device *dev)
{
	IRQ_CONNECT(DT_IRQN(DT_ALIAS(ptdevice1)),
		    DT_IRQ(DT_ALIAS(ptdevice1), priority),
		    pass_through_device_isr,
		    DEVICE_DT_GET(DT_ALIAS(ptdevice1)),
		    0);
	irq_enable(DT_IRQN(DT_ALIAS(ptdevice1)));
}
/*irq exist? if not, set function NULL.*/
static struct pass_through_device_config ptdevice_cfg_port_1 = {
	.irq_config_func = ptdevice_irq_config_func_1,
	.ptdev_spec_init_func = NULL,
	.ptdev_spec_irq_func = vm_debugger_softirq_inject,
};

static struct virt_device_config virt_ptdevice_cfg_1 = {
	.reg_base = DT_REG_ADDR(DT_ALIAS(ptdevice1)),
	.reg_size = DT_REG_SIZE(DT_ALIAS(ptdevice1)),
	.hirq_num = DT_IRQN(DT_ALIAS(ptdevice1)),
	.device_config = &ptdevice_cfg_port_1,
};

static struct virt_device_data virt_ptdevice_data_port_1 = {
	.device_data = NULL,
};

DEVICE_DT_DEFINE(DT_ALIAS(ptdevice1),
            &pass_through_device_init,
            NULL, &virt_ptdevice_data_port_1, &virt_ptdevice_cfg_1,
			POST_KERNEL, CONFIG_SERIAL_INIT_PRIORITY,
            &virt_ptdevice_api);
#endif
/*------------------------cut line---------------------------------------*/