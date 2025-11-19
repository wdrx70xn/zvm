/*
 * Copyright 2024-2025 HNU-ESNL: Guoqi Xie, Chenglai Xiong, Xingyu Hu and etc.
 * Copyright 2024-2025 openEuler SIG-Zephyr
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/kernel_structs.h>
#include <zephyr/init.h>
#include <zephyr/arch/cpu.h>
#include <zephyr/arch/arm64/lib_helpers.h>
#include <zephyr/arch/common/sys_bitops.h>
#include <zephyr/dt-bindings/interrupt-controller/arm-gic.h>
#include <zephyr/drivers/interrupt_controller/gic.h>
#include <zephyr/logging/log.h>
#include <zephyr/zvm/arm/cpu.h>
#include <zephyr/zvm/vdev/vgic_common.h>
#include <zephyr/zvm/vdev/vgic_v3.h>
#include <zephyr/zvm/vdev/zshm.h>
#include <zephyr/zvm/vdev/vserial.h>
#include <zephyr/zvm/zvm.h>
#include <zephyr/zvm/vm_irq.h>
#include <zephyr/zvm/vm_device.h>
#include <zephyr/cache.h>
LOG_MODULE_DECLARE(ZVM_MODULE_NAME);

#define VM_GIC_NAME			vm_gic_v3

#define DEV_DATA(dev) \
	((struct virt_device_data *)(dev)->data)

#define DEV_CFG(dev) \
	((const struct virt_device_config * const)(dev)->config)

static const struct virtual_device_instance *gic_virtual_device_instance;

uint32_t global_virq_now;

static void vgicv3_lrs_load(struct gicv3_vcpuif_ctxt *ctxt)
{
    uint32_t rg_cout = VGIC_TYPER_LR_NUM;

    if (rg_cout > VGIC_TYPER_LR_NUM) {
        ZVM_LOG_WARN("System list registers do not support! \n");
        return;
    }

	switch (rg_cout) {
	case 8:
		write_sysreg(ctxt->ich_lr7_el2, ICH_LR7_EL2);
	case 7:
		write_sysreg(ctxt->ich_lr6_el2, ICH_LR6_EL2);
	case 6:
		write_sysreg(ctxt->ich_lr5_el2, ICH_LR5_EL2);
	case 5:
		write_sysreg(ctxt->ich_lr4_el2, ICH_LR4_EL2);
	case 4:
		write_sysreg(ctxt->ich_lr3_el2, ICH_LR3_EL2);
	case 3:
		write_sysreg(ctxt->ich_lr2_el2, ICH_LR2_EL2);
	case 2:
		write_sysreg(ctxt->ich_lr1_el2, ICH_LR1_EL2);
	case 1:
		write_sysreg(ctxt->ich_lr0_el2, ICH_LR0_EL2);
		break;
	default:
		break;
	}
}

static void vgicv3_prios_load(struct gicv3_vcpuif_ctxt *ctxt)
{
    uint32_t rg_cout = VGIC_TYPER_PRIO_NUM;

    switch (rg_cout) {
	case 7:
		write_sysreg(ctxt->ich_ap0r2_el2, ICH_AP0R2_EL2);
		write_sysreg(ctxt->ich_ap1r2_el2, ICH_AP1R2_EL2);
	case 6:
		write_sysreg(ctxt->ich_ap0r1_el2, ICH_AP0R1_EL2);
		write_sysreg(ctxt->ich_ap1r1_el2, ICH_AP1R1_EL2);
	case 5:
		write_sysreg(ctxt->ich_ap0r0_el2, ICH_AP0R0_EL2);
		write_sysreg(ctxt->ich_ap1r0_el2, ICH_AP1R0_EL2);
		break;
	default:
		ZVM_LOG_ERR("Load prs error");
	}
}

static ALWAYS_INLINE void vgicv3_ctrls_load(struct gicv3_vcpuif_ctxt *ctxt)
{
    write_sysreg(ctxt->icc_sre_el1, ICC_SRE_EL1);
	write_sysreg(ctxt->ich_vmcr_el2, ICH_VMCR_EL2);
	write_sysreg(ctxt->ich_hcr_el2, ICH_HCR_EL2);
}

static void vgicv3_lrs_save(struct gicv3_vcpuif_ctxt *ctxt)
{
	uint32_t rg_cout = VGIC_TYPER_LR_NUM;

    if (rg_cout > VGIC_TYPER_LR_NUM) {
        ZVM_LOG_WARN("System list registers do not support! \n");
        return;
    }

	switch (rg_cout) {
	case 8:
		ctxt->ich_lr7_el2 = read_sysreg(ICH_LR7_EL2);
	case 7:
		ctxt->ich_lr6_el2 = read_sysreg(ICH_LR6_EL2);
	case 6:
		ctxt->ich_lr5_el2 = read_sysreg(ICH_LR5_EL2);
	case 5:
		ctxt->ich_lr4_el2 = read_sysreg(ICH_LR4_EL2);
	case 4:
		ctxt->ich_lr3_el2 = read_sysreg(ICH_LR3_EL2);
	case 3:
		ctxt->ich_lr2_el2 = read_sysreg(ICH_LR2_EL2);
	case 2:
		ctxt->ich_lr1_el2 = read_sysreg(ICH_LR1_EL2);
	case 1:
		ctxt->ich_lr0_el2 = read_sysreg(ICH_LR0_EL2);
		break;
	default:
		break;
	}
}

static void vgicv3_lrs_init(void)
{
    uint32_t rg_cout = VGIC_TYPER_LR_NUM;

    if (rg_cout > VGIC_TYPER_LR_NUM) {
        ZVM_LOG_WARN("System list registers do not support! \n");
        return;
    }

	rg_cout = rg_cout>8 ? 8 : rg_cout;

	switch (rg_cout) {
	case 8:
		write_sysreg(0, ICH_LR7_EL2);
	case 7:
		write_sysreg(0, ICH_LR6_EL2);
	case 6:
		write_sysreg(0, ICH_LR5_EL2);
	case 5:
		write_sysreg(0, ICH_LR4_EL2);
	case 4:
		write_sysreg(0, ICH_LR3_EL2);
	case 3:
		write_sysreg(0, ICH_LR2_EL2);
	case 2:
		write_sysreg(0, ICH_LR1_EL2);
	case 1:
		write_sysreg(0, ICH_LR0_EL2);
		break;
	default:
		break;
	}
}

static void vgicv3_prios_save(struct gicv3_vcpuif_ctxt *ctxt)
{
    uint32_t rg_cout = VGIC_TYPER_PRIO_NUM;

	switch (rg_cout) {
	case 7:
		ctxt->ich_ap0r2_el2 = read_sysreg(ICH_AP0R2_EL2);
		ctxt->ich_ap1r2_el2 = read_sysreg(ICH_AP1R2_EL2);
	case 6:
		ctxt->ich_ap0r1_el2 = read_sysreg(ICH_AP0R1_EL2);
		ctxt->ich_ap1r1_el2 = read_sysreg(ICH_AP1R1_EL2);
	case 5:
		ctxt->ich_ap0r0_el2 = read_sysreg(ICH_AP0R0_EL2);
		ctxt->ich_ap1r0_el2 = read_sysreg(ICH_AP1R0_EL2);
		break;
	default:
		ZVM_LOG_ERR(" Set ich_ap priority failed. \n");
	}
}

static ALWAYS_INLINE void vgicv3_ctrls_save(struct gicv3_vcpuif_ctxt *ctxt)
{
    ctxt->icc_sre_el1 = read_sysreg(ICC_SRE_EL1);
	ctxt->ich_vmcr_el2 = read_sysreg(ICH_VMCR_EL2);
	ctxt->ich_hcr_el2 = read_sysreg(ICH_HCR_EL2);
}

static int vdev_gicv3_init(struct z_vm *vm, struct vgicv3_dev *gicv3_vdev, uint32_t gicd_base, uint32_t gicd_size,
                            uint32_t gicr_base, uint32_t gicr_size)
{
	int i, j;
    uint32_t spi_num;
    uint64_t tmp_typer = 0;
    struct virt_gic_gicd *gicd = &gicv3_vdev->gicd;
    struct virt_gic_gicr *gicr;

	gicd->gicd_base = gicd_base;
    gicd->gicd_size = gicd_size;

	/* GICD TYPER */
    spi_num = ((VM_GLOBAL_VIRQ_NR + 32) >> 5) - 1;
	tmp_typer = (vm->vcpu_num << 5) | (9 << 19) | spi_num;
	gicd->vgicd_typer = tmp_typer;
	/* GICD PIDR2 */
	gicd->vgicd_pidr2 = VGIC_V3_REV << VGIC_ARCH_REV_SHIFT;
	/* Init spinlock */
	ZVM_SPINLOCK_INIT(&gicv3_vdev->vgic_lock);

	for (i = 0; i < vm->vcpu_num; i++) {
		for (j = 0; j < VM_GLOBAL_VIRQ_NR / 32; j++) {
			gicd->virq_enabled[i][j] = 0UL;
			gicd->virq_pending[i][j] = 0UL;
			gicd->virq_active[i][j] = 0UL;
		}
	}

	for (i = 0; i < VM_GLOBAL_VIRQ_NR; i++) {
		gicd->virq_edge_trigger[i] = 0xFFFFFFFF;
	}

    for (i = 0; i < MIN(VGIC_RDIST_SIZE/VGIC_RD_SGI_SIZE, vm->vcpu_num); i++) {
        gicr = (struct virt_gic_gicr *)k_malloc(sizeof(struct virt_gic_gicr));
        if(!gicr){
			return -ENXIO;
		}
		memset(gicr, 0, sizeof(struct virt_gic_gicr));
		/* store the vcpu id for gicr */
        gicr->vcpu_id = i;

		/* init redistribute size */
		gicr->gicr_rd_size = VGIC_RD_BASE_SIZE;

		/* init sgi redistribute size */
		gicr->gicr_sgi_size = VGIC_SGI_BASE_SIZE;

		gicr->gicr_rd_base = gicr_base + VGIC_RD_SGI_SIZE * i;
		gicr->gicr_sgi_base = gicr->gicr_rd_base + VGIC_RD_BASE_SIZE;

		gicr->gicr_rd_pidr2 = VGIC_V3_REV << VGIC_ARCH_REV_SHIFT;

		gicr->virq_enabled = 0UL;
		gicr->virq_pending = 0UL;
		gicr->virq_active = 0UL;

		/* GICR TYPER */
#if defined(CONFIG_SOC_D3000)
		tmp_typer = ((uint64_t)i % 4) << 40 | ((uint64_t)i / 4) << 48 | ((uint64_t)i << 8) | 0x21;
#elif defined(CONFIG_SOC_RK3568) || defined(CONFIG_SOC_RK3588)
		tmp_typer = 1 << GICR_TYPER_LPI_AFFINITY_SHIFT | i << GICR_TYPER_PROCESSOR_NUMBER_SHIFT | (((uint64_t)i << 8) << GICR_TYPER_AFFINITY_VALUE_SHIFT);
#else
		tmp_typer = 1 << GICR_TYPER_LPI_AFFINITY_SHIFT | i << GICR_TYPER_PROCESSOR_NUMBER_SHIFT | ((uint64_t)i << GICR_TYPER_AFFINITY_VALUE_SHIFT);
#endif
		if(i >= vm->vcpu_num - 1) {
			/* set last gicr region flag here, means it is the last gicr region */
			tmp_typer |= 1 << GICR_TYPER_LAST_SHIFT;
		}
		gicr->gicr_rd_typer = tmp_typer;
		gicr->gicr_sgi_typer = tmp_typer;

		gicv3_vdev->gicr[i] = gicr;
    }

	vgicv3_lrs_init();

	vm->vm_irq_block.virt_priv_data = gicv3_vdev;

	return 0;
}

