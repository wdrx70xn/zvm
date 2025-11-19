/*
 * Copyright 2024-2025 HNU-ESNL: Guoqi Xie, Chenglai Xiong, Xingyu Hu and etc.
 * Copyright 2024-2025 openEuler SIG-Zephyr
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/irq.h>
#include <zephyr/arch/cpu.h>
#include <zephyr/arch/arm64/lib_helpers.h>
#include <zephyr/dt-bindings/interrupt-controller/arm-gic.h>
#include <../drivers/interrupt_controller/intc_gicv3_priv.h>
#include <../kernel/include/ksched.h>
#include <zephyr/zvm/arm/cpu.h>
#include <zephyr/zvm/vdev/vgic_common.h>
#include <zephyr/zvm/vdev/vgic_v3.h>
#include <zephyr/zvm/vm_irq.h>

LOG_MODULE_DECLARE(ZVM_MODULE_NAME);

#define VGIC_LOCAL_IRQ_BASE		0
#define VGIC_EXT_IRQ_BASE		32

extern uint32_t global_virq_now;

static int virt_irq_set_type(struct z_vcpu *vcpu, uint32_t offset, uint32_t *value)
{
	uint8_t lowbit_value;
	int i, irq, idx_base;
	struct virt_irq_desc *desc;

	idx_base = (offset - GICD_ICFGRn) / 4;
	irq = 16 * idx_base;

	/**
	 * Per-register control 16 interrupt signals.
	 * TODO: This may be more simple for reduce
	 * time.
	*/
	for (i = 0; i < 16; i++, irq++) {
		desc = VGIC_GET_VIRQ_DESC(vcpu, irq);
		if (!desc) {
			return -ENOENT;
		}
		lowbit_value = (*value >> 2 * i) & GICD_ICFGR_MASK;
		if (desc->type != lowbit_value) {
			desc->type = lowbit_value;
			/* If it is a hardware device interrupt */
		}
	}
	return 0;
}

/**
 * @breif: this type value is got from desc.
 * TODO: may be direct read from vgic register.
*/
static int virt_irq_get_type(struct z_vcpu *vcpu, uint32_t offset, uint32_t *value)
{
	int i, irq, idx_base;
	struct virt_irq_desc *desc;

	desc = NULL;
	idx_base = (offset - GICD_ICFGRn) / 4;
	irq = 16 * idx_base;

	/*Per-register control 16 interrupt signals.*/
	for (i = 0; i < 16; i++, irq++) {
		desc = VGIC_GET_VIRQ_DESC(vcpu, irq);
		if(!desc) {
			continue;
		}
		*value = *value | (desc->type << i * 2);
	}
	return 0;
}

