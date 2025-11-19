/*
 * Copyright 2024-2025 HNU-ESNL: Guoqi Xie, Chenglai Xiong, Xingyu Hu and etc.
 * Copyright 2024-2025 openEuler SIG-Zephyr
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_VIRTUALIZATION_VM_IRQ_H_
#define ZEPHYR_INCLUDE_VIRTUALIZATION_VM_IRQ_H_

#include <stdint.h>
#include <zephyr/sys/dlist.h>
#include <zephyr/arch/arm64/lib_helpers.h>
#include <zephyr/zvm/arm/switch.h>
#include <zephyr/zvm/vm_device.h>

/*TODO: HW_FLAG may not enbaled for each spi.*/
#define VIRQ_FLAG_HW                BIT(0)

#define VIRQ_LEVEL_LOW             (0)
#define VIRQ_LEVEL_HIGH            (1)

/* Hardware irq states */
#define VIRQ_STATE_INVALID		        (0b00)
#define VIRQ_STATE_PENDING		        (0b01)
#define VIRQ_STATE_ACTIVE		        (0b10)
#define VIRQ_STATE_ACTIVE_AND_PENDING	(0b11)

/* VM's injuct irq num, bind to the register */
#define VM_INVALID_DESC_ID	            (0xFF)
#define VM_INVALID_VIRQ_NUM	            (0xFFFF)
#define VM_INVALID_PIRQ_NUM	            (0xFFFF)
#define VM_IPI_FROM_HOST_CPU            (0xFF000000)

#define VM_IRQ_LEVEL_TRIGGERED	        (0)
#define VM_IRQ_EDGE_TRIGGERED	        (1)

/* VM's irq prio */
#define VM_DEFAULT_LOCAL_VIRQ_PRIO      (0x20)

/* irq number for arm64 system. */
#define VM_LOCAL_VIRQ_NR	(VM_SGI_VIRQ_NR + VM_PPI_VIRQ_NR)
#define VM_GLOBAL_VIRQ_NR   (VM_LOCAL_VIRQ_NR + VM_SPI_VIRQ_NR)

struct z_vm;
struct z_virt_dev;

/**
 * @brief Description for each virt irq descripetor.
 */
struct virt_irq_desc {

    /* Id that describes the irq. */
    uint8_t id;
    uint8_t vcpu_id;
    uint8_t vm_id;
    uint8_t prio;

    /* irq level type */
    uint8_t type;

    /* hardware virq states */
    uint8_t virq_states;

    /* Specific CPU that triggered the interrupt */
    uint32_t src_cpu;

    /**
     * If physical irq is existed, pirq_num has
     * a value, otherwise, it is set to 0xFFFFFFFF
    */
    uint32_t pirq_num;
    uint32_t virq_num;

    atomic_t bind_hwirq_flag;
    /**
     * When this flag is set, it means this irq is a
     * level triggered irq, and it is high level now.
     */
    atomic_t level_triggered;

    sys_dnode_t desc_node;
};

/* vcpu wfi struct */
struct vcpu_wfi {
    bool state;
    uint16_t yeild_count;
    struct k_spinlock wfi_lock;
    void *priv;
};

/**
 * @brief vm's irq block to describe this all the device interrup
 * for vm. In this struct, it called `VM_LOCAL_VIRQ_NR`;
*/
struct vcpu_virt_irq_block {

    uint32_t pending_sgi_num;

    struct virt_irq_desc vcpu_virt_irq_desc[VM_GLOBAL_VIRQ_NR];
    struct vcpu_wfi vwfi;

    struct k_spinlock spinlock;

    sys_dlist_t active_irqs;
    sys_dlist_t pending_irqs;
};

/**
 * @brief vm's irq block to describe this all the device interrup
 * for vm. In this struct, it called `VM_GLOBAL_VIRQ_NR-VM_LOCAL_VIRQ_NR`;
*/
struct vm_virt_irq_block {

    bool enabled;
    bool pt_irq_bitmap[VM_GLOBAL_VIRQ_NR];

    /* interrupt control block flag */
    uint32_t flags;
    uint32_t irq_num;
    uint32_t cpu_num;

	uint32_t irq_target[VM_GLOBAL_VIRQ_NR];
	uint32_t ipi_vcpu_source[CONFIG_MP_NUM_CPUS][VM_SGI_VIRQ_NR];

    /* virtual irq block. */
    struct k_spinlock vm_virq_lock;

    /* bind to interrupt controller */
    void *virt_priv_data;
};

bool vcpu_irq_exist(struct z_vcpu *vcpu);

int vcpu_wait_for_irq(struct z_vcpu *vcpu);

/**
 * @brief init the irq desc when add vm_dev.
*/
void vm_device_irq_init(struct z_vm *vm, struct z_virt_dev *vm_dev);

/**
 * @brief init irq block for vm.
 */
int vm_irq_block_init(struct z_vm *vm);

/**
 * @brief sync and flush the vcpu's irq.
 */
void virt_sync_and_flush_vcpu_irq(struct z_vcpu *vcpu);

/**
 * @brief Check if the virtual interrupt is enabled.
 */
bool virt_irq_isenabled(struct z_vcpu *vcpu);

/**
 * @brief Check if the virtual interrupt is pending.
 */
bool virt_irq_ispending(struct z_vcpu *vcpu);

/**
 * @brief Check if the virtual interrupt is active.
 */
bool virt_irq_isactive(struct z_vcpu *vcpu);

#endif /* ZEPHYR_INCLUDE_VIRTUALIZATION_VM_IRQ_H_ */
