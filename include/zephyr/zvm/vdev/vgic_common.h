/*
 * Copyright 2024-2025 HNU-ESNL: Guoqi Xie, Chenglai Xiong, Xingyu Hu and etc.
 * Copyright 2024-2025 openEuler SIG-Zephyr
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_VIRTUALIZATION_ARM_VGIC_COMMON_H_
#define ZEPHYR_INCLUDE_VIRTUALIZATION_ARM_VGIC_COMMON_H_

#include <zephyr/kernel.h>
#include <ksched.h>
#include <zephyr/devicetree.h>
#include <zephyr/arch/common/sys_bitops.h>
#include <zephyr/drivers/interrupt_controller/gic.h>
#include <zephyr/zvm/vm.h>
#include <zephyr/zvm/vm_device.h>
#include <zephyr/zvm/arm/switch.h>
#include <zephyr/zvm/zlog.h>

#define VGIC_CONTROL_BLOCK_ID		vgic_control_block
#define VGIC_CONTROL_BLOCK_NAME		vm_irq_control_block

/* GIC version */
#define VGIC_V2_REV     	(0x2)
#define VGIC_V3_REV     	(0x3)
#define VGIC_ARCH_REV_SHIFT	(4)

/* GIC dev type */
#define TYPE_GIC_GICD		BIT(0)
#define TYPE_GIC_GICR_RD	BIT(1)
#define TYPE_GIC_GICR_SGI	BIT(2)
#define TYPE_GIC_GICR_VLPI	BIT(3)
#define TYPE_GIC_INVAILD	(0xFF)

/* GIC device macro here */
#define VGIC_DIST_BASE		DT_REG_ADDR_BY_IDX(DT_INST(0, arm_gic), 0)
#define VGIC_DIST_SIZE		DT_REG_SIZE_BY_IDX(DT_INST(0, arm_gic), 0)
#define VGIC_RDIST_BASE		DT_REG_ADDR_BY_IDX(DT_INST(0, arm_gic), 1)
#define VGIC_RDIST_SIZE		DT_REG_SIZE_BY_IDX(DT_INST(0, arm_gic), 1)

/* GICD registers offset from DIST_base(n) */
#define VGICD_CTLR			(GICD_CTLR-GIC_DIST_BASE)
#define VGICD_TYPER			(GICD_TYPER-GIC_DIST_BASE)
#define VGICD_IIDR			(GICD_IIDR-GIC_DIST_BASE)
#define VGICD_STATUSR		(GICD_STATUSR-GIC_DIST_BASE)
#define VGICD_ISENABLERn	(GICD_ISENABLERn-GIC_DIST_BASE)
#define VGICD_ICENABLERn	(GICD_ICENABLERn-GIC_DIST_BASE)
#define VGICD_ISPENDRn		(GICD_ISPENDRn-GIC_DIST_BASE)
#define VGICD_ICPENDRn		(GICD_ICPENDRn-GIC_DIST_BASE)
#define VGICD_ISACTIVERn	(GICD_ISACTIVERn-GIC_DIST_BASE)
#define VGICD_ICACTIVERn	(0x380)

#define VGIC_RESERVED		0x0F30
#define VGIC_INMIRn			0x0f80
#define VGICD_PIDR2			0xFFE8

/* Vgic control block flag */
#define VIRQ_HW_SUPPORT		BIT(1)

#define VGIC_VIRQ_IN_SGI	(0x0)
#define VGIC_VIRQ_IN_PPI	(0x1)

/* Sorting virt irq to SGI/PPI/SPI */
#define VGIC_VIRQ_LEVEL_SORT(irq)	((irq)/VM_SGI_VIRQ_NR)

/* VGIC Type for virtual interrupt control */
#define VGIC_TYPER_REGISTER			(read_sysreg(ICH_VTR_EL2))
#define VGIC_TYPER_LR_NUM 			((VGIC_TYPER_REGISTER & 0x1F) + 1)
#define VGIC_TYPER_PRIO_NUM			(((VGIC_TYPER_REGISTER >> 29) & 0x07) + 1)

/* 64k frame */
#define VGIC_RD_BASE_SIZE			(64 * 1024)
#define VGIC_SGI_BASE_SIZE			(64 * 1024)
#define VGIC_RD_SGI_SIZE			(VGIC_RD_BASE_SIZE + VGIC_SGI_BASE_SIZE)