static int vgic_gicd_mem_read(struct z_vcpu *vcpu, struct virt_gic_gicd *gicd,
                                uint32_t offset, uint64_t *v)
{
	bool set_flag = false;
	int i;
	uint32_t *value;
	uint32_t cpu_mask, reg_index, irq_base, irq;

	value = (uint32_t *)v;

	offset += GIC_DIST_BASE;
	switch (offset) {
		case GICD_CTLR:
			*value = gicd->vgicd_ctlr & ~(1 << 31);
			break;
		case GICD_TYPER:
			*value = gicd->vgicd_typer;
			break;
		case GICD_IIDR:
		case GICD_STATUSR:
			break;
		case GICD_ISENABLERn...(GICD_ICENABLERn - 1):
			cpu_mask = 1 << vcpu->vcpu_id;
			reg_index = (offset - GICD_ISENABLERn) / 4;
			irq_base = reg_index * 32;
			if (irq_base >= VM_GLOBAL_VIRQ_NR) {
				ZVM_LOG_WARN("vgic_gicd_mem_read isenable: irq_base %d exceeds the limit of irq num, vcpu: %d",
					irq_base, vcpu->vcpu_id);
				break;
			}
			for (i = 0; i < 32; i++) {
				irq = irq_base + i;
				set_flag = vgic_is_virq_enabled(vcpu, irq, vcpu->vm->vm_irq_block.virt_priv_data, cpu_mask);
				if (irq < VM_GLOBAL_VIRQ_NR && set_flag) {
					*value |= BIT(i);
				}
			}
			break;
		case GICD_ICENABLERn...(GICD_ISPENDRn - 1):
			cpu_mask = 1 << vcpu->vcpu_id;
			reg_index = (offset - GICD_ICENABLERn) / 4;
			irq_base = reg_index * 32;
			if (irq_base >= VM_GLOBAL_VIRQ_NR) {
				ZVM_LOG_WARN("vgic_gicd_mem_read icenable: irq_base %d exceeds the limit of irq num, vcpu: %d",
					irq_base, vcpu->vcpu_id);
				break;
			}
			for (i = 0; i < 32; i++) {
				irq = irq_base + i;
				set_flag = vgic_is_virq_enabled(vcpu, irq, vcpu->vm->vm_irq_block.virt_priv_data, cpu_mask);
				if (irq < VM_GLOBAL_VIRQ_NR && set_flag) {
					*value |= BIT(i);
				}
			}
			break;
		case GICD_ISPENDRn...(GICD_ICPENDRn - 1):
			cpu_mask = 1 << vcpu->vcpu_id;
			reg_index = (offset - GICD_ISPENDRn) / 4;
			irq_base = reg_index * 32;
			if (irq_base >= VM_GLOBAL_VIRQ_NR) {
				ZVM_LOG_WARN("vgic_gicd_mem_read ispend: irq_base %d exceeds the limit of irq num, vcpu: %d",
					irq_base, vcpu->vcpu_id);
				break;
			}
			for (i = 0; i < 32; i++) {
				irq = irq_base + i;
				set_flag = vgic_is_virq_pending(vcpu, irq, vcpu->vm->vm_irq_block.virt_priv_data, cpu_mask);
				if (irq < VM_GLOBAL_VIRQ_NR && set_flag) {
					*value |= BIT(i);
				}
			}
			break;
		case GICD_ICPENDRn...(GICD_ISACTIVERn - 1):
			cpu_mask = 1 << vcpu->vcpu_id;
			reg_index = (offset - GICD_ICPENDRn) / 4;
			irq_base = reg_index * 32;
			if (irq_base >= VM_GLOBAL_VIRQ_NR) {
				ZVM_LOG_WARN("vgic_gicd_mem_read icpend: irq_base %d exceeds the limit of irq num, vcpu: %d",
					irq_base, vcpu->vcpu_id);
				break;
			}
			for (i = 0; i < 32; i++) {
				irq = irq_base + i;
				set_flag = vgic_is_virq_pending(vcpu, irq, vcpu->vm->vm_irq_block.virt_priv_data, cpu_mask);
				if (irq < VM_GLOBAL_VIRQ_NR && set_flag) {
					*value |= BIT(i);
				}
			}
			break;
		case GICD_ISACTIVERn...((GIC_DIST_BASE + 0x380) - 1):
			reg_index = (offset - GICD_ISACTIVERn) / 4;
			irq_base = reg_index * 32;
			cpu_mask = irq_base ? VGIC_ALL_VCPU_MASK(vcpu->vm) : (1 << vcpu->vcpu_id);
			if (irq_base >= VM_GLOBAL_VIRQ_NR) {
				ZVM_LOG_WARN("vgic_gicd_mem_read isactive: irq_base %d exceeds the limit of irq num, vcpu: %d",
					irq_base, vcpu->vcpu_id);
				break;
			}
			for (i = 0; i < 32; i++) {
				irq = irq_base + i;
				set_flag = vgic_is_virq_active(vcpu, irq, vcpu->vm->vm_irq_block.virt_priv_data, cpu_mask);
				if (irq < VM_GLOBAL_VIRQ_NR && set_flag) {
					*value |= BIT(i);
				}
			}
			break;
		case (GIC_DIST_BASE + 0x380)...(GICD_IPRIORITYRn - 1):
			cpu_mask = 1 << vcpu->vcpu_id;
			reg_index = (offset - (GIC_DIST_BASE + 0x380)) / 4;
			irq_base = reg_index * 32;
			if (irq_base >= VM_GLOBAL_VIRQ_NR) {
				ZVM_LOG_WARN("vgic_gicd_mem_read ipriority: irq_base %d exceeds the limit of irq num, vcpu: %d",
					irq_base, vcpu->vcpu_id);
				break;
			}
			for (i = 0; i < 32; i++) {
				irq = irq_base + i;
				set_flag = vgic_is_virq_active(vcpu, irq, vcpu->vm->vm_irq_block.virt_priv_data, cpu_mask);
				if (irq < VM_GLOBAL_VIRQ_NR && set_flag) {
					*value |= BIT(i);
				}
			}
			break;
		case GICD_IPRIORITYRn...(GICD_ICFGRn - 1):
			irq_base = ((offset - GICD_IPRIORITYRn) / 4) * 4;
			if (irq_base >= VM_GLOBAL_VIRQ_NR) {
				ZVM_LOG_WARN("Invalid virt irq number for prio: %d for VM: %d, max: %d \n",
						irq_base, vcpu->vm->vmid, VM_GLOBAL_VIRQ_NR);
				break;
			}
			__vgic_set_and_get_priority_bit(vcpu, irq_base, value, 32, 0, gicd);
			break;
		case GICD_ICFGRn...(GIC_DIST_BASE + 0x0cfc - 1):
			virt_irq_get_type(vcpu, offset, value);
			break;
		case (GIC_DIST_BASE + VGICD_PIDR2):
			*value = gicd->vgicd_pidr2;
			break;
		case (GIC_DIST_BASE + VGIC_RESERVED)...(GIC_DIST_BASE + VGIC_INMIRn - 1):
		default:
			break;
	}

	return 0;
}