static int vdev_gicv3_deinit(struct z_vm *vm, struct vgicv3_dev *gicv3_vdev)
{
	int i = 0;
    struct virt_gic_gicr *gicr;

	for (i = 0; i < MIN(VGIC_RDIST_SIZE/VGIC_RD_SGI_SIZE, vm->vcpu_num); i++) {
		gicr = gicv3_vdev->gicr[i];
		k_free(gicr);
	}

	return 0;
}

/**
 * @brief init vm gic device for each vm. Including:
 * 1. creating virt device for vm.
 * 2. building memory map for this device.
*/
static int vm_vgicv3_init(const struct device *dev, struct z_vm *vm, struct z_virt_dev *vdev_desc)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(vdev_desc);
	int ret;
	uint32_t gicd_base, gicd_size, gicr_base, gicr_size;
	struct z_virt_dev *virt_dev;
	struct vgicv3_dev *vgicv3;

    gicd_base = VGIC_DIST_BASE;
    gicd_size = VGIC_DIST_SIZE;
    gicr_base = VGIC_RDIST_BASE;
    gicr_size = VGIC_RDIST_SIZE;
	/* check gic device */
	if(!gicd_base || !gicd_size  || !gicr_base  || !gicr_size){
        ZVM_LOG_ERR("GIC device has init error!");
        return -ENODEV;
	}

	/* Init virtual device for vm. */
	virt_dev = vm_virt_dev_add(vm, gic_virtual_device_instance->name, false, false, gicd_base,
						gicd_base, gicr_base+gicr_size-gicd_base, 0, 0);
	if(!virt_dev){
        return -ENODEV;
	}

	/* Init virtual gic device for virtual device. */
	vgicv3 = (struct vgicv3_dev *)k_malloc(sizeof(struct vgicv3_dev));
	memset(vgicv3, 0, sizeof(struct vgicv3_dev));
    if (!vgicv3) {
        ZVM_LOG_ERR("Allocat memory for vgicv3 error \n");
        return -ENODEV;
    }
    ret = vdev_gicv3_init(vm, vgicv3, gicd_base, gicd_size, gicr_base, gicr_size);
    if(ret) {
        ZVM_LOG_ERR("Init virt gicv3 error \n");
        return -ENODEV;
    }

	/* get the private data for vgicv3 */
	virt_dev->priv_data = gic_virtual_device_instance;
	virt_dev->priv_vdev = vgicv3;

	return 0;
}