#define ENABLE_BIT_PER_REG			(32)
#define PENDING_BIT_PER_REG			(32)
#define ACTIVE_BIT_PER_REG			(32)


/**
 * @brief Virtual generatic interrupt controller distributor
 * struct for each vm.
*/
struct virt_gic_gicd {
	/**
	 * gicd address base and size which are used
	 * to locate vdev access for vm.
	*/
	uint32_t gicd_base;
	uint32_t gicd_size;

	uint32_t vgicd_ctlr;
	uint32_t vgicd_typer;
	uint32_t vgicd_pidr2;

	uint32_t virq_enabled[CONFIG_MAX_VCPU_PER_VM][VM_GLOBAL_VIRQ_NR / ENABLE_BIT_PER_REG];
	uint32_t virq_pending[CONFIG_MAX_VCPU_PER_VM][VM_GLOBAL_VIRQ_NR / PENDING_BIT_PER_REG];
	uint32_t virq_active[CONFIG_MAX_VCPU_PER_VM][VM_GLOBAL_VIRQ_NR / ACTIVE_BIT_PER_REG];

	/**
	 * Each bit is corsponding to cpu mask,
	 * if the bit is set, it means that the irq is edge triggered.
	 * If the bit is clear, it means that the irq is level triggered.
	 */
	uint32_t virq_edge_trigger[VM_GLOBAL_VIRQ_NR];

};

/**
 * @brief Just for enable menopoly irq for vcpu.
 */
void arch_hw_irq_enable(struct z_vcpu *vcpu);

/**
 * @brief Just for disable menopoly irq for vcpu.
 */
void arch_hw_irq_disable(struct z_vcpu *vcpu);


int vgic_vdev_mem_read(struct z_virt_dev *vdev, uint64_t addr, uint64_t *value, uint16_t size);
int vgic_vdev_mem_write(struct z_virt_dev *vdev, uint64_t addr, uint64_t *value, uint16_t size);

/**
 * @brief set/unset a virt irq signal to a vcpu.
 */
int set_virq_to_vcpu(struct z_vcpu *vcpu, uint32_t virq_num, uint8_t virq_level);

/**
 * @brief set/unset a virt irq to vm.
 */
int set_virq_to_vm(struct z_vm *vm, uint32_t virq_num);
int unset_virq_to_vm(struct z_vm *vm, uint32_t virq_num);

int cpu_virq_flush_lock(struct z_vcpu *vcpu);
int cpu_virq_sync_lock(struct z_vcpu *vcpu);

/**
 * @brief Get the virq desc object.
 */
struct virt_irq_desc *get_virt_irq_desc(struct z_vcpu *vcpu, uint32_t virq);

#define IS_VM_IRQ_VALID(vm) \
    (!( (vm)->vm_status == VM_STATE_NEVER_RUN || \
        (vm)->vm_status == VM_STATE_HALT ))

#define VGIC_GET_VIRQ_DESC(vcpu, virq)                                 \
    ((uint32_t)(virq) < VM_GLOBAL_VIRQ_NR                              \
            ? &((vcpu)->virq_block.vcpu_virt_irq_desc[(virq)]): NULL)

/**
 * @brief When vcpu is loop on idel mode, we must send virq
 * to activate it.
 */
static ALWAYS_INLINE void wakeup_target_vcpu(struct z_vcpu *vcpu, struct virt_irq_desc *desc)
{
    ARG_UNUSED(desc);
    /* Set thread into runnig queue */
    z_ready_thread(vcpu->work->vcpu_thread);
}

static ALWAYS_INLINE bool vgic_irq_test_bit(struct z_vcpu *vcpu, uint32_t spi_nr_count,
						uint32_t *value, uint32_t bit_size, bool enable)
{
	ARG_UNUSED(enable);
	ARG_UNUSED(spi_nr_count);
	int bit;
	uint32_t reg_mem_addr = (uint64_t)value;
	for (bit=0; bit<bit_size; bit++) {
		if (sys_test_bit(reg_mem_addr, bit)) {
			return true;
		}
	}
	return false;
}

#endif /* ZEPHYR_INCLUDE_VIRTUALIZATION_ARM_VGIC_COMMON_H_ */