static int vgic_gicd_mem_write(struct z_vcpu *vcpu, struct virt_gic_gicd *gicd,
                                uint32_t offset, uint64_t *v)
{
	uint32_t x, y, i;
	uint32_t *value = (uint32_t *)v;

	offset += GIC_DIST_BASE;
	switch (offset) {
		case GICD_CTLR:
			gicd->vgicd_ctlr = *value;
			break;
		case GICD_TYPER:
		case GICD_IIDR:
		case GICD_STATUSR:
			break;
		case GICD_ISENABLERn...(GICD_ICENABLERn - 1):
			x = (offset - GICD_ISENABLERn) / 4;
			y = x * 32;
			if (y >= VM_GLOBAL_VIRQ_NR) {
				ZVM_LOG_WARN("Invalid virt irq number: %d for VM: %d, max: %d \n",
							y, vcpu->vm->vmid, VM_GLOBAL_VIRQ_NR);
				return -EINVAL;
			}
			if (ZVM_VGIC_V3_DEBUG)
				ZVM_LOG_WARN("vgic_gicd_mem_write isenable: x %d, y %d, value %x, vcpu: %d \n",
						x, y, *value, vcpu->vcpu_id);
			/* Set the enable bit for all vcpus in the vm */
			for (i = 0; i < vcpu->vm->vcpu_num; i++) {
				__vgic_test_and_set_enable_bit(vcpu->vm->vcpus[i], y, value, 32, 1, gicd);
			}
			break;
		case GICD_ICENABLERn...(GICD_ISPENDRn - 1):
			x = (offset - GICD_ICENABLERn) / 4;
			y = x * 32;
			if (y >= VM_GLOBAL_VIRQ_NR) {
				break;
			}
			if (ZVM_VGIC_V3_DEBUG)
				ZVM_LOG_WARN("vgic_gicd_mem_write icenable: x %d, y %d, value %x, vcpu: %d \n",
						x, y, *value, vcpu->vcpu_id);
			for (i = 0; i < vcpu->vm->vcpu_num; i++) {
				__vgic_test_and_set_enable_bit(vcpu->vm->vcpus[i], y, value, 32, 0, gicd);
			}
			break;
		case GICD_ISPENDRn...(GICD_ICPENDRn - 1):
			x = (offset - GICD_ISPENDRn) / 4;
			y = x * 32;
			if (y >= VM_GLOBAL_VIRQ_NR) {
				ZVM_LOG_WARN("Invalid virt irq number: %d for VM: %d, max: %d \n",
							y, vcpu->vm->vmid, VM_GLOBAL_VIRQ_NR);
				return -EINVAL;
			}
			if (ZVM_VGIC_V3_DEBUG)
				ZVM_LOG_WARN("vgic_gicd_mem_write ispend: x %d, y %d, value %x, vcpu: %d \n",
						x, y, *value, vcpu->vcpu_id);
			for (i = 0; i < vcpu->vm->vcpu_num; i++) {
				__vgic_test_and_set_pending_bit(vcpu->vm->vcpus[i], y, value, 32, 1, gicd);
			}
			break;
		case GICD_ICPENDRn...(GICD_ISACTIVERn - 1):
			x = (offset - GICD_ICPENDRn) / 4;
			y = x * 32;
			if (y >= VM_GLOBAL_VIRQ_NR) {
				break;
			}
			if (ZVM_VGIC_V3_DEBUG)
				ZVM_LOG_WARN("vgic_gicd_mem_write icpend: x %d, y %d, value %x, vcpu: %d \n",
						x, y, *value, vcpu->vcpu_id);
			for (i = 0; i < vcpu->vm->vcpu_num; i++) {
				__vgic_test_and_set_pending_bit(vcpu->vm->vcpus[i], y, value, 32, 0, gicd);
			}
			break;
		case GICD_ISACTIVERn...((GIC_DIST_BASE + 0x380) - 1):
			x = (offset - GICD_ISACTIVERn) / 4;
			y = x * 32;
			if (y >= VM_GLOBAL_VIRQ_NR) {
				ZVM_LOG_WARN("Invalid virt irq number: %d for VM: %d, max: %d \n",
							y, vcpu->vm->vmid, VM_GLOBAL_VIRQ_NR);
				return -EINVAL;
			}
			if (ZVM_VGIC_V3_DEBUG)
				ZVM_LOG_WARN("vgic_gicd_mem_write isactive: x %d, y %d, value %x, vcpu: %d \n",
						x, y, *value, vcpu->vcpu_id);
			for (i = 0; i < vcpu->vm->vcpu_num; i++) {
				__vgic_test_and_set_active_bit(vcpu->vm->vcpus[i], y, value, 32, 1, gicd);
			}
			break;
		case (GIC_DIST_BASE + 0x380)...(GICD_IPRIORITYRn - 1):
			x = (offset - (GIC_DIST_BASE + 0x380)) / 4;
			y = x * 32;
			if (y >= VM_GLOBAL_VIRQ_NR) {
				break;
			}
			if (ZVM_VGIC_V3_DEBUG)
				ZVM_LOG_WARN("vgic_gicd_mem_write icactive: x %d, y %d, value %x, vcpu: %d \n",
						x, y, *value, vcpu->vcpu_id);
			for (i = 0; i < vcpu->vm->vcpu_num; i++) {
				__vgic_test_and_set_active_bit(vcpu->vm->vcpus[i], y, value, 32, 0, gicd);
			}
			break;
		case GICD_IPRIORITYRn...(GIC_DIST_BASE + 0x07f8 - 1):
			x = ((offset - GICD_IPRIORITYRn) / 4);
			y = x * 4;
			if (y >= VM_GLOBAL_VIRQ_NR) {
				break;
			}
			__vgic_set_and_get_priority_bit(vcpu, y, value, 32, 1, gicd);
			break;
		case GICD_ICFGRn...(GIC_DIST_BASE + 0x0cfc - 1):
			// /* Get the irq num */
			// x = (offset - GICD_ICFGRn) * 4;
			// if (x >= VM_GLOBAL_VIRQ_NR) {
			// 	break;
			// } else if (x < 32)	{
			// 	*value = 0xAAAAAAAA;
			// }
			// for (i = 0; i < vcpu->vm->vcpu_num; i++) {
			// 	__vgic_test_and_set_trigger_type_bit(vcpu->vm->vcpus[i], value, offset - GICD_ICFGRn, gicd);
			// }
			virt_irq_set_type(vcpu, offset, value);
			break;
		case (GIC_DIST_BASE + VGIC_RESERVED)...(GIC_DIST_BASE + VGIC_INMIRn - 1):
		default:
			break;
	}
	barrier_dmem_fence_full();
	barrier_dsync_fence_full();
	barrier_isync_fence_full();

	return 0;
}

