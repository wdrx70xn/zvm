/*
 * Copyright 2024-2025 HNU-ESNL: Guoqi Xie, Chenglai Xiong, Xingyu Hu and etc.
 * Copyright 2024-2025 openEuler SIG-Zephyr
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/spinlock.h>
#include <zephyr/dt-bindings/interrupt-controller/arm-gic.h>
#include <zephyr/zvm/vdev/vgic_common.h>
#include <zephyr/zvm/vdev/vgic_v3.h>
#include <zephyr/zvm/vm_irq.h>
#include <zephyr/zvm/zvm.h>

LOG_MODULE_DECLARE(ZVM_MODULE_NAME);

#define VWFI_YIELD_THRESHOLD	100

bool vcpu_irq_exist(struct z_vcpu *vcpu)
{
    bool pend, active;
	struct vcpu_virt_irq_block *vb = &vcpu->virq_block;

	pend = sys_dlist_is_empty(&vb->pending_irqs);
	active = sys_dlist_is_empty(&vb->active_irqs);

    if((!(pend && active)) || virt_irq_ispending(vcpu)){
        return true;
    }

	return false;
}

int vcpu_wait_for_irq(struct z_vcpu *vcpu)
{
    bool irq_exist, vcpu_will_yeild=false, vcpu_will_pause=false;
    k_spinlock_key_t key;

    /* judge whether the vcpu has pending or active irq */
    key = k_spin_lock(&vcpu->virq_block.vwfi.wfi_lock);
    irq_exist = vcpu_irq_exist(vcpu);

    if(irq_exist){
        vcpu->virq_block.vwfi.yeild_count = 0;
        goto done;
    }else if(vcpu->virq_block.vwfi.yeild_count < VWFI_YIELD_THRESHOLD){
        vcpu->virq_block.vwfi.yeild_count++;
        vcpu_will_yeild = true;
        goto done;
    }

    if(!vcpu->virq_block.vwfi.state){
        vcpu_will_pause = true;
        vcpu->virq_block.vwfi.state = true;
        /*start wfi timeout*/
    }

done:
    k_spin_unlock(&vcpu->virq_block.vwfi.wfi_lock, key);

    if(vcpu_will_yeild){
        /*yield this thread*/
    }

    if(vcpu_will_pause){
        irq_exist = vcpu_irq_exist(vcpu);
        if(irq_exist){
            key = k_spin_lock(&vcpu->virq_block.vwfi.wfi_lock);
            vcpu->virq_block.vwfi.yeild_count = 0;
            vcpu->virq_block.vwfi.state=false;
            /*end wfi timeout*/
            k_spin_unlock(&vcpu->virq_block.vwfi.wfi_lock, key);
        }
    }

    return 0;
}

/**
 * @brief Init call for creating interrupt control block for vm.
 */
static int vm_irq_ctrlblock_create(struct device *unused, struct z_vm *vm)
{
    ARG_UNUSED(unused);
	struct vm_virt_irq_block *vvi_block = &vm->vm_irq_block;

	if (VGIC_TYPER_LR_NUM != 0) {
		vvi_block->flags = 0;
		vvi_block->flags |= VIRQ_HW_SUPPORT;
	} else {
		ZVM_LOG_ERR("Init gicv3 failed, the hardware do not supporte it. \n");
		return -ENODEV;
	}

	vvi_block->enabled = false;
	vvi_block->cpu_num = vm->vcpu_num;
	vvi_block->irq_num = VM_GLOBAL_VIRQ_NR;
	memset(vvi_block->ipi_vcpu_source, 0,
        sizeof(uint32_t) * CONFIG_MP_NUM_CPUS * VM_SGI_VIRQ_NR);
	memset(vvi_block->pt_irq_bitmap, 0, VM_GLOBAL_VIRQ_NR / 0x08);

    vvi_block->virt_priv_data = NULL;

	return 0;
}

void vm_device_irq_init(struct z_vm *vm, struct z_virt_dev *vm_dev)
{
    bool *bit_map;
    struct virt_irq_desc *desc;

	desc = get_virt_irq_desc(vm->vcpus[DEFAULT_VCPU], vm_dev->virq);
    if(vm_dev->dev_pt_flag) {
        atomic_set(&desc->bind_hwirq_flag, true);
        // desc->pirq_num = vm_dev->hirq;
    }

    desc->virq_num = vm_dev->virq;
    /* For passthrough device, using fast irq path. */
    if(vm_dev->dev_pt_flag || strstr(vm_dev->name, "ZSHMN") != NULL) {
        bit_map = vm->vm_irq_block.pt_irq_bitmap;
        bit_map[vm_dev->hirq] = true;
    }
}

int vm_irq_block_init(struct z_vm *vm)
{
    return vm_irq_ctrlblock_create(NULL, vm);
}

void virt_sync_and_flush_vcpu_irq(struct z_vcpu *vcpu)
{
    virt_sync_and_flush_vcpu_vgic_lock(vcpu);
}

bool virt_irq_isactive(struct z_vcpu *vcpu)
{
    uint32_t irq, cpu_mask;

    cpu_mask = BIT(vcpu->vcpu_id);

    for (irq = 0; irq < VM_GLOBAL_VIRQ_NR; irq++) {
        if (vgic_is_virq_active_lock(vcpu, irq, cpu_mask)) {
            return true;
        }
    }

    return false;
}

bool virt_irq_ispending(struct z_vcpu *vcpu)
{
    uint32_t irq, cpu_mask;

    cpu_mask = BIT(vcpu->vcpu_id);

    for (irq = 0; irq < VM_GLOBAL_VIRQ_NR; irq++) {
        if (vgic_is_virq_pending_lock(vcpu, irq, cpu_mask)) {
            return true;
        }
    }

    return false;
}

bool virt_irq_isenabled(struct z_vcpu *vcpu)
{
    uint32_t irq, cpu_mask;

    cpu_mask = BIT(vcpu->vcpu_id);

    for (irq = 0; irq < VM_GLOBAL_VIRQ_NR; irq++) {
        if (vgic_is_virq_enabled_lock(vcpu, irq, cpu_mask)) {
            return true;
        }
    }

    return false;
}