static int vm_vgicv3_deinit(const struct device *dev, struct z_vm *vm, struct z_virt_dev *vdev_desc)
{
	ARG_UNUSED(dev);
	int ret;
	struct vgicv3_dev *vgicv3;

	vgicv3 = (struct vgicv3_dev *)vdev_desc->priv_vdev;
	if(!vgicv3){
		ZVM_LOG_WARN("Can not find virt gicv3 device! \n");
		return 0;
	}
	ret = vdev_gicv3_deinit(vm, vgicv3);
	if(ret){
		ZVM_LOG_WARN("Deinit virt gicv3 error \n");
		return 0;
	}
	k_free(vgicv3);

	vdev_desc->priv_vdev = NULL;
	vdev_desc->priv_data = NULL;
	ret = vm_virt_dev_remove(vm, vdev_desc);
	return ret;
}

/**
 * @brief The init function of vgic, it provides the
 * gic hardware device information to ZVM.
*/
static int virt_gic_v3_init(void)
{
	int i;

	for (i = 0; i < zvm_virtual_devices_count_get(); i++) {
		const struct virtual_device_instance *virtual_device = zvm_virtual_device_get(i);
		if(strcmp(virtual_device->name, TOSTRING(VM_GIC_NAME))){
			continue;
		}
		DEV_DATA(virtual_device)->vdevice_type |= VM_DEVICE_PRE_KERNEL_1;
		gic_virtual_device_instance = virtual_device;
		break;
	}
	return 0;
}