void arch_hw_irq_enable(struct z_vcpu *vcpu)
{
	uint32_t irq;
	struct z_vm *vm = vcpu->vm;
	struct z_virt_dev *vdev;
    struct  _dnode *d_node, *ds_node;

	SYS_DLIST_FOR_EACH_NODE_SAFE(&vm->vdev_list, d_node, ds_node) {
        vdev = CONTAINER_OF(d_node, struct z_virt_dev, vdev_node);
		if(vdev->dev_pt_flag && vcpu->vcpu_id == 0) {
			/* enable spi interrupt */
			irq = vdev->hirq;
			if (irq > CONFIG_NUM_IRQS) {
				continue;
			}
			arm_gic_irq_enable(irq);
		}
    }
}

void arch_hw_irq_disable(struct z_vcpu *vcpu)
{
	uint32_t irq;
	struct z_vm *vm = vcpu->vm;
	struct z_virt_dev *vdev;
    struct  _dnode *d_node, *ds_node;

	SYS_DLIST_FOR_EACH_NODE_SAFE(&vm->vdev_list, d_node, ds_node) {
        vdev = CONTAINER_OF(d_node, struct z_virt_dev, vdev_node);
		if(vdev->dev_pt_flag && vcpu->vcpu_id == 0) {
			/* disable spi interrupt */
			irq = vdev->hirq;
			if(irq > CONFIG_NUM_IRQS){
				continue;
			}
			arm_gic_irq_disable(irq);
		}
    }
}

int vgic_vdev_mem_read(struct z_virt_dev *vdev, uint64_t addr, uint64_t *value, uint16_t size)
{
	uint32_t offset, ret, type;
	struct z_vcpu *vcpu;
	struct vgicv3_dev *vgic;
	struct virt_gic_gicd *gicd;
	struct virt_gic_gicr *gicr;

	type = TYPE_GIC_INVAILD;
	vcpu = _current_vcpu;
 	vgic= (struct vgicv3_dev *)vdev->priv_vdev;

	/*Avoid some case that we only just use '|' to get the value */
	*value = 0;

	gicd = &vgic->gicd;
	if ((addr >= gicd->gicd_base) && (addr < gicd->gicd_base + gicd->gicd_size)) {
		type = TYPE_GIC_GICD;
		offset = addr - gicd->gicd_base;
	} else {
		gicr = get_vcpu_gicr_type(vcpu, vgic, addr, &type, &offset);
		if (gicr == NULL) {
			ZVM_LOG_WARN("Can not get gicr type for addr: 0x%llx, vdev: %s", addr, vdev->name);
			return -EINVAL;
		}
	}
	K_SPINLOCK(&vgic->vgic_lock) {
		switch (type) {
		case TYPE_GIC_GICD:
			ret = vgic_gicd_mem_read(vcpu, gicd, offset, value);
			break;
		case TYPE_GIC_GICR_RD:
			ret =  vgic_gicrrd_mem_read(vcpu, gicr, offset, value);
			break;
		case TYPE_GIC_GICR_SGI:
			ret =  vgic_gicrsgi_mem_read(vcpu, gicr, offset, value);
			break;
		case TYPE_GIC_GICR_VLPI:
				/* ignore vlpi register */
    	        ret = 0;
				break;
		default:
			ret = 0;
			return 0;
		}
	}
	return ret;
}