static struct virt_device_config virt_gicv3_cfg = {
	.hirq_num = VM_DEVICE_INVALID_VIRQ,
	.device_config = NULL,
};

static struct virt_device_data virt_gicv3_data_port = {
	.device_data = NULL,
};

/**
 * @brief vgic device operations api.
*/
static const struct virt_device_api virt_gicv3_api = {
	.init_fn = vm_vgicv3_init,
	.deinit_fn = vm_vgicv3_deinit,
	.virt_device_read = vgic_vdev_mem_read,
	.virt_device_write = vgic_vdev_mem_write,
};

ZVM_VIRTUAL_DEVICE_DEFINE(virt_gic_v3_init,
			POST_KERNEL, CONFIG_VM_VGICV3_INIT_PRIORITY,
			VM_GIC_NAME,
			virt_gicv3_data_port,
			virt_gicv3_cfg,
			virt_gicv3_api);

/*******************vgicv3 function****************************/

int vgic_inject_virq_vcpuif(struct z_vcpu *vcpu, struct virt_irq_desc *desc)
{
	int reg_id;
	uint64_t value = 0;
	struct gicv3_list_reg *lr = (struct gicv3_list_reg *)&value;

	/* List register is in the used list. */
	if (VGIC_LIST_REGS_TEST(desc->id, vcpu)) {
		value = vgicv3_read_lr(desc->id);
		if (lr->vINTID == desc->virq_num) {
			desc->virq_states |= VIRQ_STATE_PENDING;
			vgicv3_pre_set_lr(desc, lr);
			vgicv3_write_lr(desc->id, value);
			if (desc->virq_num == ZSHM_NOTIFY_VIRQ_NUM) {
				atomic_set(&desc->level_triggered, VIRQ_LEVEL_LOW);
			}
			return 1;
		}
	}

	reg_id = vgicv3_get_idle_lr(vcpu);
	if (reg_id < 0) {
		ZVM_LOG_WARN("List register reach the limits! \n");
		return -ENOSPC;
	}
	desc->id = reg_id;
	VGIC_LIST_REGS_SET(reg_id, vcpu);
	desc->virq_states = VIRQ_STATE_PENDING;
	vgicv3_pre_set_lr(desc, lr);
	vgicv3_write_lr(desc->id, value);
	if (desc->virq_num == VSERIAL_DEV_VIRQ || desc->virq_num == ZSHM_NOTIFY_VIRQ_NUM) {
		atomic_set(&desc->level_triggered, VIRQ_LEVEL_LOW);
	}
	return 0;
}