int vgic_vdev_mem_write(struct z_virt_dev *vdev, uint64_t addr, uint64_t *value, uint16_t size)
{
    uint32_t offset, type , ret = 0;
	struct z_vcpu *vcpu;
	struct vgicv3_dev *vgic;
	struct virt_gic_gicd *gicd;
	struct virt_gic_gicr *gicr;

	type = TYPE_GIC_INVAILD;
	vcpu = _current_vcpu;
 	vgic = (struct vgicv3_dev *)vdev->priv_vdev;

	gicd = &vgic->gicd;
    if ((addr >= gicd->gicd_base) && (addr < gicd->gicd_base + gicd->gicd_size)) {
		type = TYPE_GIC_GICD;
		offset = addr - gicd->gicd_base;
	} else {
		gicr = get_vcpu_gicr_type(vcpu, vgic, addr, &type, &offset);
		if (gicr == NULL) {
			ZVM_LOG_WARN("Can not get gicr type for addr: 0x%llx, vdev: %s", addr, vdev->name);
			return -EINVAL;
		}
	}

	K_SPINLOCK(&vgic->vgic_lock) {
		switch (type) {
		case TYPE_GIC_GICD:
			ret =  vgic_gicd_mem_write(vcpu, gicd, offset, value);
			break;
		case TYPE_GIC_GICR_RD:
			ret =  vgic_gicrrd_mem_write(vcpu, gicr, offset, value);
			break;
		case TYPE_GIC_GICR_SGI:
			ret =  vgic_gicrsgi_mem_write(vcpu, gicr, offset, value);
			break;
		case TYPE_GIC_GICR_VLPI:
			ret =  0;
			break;
		default:
			ret =  0;
		}
	}
    return ret;
}

int set_virq_to_vcpu(struct z_vcpu *vcpu, uint32_t virq_num, uint8_t virq_level)
{
    return vgic_set_virq_to_vcpu(vcpu, virq_num, virq_level);
}

int set_virq_to_vm(struct z_vm *vm, uint32_t virq_num)
{
	struct z_vcpu *vcpu;
	vcpu = _current_vcpu ? _current_vcpu : vm->vcpus[DEFAULT_VCPU];

	return vgic_set_virq_to_vcpu(vcpu, virq_num, true);
}

int unset_virq_to_vm(struct z_vm *vm, uint32_t virq_num)
{
	struct z_vcpu *vcpu;
	vcpu = _current_vcpu ? _current_vcpu : vm->vcpus[DEFAULT_VCPU];

	return vgic_set_virq_to_vcpu(vcpu, virq_num, false);
}

int virt_irq_flush_vgic(struct z_vcpu *vcpu, struct vgicv3_dev *vgic)
{
	bool is_idle_lr = true;
	int ret, i, irq;
	uint32_t virq_mask, src_cpu;
	uint32_t *vpending, *venabled;
	struct virt_irq_desc *desc;
	uint32_t cpu_mask = 1 << vcpu->vcpu_id;

	virq_mask = VGIC_GET_PENDING(vcpu, VGIC_LOCAL_IRQ_BASE, 32, vgic);
	virq_mask &= VGIC_GET_ENABLED(vcpu, VGIC_LOCAL_IRQ_BASE, 32, vgic);
	while (virq_mask) {
		irq = __builtin_ctz(virq_mask);
		virq_mask &= ~BIT(irq);

		desc = VGIC_GET_VIRQ_DESC(vcpu, irq);
		if (!desc) {
			ZVM_LOG_ERR("Can not find the virq desc for vcpu %d, virq num %d. \n",
						vcpu->vcpu_id, irq);
			return -ENODEV;
		}

		src_cpu = desc->src_cpu;
		if (!src_cpu) {
			ret = vgic_inject_virq_vcpuif(vcpu, desc);
			if (ret >= 0) {
				VGIC_UNSET_PENDING(vcpu, irq, cpu_mask);
			} else {
				is_idle_lr = false;
				goto flush_done;
			}
		} else {
			for (i = 0; i < vcpu->vm->vcpu_num; i++) {
				if (!(src_cpu & BIT(i))) {
					continue;
				}
				/* Inject virq to vcpu */
				ret = vgic_inject_virq_vcpuif(vcpu, desc);
				if (ret >= 0) {
					src_cpu &= ~BIT(i);
				}
			}
			desc->src_cpu = src_cpu;
			if (!src_cpu) {
				VGIC_UNSET_PENDING(vcpu, irq, cpu_mask);
			} else {
				is_idle_lr = false;
				goto flush_done;
			}
		}
	}

	vpending = vgic->gicd.virq_pending[vcpu->vcpu_id];
	venabled = vgic->gicd.virq_enabled[vcpu->vcpu_id];
	for (i = 1; i < (VM_GLOBAL_VIRQ_NR / 32); i++) {
		virq_mask = vpending[i] & venabled[i];
		while(virq_mask) {
			irq = __builtin_ctz(virq_mask);
			virq_mask &= ~BIT(irq);
			irq += i * 32;
			desc = VGIC_GET_VIRQ_DESC(vcpu, irq);
			if (!desc) {
				ZVM_LOG_ERR("Can not find the virq desc for vcpu %d, virq num %d.",
							vcpu->vcpu_id, irq);
				return -ENODEV;
			}
			ret = vgic_inject_virq_vcpuif(vcpu, desc);
			if (ret >= 0) {
				VGIC_UNSET_PENDING(vcpu, irq, cpu_mask);
				if (VGIC_IRQ_TEST_TRIGGER_TYPE(vcpu, desc->virq_num, vgic) == VM_IRQ_LEVEL_TRIGGERED) {
					VGIC_SET_ACTIVE(vcpu, irq, cpu_mask);
				}
			} else {
				is_idle_lr = false;
				goto flush_done;
			}
		}
	}

flush_done:
	if (!is_idle_lr) {
		ZVM_LOG_WARN("There are no idle list registers, vcpu_id: %d, vm_id: %d.",
						vcpu->vcpu_id, vcpu->vm->vmid);
		ZVM_LOG_WARN("ZVM needs maintain irq to process.");
	}
	return 0;
}

int virt_irq_sync_vgic(struct z_vcpu *vcpu, struct vgicv3_dev *vgic)
{
	bool is_idle_lr = false;
	int lr_count, i;
	uint32_t irq_num;
	uint64_t elrsr, eisr, value = 0;
	struct gicv3_list_reg *lr = (struct gicv3_list_reg *)&value;
	struct virt_irq_desc *desc;
	uint32_t cpu_mask = 1 << vcpu->vcpu_id;

	if (!VGIC_LIST_REGS_USED(vcpu)) {
		return 0;
	}

	/* Get a valid list register */
	elrsr = read_elrsr_el2();
	eisr = read_eisr_el2();
	elrsr |= eisr;
	elrsr &= vcpu->arch->list_regs_map;

	for (lr_count = 0; lr_count < VGIC_TYPER_LR_NUM; lr_count++) {
		if (!(VGIC_ELRSR_REG_TEST(lr_count, elrsr))) {
			if (VGIC_ELRSR_REG_TEST(lr_count, eisr)) {
				ZVM_LOG_WARN("There is an eoi interrupt, but no list register is used, vcpu_id: %d, lr_count: %d. \n",
						vcpu->vcpu_id, lr_count);
			}
			continue;
		}
		is_idle_lr = true;
		value = vgicv3_read_lr(lr_count);
		vgicv3_write_lr(lr_count, 0);
		irq_num = lr->vINTID;

		if (irq_num >= VM_GLOBAL_VIRQ_NR) {
			ZVM_LOG_WARN("Invalid irq_num: %d, vcpu_id: %d, vm_id: %d.",
						irq_num, vcpu->vcpu_id, vcpu->vm->vmid);
		}

		desc = VGIC_GET_VIRQ_DESC(vcpu, irq_num);
		if (!desc) {
			ZVM_LOG_ERR("Can not find the virq desc for vcpu %d, virq num %d.",
						vcpu->vcpu_id, irq_num);
			continue;
		}
		if (VGIC_IRQ_TEST_TRIGGER_TYPE(vcpu, desc->virq_num, vgic) == VM_IRQ_LEVEL_TRIGGERED) {
			/* clear active bit */
			VGIC_UNSET_ACTIVE(vcpu, irq_num, cpu_mask);

			/**
			 * If the irq is level triggered, and the level is high,
			 * we should not clear the pending bit, because it is still
			 * pending until the level set to low.
			 * @TODO: Support no interrupt affinity now, all seem to be routed to the all vCPU
			 */
			if (atomic_get(&desc->level_triggered) == VIRQ_LEVEL_HIGH) {
				ZVM_LOG_WARN("irq %d is level high, do not clear pending bit, vcpu_id: %d, vm_id: %d. \n",
						irq_num, vcpu->vcpu_id, vcpu->vm->vmid);
				/* If the irq is level high, we should not clear it */
				VGIC_SET_PENDING(vcpu, irq_num, cpu_mask);
			} else {
				ZVM_LOG_WARN("irq %d is level low, clear pending bit, vcpu_id: %d, vm_id: %d. \n",
						irq_num, vcpu->vcpu_id, vcpu->vm->vmid);
				VGIC_UNSET_PENDING(vcpu, irq_num, cpu_mask);
			}
		}

		VGIC_LIST_REGS_UNSET(lr_count, vcpu);
		desc->id = VM_INVALID_DESC_ID;
	}

	if (!is_idle_lr) {
		for (i = 0; i < vcpu->vm->vcpu_num; i++) {
			/* Notify the vcpu scheduler */
			if (vcpu->vm->vcpus[i]->vcpu_id != vcpu->vcpu_id) {
				atomic_inc(&vcpu->vm->vcpus[i]->hcpuipi_count);
				/* Notify other vCPUs to sync the vgic */
				vcpu_ipi_scheduler(BIT(vcpu->vm->vcpus[i]->cpu), 0);
			}
		}
		// ZVM_LOG_INFO("No list registers need to process, virq: %d, vcpu_id: %d, vm_id: %d.\n",
		//  				global_virq_now, vcpu->vcpu_id, vcpu->vm->vmid);
	}
	return 0;
}