int vgicv3_raise_sgi2vcpu(struct z_vcpu *vcpu, uint64_t sgi_value)
{
	int bit, ret = 0;
	uint32_t sgi_id, sgi_mode, sgi_mask = 0;
	uint32_t target_list, aff1, aff2, aff3;
	struct virt_irq_desc *desc;
	struct z_vm *vm;

	vm = vcpu->vm;
	/*Get irq number*/
	sgi_id = (sgi_value & (0xf << 24)) >> 24;
	__ASSERT_NO_MSG(GIC_IS_SGI(sgi_id));

	sgi_mode = sgi_value & (1UL << 40) ? SGI_SIG_TO_OTHERS : SGI_SIG_TO_LIST;
	if (sgi_mode == SGI_SIG_TO_OTHERS) {
		sgi_mask = VGIC_ALL_VCPU_MASK(vm) & ~(1UL << vcpu->vcpu_id);
	} else if (sgi_mode == SGI_SIG_TO_LIST) {
		target_list = sgi_value & 0xffff;
		aff1 = (sgi_value & (uint64_t)(0xffUL << 16)) >> 16;
		aff2 = (sgi_value & (uint64_t)(0xffUL << 32)) >> 32;
		aff3 = (sgi_value & (uint64_t)(0xffUL << 48)) >> 48;
		if (aff1 || aff2 || aff3) {
			sgi_mask |= (1UL << aff1);
		} else {
			for (bit = 0; bit < vm->vcpu_num; bit++) {
				if (target_list & (1UL << bit)) {
					sgi_mask |= (1UL << bit);
				}
			}
		}
	}

	for (bit = 0; bit < vm->vcpu_num; bit++) {
		if (!(sgi_mask & (1UL << bit))) {
			continue;
		}

		desc = VGIC_GET_VIRQ_DESC(vm->vcpus[bit], sgi_id);
		if (!desc) {
			ZVM_LOG_ERR("Can not find the virq desc for vcpu %d, virq num %d.",
						vm->vcpus[bit]->vcpu_id, sgi_id);
			return -ENODEV;
		}
		/* Record the source CPU */
		desc->src_cpu |= BIT(vcpu->vcpu_id);
		ret = set_virq_to_vcpu(vm->vcpus[bit], sgi_id, 1);
		if (ret) {
			ZVM_LOG_WARN("Set sgi %d to vcpu %d failed! \n", sgi_id, bit);
			return ret;
		}
	}

	return 0;
}

int vgic_gicrsgi_mem_read(struct z_vcpu *vcpu, struct virt_gic_gicr *gicr,
                                uint32_t offset, uint64_t *v)
{
	uint32_t *value = (uint32_t *)v;

	switch (offset) {
	case GICR_SGI_ISENABLER:
	case GICR_SGI_ICENABLER:
		__VGIC_GET_ENABLE_BIT(vcpu, 0, value, 32, gicr);
		break;
	case GICR_SGI_PENDING:
	case GICR_SGI_ICPENDING:
		__VGIC_GET_PENDING_BIT(vcpu, 0, value, 32, gicr);
		break;
	case GICR_SGI_ACTIVE:
	case GICR_SGI_ICACTIVE:
		__VGIC_GET_ACTIVE_BIT(vcpu, 0, value, 32, gicr);
		break;
	case GICR_SGI_PIDR2:
		*value = (0x03 << 4);
		break;
	default:
		*value = 0;
		break;
	}
	barrier_dmem_fence_full();

	return 0;
}

int vgic_gicrsgi_mem_write(struct z_vcpu *vcpu, struct virt_gic_gicr *gicr, uint32_t offset, uint64_t *v)
{
	uint32_t *value = (uint32_t *)v;

	switch (offset) {
	case GICR_SGI_ISENABLER:
		if (ZVM_VGIC_V3_DEBUG)
			ZVM_LOG_WARN("GICR_SGI_ISENABLER write offset:0x%x, value:0x%x \n", offset, *value);
		__vgic_test_and_set_enable_bit(vcpu, 0, value, 32, 1, gicr);
		break;
	case GICR_SGI_ICENABLER:
		if (ZVM_VGIC_V3_DEBUG)
			ZVM_LOG_WARN("GICR_SGI_ICENABLER write offset:0x%x, value:0x%x \n", offset, *value);
		__vgic_test_and_set_enable_bit(vcpu, 0, value, 32, 0, gicr);
		break;
	case GICR_SGI_PENDING:
		if (ZVM_VGIC_V3_DEBUG)
			ZVM_LOG_WARN("GICR_SGI_PENDING write offset:0x%x, value:0x%x \n", offset, *value);
		__vgic_test_and_set_pending_bit(vcpu, 0, value, 32, 1, gicr);
		break;
	case GICR_SGI_ICPENDING:
		if (ZVM_VGIC_V3_DEBUG)
			ZVM_LOG_WARN("GICR_SGI_ICPENDING write offset:0x%x, value:0x%x \n", offset, *value);
		__vgic_test_and_set_pending_bit(vcpu, 0, value, 32, 0, gicr);
		break;
	case GICR_SGI_ACTIVE:
		if (ZVM_VGIC_V3_DEBUG)
			ZVM_LOG_WARN("GICR_SGI_ACTIVE write offset:0x%x, value:0x%x \n", offset, *value);
		__vgic_test_and_set_active_bit(vcpu, 0, value, 32, 1, gicr);
		break;
	case GICR_SGI_ICACTIVE:
		if (ZVM_VGIC_V3_DEBUG)
			ZVM_LOG_WARN("GICR_SGI_ICACTIVE write offset:0x%x, value:0x%x \n", offset, *value);
		__vgic_test_and_set_active_bit(vcpu, 0, value, 32, 0, gicr);
		break;
	default:
		*value = 0;
		break;
	}
	barrier_dmem_fence_full();
	barrier_dsync_fence_full();
	barrier_isync_fence_full();
	return 0;
}

int vgic_gicrrd_mem_read(struct z_vcpu *vcpu, struct virt_gic_gicr *gicr, uint32_t offset, uint64_t *v)
{
	uint64_t *value = v;

	/* consider multiple cpu later, Now just return 0 */
	switch (offset) {
	case GICR_CTLR:
		*value = gicr->gicr_rd_ctlr;
		break;
	case GICR_TYPER:
		*value = gicr->gicr_rd_typer;
		break;
	case VGICR_PIDR2:
		*value = gicr->gicr_rd_pidr2;
		break;
	default:
		*value = 0;
		break;
	}
	barrier_dmem_fence_full();

	return 0;
}

int vgic_gicrrd_mem_write(struct z_vcpu *vcpu, struct virt_gic_gicr *gicr, uint32_t offset, uint64_t *v)
{
	uint64_t *value = v;

	/* consider multiple cpu later, Now just return 0 */
	switch (offset) {
	case GICR_CTLR:
		gicr->gicr_rd_ctlr = *value;
		break;
	case GICR_TYPER:
		gicr->gicr_rd_typer = *value;
		break;
	case VGICR_PIDR2:
	default:
		break;
	}
	barrier_dmem_fence_full();

	return 0;
}