int cpu_virq_flush_lock(struct z_vcpu *vcpu)
{
    struct vgicv3_dev *vgic = (struct vgicv3_dev *)vcpu->vm->vm_irq_block.virt_priv_data;
    k_spinlock_key_t key;

	unsigned int k = arch_irq_lock();
    key = k_spin_lock(&vgic->vgic_lock);
    int ret = virt_irq_flush_vgic(vcpu, vgic);
    k_spin_unlock(&vgic->vgic_lock, key);
    arch_irq_unlock(k);
    if (ret) {
        ZVM_LOG_ERR("Flush vgic info failed, Unknow reason \n");
    }
    return ret;
}

int cpu_virq_sync_lock(struct z_vcpu *vcpu)
{
    struct vgicv3_dev *vgic = (struct vgicv3_dev *)vcpu->vm->vm_irq_block.virt_priv_data;
    k_spinlock_key_t key;

	unsigned int k = arch_irq_lock();
    key = k_spin_lock(&vgic->vgic_lock);
    int ret = virt_irq_sync_vgic(vcpu, vgic);
    k_spin_unlock(&vgic->vgic_lock, key);
	arch_irq_unlock(k);

    if (ret) {
        ZVM_LOG_ERR("Sync vgic info failed, Unknow reason \n");
    }
    return ret;
}

void virt_sync_and_flush_vcpu_vgic(struct z_vcpu *vcpu, struct vgicv3_dev *vgic)
{
	/* Sync the vcpu's vgic state */
	virt_irq_sync_vgic(vcpu, vgic);
	/* Flush the vcpu's vgic state */
	virt_irq_flush_vgic(vcpu, vgic);
}

void virt_sync_and_flush_vcpu_vgic_lock(struct z_vcpu *vcpu)
{
	struct vgicv3_dev *vgic = (struct vgicv3_dev *)vcpu->vm->vm_irq_block.virt_priv_data;
	K_SPINLOCK(&vgic->vgic_lock) {
		virt_sync_and_flush_vcpu_vgic(vcpu, vgic);
	}
}

ALWAYS_INLINE struct virt_irq_desc *get_virt_irq_desc(struct z_vcpu *vcpu, uint32_t virq)
{
	return VGIC_GET_VIRQ_DESC(vcpu, virq);
}