struct virt_gic_gicr* get_vcpu_gicr_type(struct z_vcpu *vcpu, struct vgicv3_dev *vgic, uint32_t addr,
											uint32_t *type,  uint32_t *offset)
{
	int i;
	struct virt_gic_gicr *gicr;

	for(i = 0; i < MIN(VGIC_RDIST_SIZE/VGIC_RD_SGI_SIZE, vcpu->vm->vcpu_num); i++) {
		gicr = vgic->gicr[i];
		if ((addr >= gicr->gicr_sgi_base) && addr < gicr->gicr_sgi_base + gicr->gicr_sgi_size) {
			*offset = addr - gicr->gicr_sgi_base;
			*type = TYPE_GIC_GICR_SGI;
			return vgic->gicr[i];
		}

		if (addr >= gicr->gicr_rd_base && addr < (gicr->gicr_rd_base + gicr->gicr_rd_size)) {
			*offset = addr - gicr->gicr_rd_base;
			*type = TYPE_GIC_GICR_RD;
			return vgic->gicr[i];
		}
	}

	*type = TYPE_GIC_INVAILD;
	return NULL;
}

int vgic_set_virq_to_vcpu(struct z_vcpu *vcpu, uint32_t virq_num, uint8_t virq_level)
{
	bool irq_pending_exist = false;
	int cpu_mask, virq_mask, i;
	uint32_t *vpending, *venabled;
	struct vgicv3_dev *vgic = (struct vgicv3_dev *)vcpu->vm->vm_irq_block.virt_priv_data;
	struct virt_irq_desc *desc;
	struct z_vcpu *cur_vcpu = _current_vcpu;
	k_spinlock_key_t key;

	global_virq_now = virq_num;

	key = k_spin_lock(&vgic->vgic_lock);

	desc = VGIC_GET_VIRQ_DESC(vcpu, virq_num);
	if (!desc) {
		ZVM_LOG_ERR("Can not find the virq desc for vcpu %d, virq num %d.",
					vcpu->vcpu_id, virq_num);
		return -ENODEV;
	}

	if (virq_num < VM_LOCAL_VIRQ_NR || virq_num == ZSHM_NOTIFY_VIRQ_NUM) {
		cpu_mask = (1 << vcpu->vcpu_id);
	} else {
		if (virq_level) {
			cpu_mask = (1 << vcpu->vcpu_id);
		} else {
			cpu_mask = VGIC_ALL_VCPU_MASK(vcpu->vm);
		}
	}

	/* 中断号没有使能 */
	if (!vgic_is_virq_enabled(vcpu, virq_num, vgic, cpu_mask)) {
		k_spin_unlock(&vgic->vgic_lock, key);
		return 0;
	}

	if (virq_num < VM_LOCAL_VIRQ_NR) {
		VGIC_SET_PENDING(vcpu, virq_num, cpu_mask);
	} else {
		if (atomic_get(&desc->level_triggered) != virq_level) {
			atomic_set(&desc->level_triggered, virq_level);
			if (virq_level) {
				VGIC_SET_PENDING(vcpu, virq_num, cpu_mask);
			}
		}
	}

	if (vcpu == cur_vcpu) {
		virt_sync_and_flush_vcpu_vgic(vcpu, vgic);
	} else {
		irq_pending_exist = VGIC_LIST_REGS_USED(vcpu) ? true : false;
		if (!irq_pending_exist) {
			virq_mask = VGIC_GET_PENDING(vcpu, 0, 32, vgic);
			virq_mask &= VGIC_GET_ENABLED(vcpu, 0, 32, vgic);
			if (virq_mask) {
				irq_pending_exist = true;
				goto irq_pending_exist_done;
			}
			vpending = vgic->gicd.virq_pending[vcpu->vcpu_id];
			venabled = vgic->gicd.virq_enabled[vcpu->vcpu_id];
			for (i = 1; i < (VM_GLOBAL_VIRQ_NR / 32); i++) {
				virq_mask = vpending[i] & venabled[i];
				if(virq_mask) {
					irq_pending_exist = true;
					break;
				}
			}
		}
	}
irq_pending_exist_done:

	k_spin_unlock(&vgic->vgic_lock, key);

	if (irq_pending_exist) {

		if (cur_vcpu == vcpu) {
			atomic_inc(&vcpu->vcpuipi_count);
			if(is_thread_active_elsewhere(vcpu->work->vcpu_thread)) {
#if defined(CONFIG_SMP) &&  defined(CONFIG_SCHED_IPI_SUPPORTED)
				arch_sched_directed_ipi(BIT(vcpu->cpu));
#endif
			} else {
				wakeup_target_vcpu(vcpu, desc);
			}
		} else {
#if defined(CONFIG_SMP) &&  defined(CONFIG_SCHED_IPI_SUPPORTED)
			atomic_inc(&vcpu->hcpuipi_count);
			arch_sched_directed_ipi(BIT(vcpu->cpu));
#endif
		}
	}
	return 0;
}

ALWAYS_INLINE int vgicv3_state_load(struct z_vcpu *vcpu, struct gicv3_vcpuif_ctxt *ctxt)
{
    vgicv3_lrs_load(ctxt);
    vgicv3_prios_load(ctxt);
    vgicv3_ctrls_load(ctxt);

	arch_hw_irq_enable(vcpu);
	return 0;
}

ALWAYS_INLINE int vgicv3_state_save(struct z_vcpu *vcpu, struct gicv3_vcpuif_ctxt *ctxt)
{
    vgicv3_lrs_save(ctxt);
    vgicv3_prios_save(ctxt);
    vgicv3_ctrls_save(ctxt);

	arch_hw_irq_disable(vcpu);
	return 0;
}

ALWAYS_INLINE int vcpu_gicv3_init(struct gicv3_vcpuif_ctxt *ctxt)
{

    ctxt->icc_sre_el1 = 0x07;
	ctxt->icc_ctlr_el1 = read_sysreg(ICC_CTLR_EL1);

    ctxt->ich_vmcr_el2 = GICH_VMCR_VENG1 | GICH_VMCR_DEFAULT_MASK;
    ctxt->ich_hcr_el2 = GICH_HCR_EN;

    return 0;
}

void save_disable_internal_int(struct z_vcpu *vcpu, bool disable_all)
{
	bool *bit_map;
	int bit;
	uint32_t internel_reg_map, gic_value;
	uint64_t paddr;
	struct vgicv3_dev *vgic;

	vgic = (struct vgicv3_dev *)vcpu->vm->vm_irq_block.virt_priv_data;
	paddr = GIC_RDIST_BASE + vcpu->cpu * VGIC_RD_SGI_SIZE + VGIC_SGI_BASE_SIZE;
	internel_reg_map = 0;
	gic_value = vgic->gicr[vcpu->vcpu_id]->virq_enabled;

	bit_map = vcpu->vm->vm_irq_block.pt_irq_bitmap;
	for(bit = 0; bit < 32; bit++) {
		if(bit_map[bit]) {
			if (vcpu->vm->is_rtos && disable_all) {
				internel_reg_map |= BIT(bit);
			} else if (!disable_all && !vcpu->vm->is_rtos) {
				internel_reg_map |= (gic_value & BIT(bit));
			}
		}
	}
	sys_write32(internel_reg_map, paddr + VGICR_ICENABLER0);
}

void load_enable_internal_int(struct z_vcpu *vcpu, bool enable_all)
{
	bool *bit_map;
	int bit;
	uint32_t internel_reg_map, gic_value;
	uint64_t paddr;
	struct vgicv3_dev *vgic;

	vgic = (struct vgicv3_dev *)vcpu->vm->vm_irq_block.virt_priv_data;
	paddr = GIC_RDIST_BASE + vcpu->cpu * VGIC_RD_SGI_SIZE + VGIC_SGI_BASE_SIZE;
	internel_reg_map = sys_read32(paddr + VGICR_ISENABLER0);
	gic_value = vgic->gicr[vcpu->vcpu_id]->virq_enabled;
	bit_map = vcpu->vm->vm_irq_block.pt_irq_bitmap;
	for(bit = 0; bit < 32; bit++) {
		if(bit_map[bit]) {
			if (vcpu->vm->is_rtos && enable_all) {
				internel_reg_map |= BIT(bit);
			} else if (!enable_all && !vcpu->vm->is_rtos) {
				internel_reg_map |= (gic_value & BIT(bit));
			}
		}
	}
	sys_write32(internel_reg_map, paddr + VGICR_ISENABLER0);
}
