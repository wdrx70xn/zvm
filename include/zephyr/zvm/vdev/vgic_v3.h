/*
 * Copyright 2024-2025 HNU-ESNL: Guoqi Xie, Chenglai Xiong, Xingyu Hu and etc.
 * Copyright 2024-2025 openEuler SIG-Zephyr
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_VIRTUALIZATION_ARM_VGIC_V3_H_
#define ZEPHYR_INCLUDE_VIRTUALIZATION_ARM_VGIC_V3_H_

#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/spinlock.h>
#include <zephyr/drivers/interrupt_controller/gic.h>
#include <zephyr/arch/arm64/sys_io.h>
#include <zephyr/zvm/vm_device.h>
#include <zephyr/zvm/vm_irq.h>
#include <zephyr/zvm/zvm.h>
#include <zephyr/zvm/vdev/vgic_common.h>
#include "../../../drivers/interrupt_controller/intc_gicv3_priv.h"

#define ZVM_VGIC_V3_DEBUG	0

/* SGI mode */
#define SGI_SIG_TO_LIST			(0)
#define SGI_SIG_TO_OTHERS		(1)

#define VGIC_MAX_VCPU       	64
#define VGIC_UNDEFINE_ADDR  	0xFFFFFFFF
#define VGIC_ALL_VCPU_MASK(vm)	((1 << (vm->vcpu_num)) - 1)

/* vgic action */
#define ACTION_CLEAR_VIRQ	    BIT(0)
#define ACTION_SET_VIRQ		    BIT(1)

/* GIC control value */
#define GICH_VMCR_VENG0			(1 << 0)
#define GICH_VMCR_VENG1			(1 << 1)
#define GICH_VMCR_VACKCTL		(1 << 2)
#define GICH_VMCR_VFIQEN		(1 << 3)
#define GICH_VMCR_VCBPR			(1 << 4)
#define GICH_VMCR_VEOIM			(1 << 9)
#define GICH_VMCR_DEFAULT_MASK  (0xf8 << 24)

#define GICH_HCR_EN       		(1 << 0)
#define GICH_HCR_UIE      		(1 << 1)
#define GICH_HCR_LRENPIE  		(1 << 2)
#define GICH_HCR_NPIE     		(1 << 3)
#define GICH_HCR_TALL1			(1 << 12)

/* list register */
#define LIST_REG_GTOUP0			(0)
#define LIST_REG_GROUP1			(1)
#define LIST_REG_NHW_VIRQ		(0)
#define LIST_REG_HW_VIRQ		(1)

/* GICR registers offset from RDIST_base(n) */
#define VGICR_CTLR				GICR_CTLR
#define VGICR_IIDR				GICR_IIDR
#define VGICR_TYPER				GICR_TYPER
#define VGICR_STATUSR			GICR_STATUSR
#define VGICR_WAKER				GICR_WAKER
#define VGICR_PROPBASER			GICR_PROPBASER
#define VGICR_PENDBASER			GICR_PENDBASER
#define VGICR_ISENABLER0		0x0100
#define VGICR_ICENABLER0		0x0180
#define VGICR_SGI_PENDING		0x0200
#define VGICR_SGI_ICPENDING		0x0280
#define VGICR_SGI_ACTIVE		0x0300
#define VGICR_SGI_ICACTIVE		0x0380
#define VGICR_PIDR2				0xFFE8

/**
 * @brief vcpu vgicv3 register interface.
 */
struct gicv3_vcpuif_ctxt {
	uint64_t ich_lr0_el2;
	uint64_t ich_lr1_el2;
	uint64_t ich_lr2_el2;
	uint64_t ich_lr3_el2;
	uint64_t ich_lr4_el2;
	uint64_t ich_lr5_el2;
	uint64_t ich_lr6_el2;
	uint64_t ich_lr7_el2;

	uint32_t ich_ap0r2_el2;
	uint32_t ich_ap1r2_el2;
	uint32_t ich_ap0r1_el2;
	uint32_t ich_ap1r1_el2;
	uint32_t ich_ap0r0_el2;
	uint32_t ich_ap1r0_el2;
	uint32_t ich_vmcr_el2;
	uint32_t ich_hcr_el2;

	uint32_t icc_ctlr_el1;
	uint32_t icc_sre_el1;
	uint32_t icc_pmr_el1;
};

/**
 * @brief gicv3_list_reg register bit field, which
 * provides interrupt context information for the virtual
 * CPU interface.
 */
struct gicv3_list_reg {
	uint64_t vINTID 	: 32;
	uint64_t pINTID 	: 13;
	uint64_t res0 		: 3;
	uint64_t priority 	: 8;
	uint64_t res1 		: 3;
	uint64_t nmi 		: 1;
	uint64_t group 		: 1;
	uint64_t hw 		: 1;
	uint64_t state 		: 2;
};

/**
 * @brief Virtual generatic interrupt controller redistributor
 * struct for each vm's vcpu.
 * Each Redistributor defines four 64KB frames as follows:
 * 1. RD_base
 * 2. SGI_base
 * 3. VLPI_base
 * 4. Reserved
 * TODO: support vlpi later.
*/
struct virt_gic_gicr {
	uint32_t vcpu_id;

	/**
	 * gicr address base and size which
	 * are used to locate vdev access from
	 * vm.
	*/
	uint32_t gicr_rd_base;
	uint32_t gicr_rd_size;
	uint32_t gicr_sgi_base;
	uint32_t gicr_sgi_size;

	uint32_t gicr_rd_ctlr;
	uint32_t gicr_rd_pidr2;
	uint64_t gicr_rd_typer;
	uint64_t gicr_sgi_typer;

	uint32_t virq_enabled;
	uint32_t virq_pending;
	uint32_t virq_active;
};

/**
 * @brief vgicv3 virtual device struct, for emulate device.
 */
struct vgicv3_dev {
	struct virt_gic_gicd gicd;
	struct virt_gic_gicr *gicr[VGIC_RDIST_SIZE/VGIC_RD_SGI_SIZE];
	struct k_spinlock vgic_lock;
};

/**
 * @brief virtual gicv3 device information.
*/
struct gicv3_vdevice {
	uint64_t gicd_base;
	uint64_t gicd_size;
	uint64_t gicr_base;
	uint64_t gicr_size;
};


/* list register test and set */
#define VGIC_LIST_REGS_USED(vcpu)	\
			(((struct z_vcpu *)vcpu)->arch->list_regs_map)
#define VGIC_LIST_REGS_TEST(id, vcpu)	\
			((((struct z_vcpu *)vcpu)->arch->list_regs_map) & (1 << id))
#define VGIC_LIST_REGS_UNSET(id, vcpu) ((((struct z_vcpu *)vcpu)->arch->list_regs_map)\
			= ((((struct z_vcpu *)vcpu)->arch->list_regs_map)\
			& (~(1 << id))))
#define VGIC_LIST_REGS_SET(id, vcpu) ((((struct z_vcpu *)vcpu)->arch->list_regs_map)\
			= ((((struct z_vcpu *)vcpu)->arch->list_regs_map)\
			| (1 << id)))
#define VGIC_ELRSR_REG_TEST(id, elrsr) ((1 << ((id) & 0x1F)) & elrsr)


#define VGIC_ISENABLE_REG_OFFSET(irq)	\
			((irq) < 32 ? VGICR_ISENABLER0 : VGICD_ISENABLERn + 4 * ((irq) >> 5))

#define VGIC_ICENABLE_REG_OFFSET(irq)	\
			((irq) < 32 ? VGICR_ICENABLER0 : VGICD_ICENABLERn + 4 * ((irq) >> 5))

#define VGIC_ISPENDING_REG_OFFSET(irq)	\
			((irq) < 32 ? VGICR_SGI_PENDING : VGICD_ISPENDRn + 4 * ((irq) >> 5))

#define VGIC_ICPENDING_REG_OFFSET(irq)	\
			((irq) < 32 ? VGICR_SGI_ICPENDING : VGICD_ICPENDRn + 4 * ((irq) >> 5))

#define VGIC_ISACTIVE_REG_OFFSET(irq)	\
			((irq) < 32 ? VGICR_SGI_ACTIVE : VGICD_ISPENDRn + 4 * ((irq) >> 5))

#define VGIC_ICACTIVE_REG_OFFSET(irq)	\
			((irq) < 32 ? VGICR_SGI_ICACTIVE : VGICD_ICPENDRn + 4 * ((irq) >> 5))


#define VGIC_TEST_AND_SET_ENABLE(vcpu, irq, bs, set)                    \
    do {                                                                \
        struct vgicv3_dev *__vgic =                                       \
            (struct vgicv3_dev *)(vcpu)->vm->vm_irq_block.virt_priv_data; \
        uint32_t __irq_ = (irq), __bs = (bs);                            \
        uint32_t *__p = (__irq_ < VM_LOCAL_VIRQ_NR                        \
             ? &__vgic->gicr[(vcpu)->vcpu_id]->virq_enabled                \
             : &__vgic->gicd.virq_enabled[(vcpu)->vcpu_id][__irq_ / __bs]); \
        if (set)                                                        \
            *__p |= BIT(__irq_ % 32);                                    \
        else                                                            \
            *__p &= ~BIT(__irq_ % 32);                                   \
    } while (0)

#define VGIC_TEST_AND_SET_PENDING(vcpu, irq, bs, set)                \
    do {                                                             \
        struct vgicv3_dev *__vgic =                                    \
            (struct vgicv3_dev *)(vcpu)->vm->vm_irq_block.virt_priv_data; \
        uint32_t __irq_ = (irq), _bs = (bs);                           \
        uint32_t *p = (__irq_ < VM_LOCAL_VIRQ_NR                       \
             ? &__vgic->gicr[(vcpu)->vcpu_id]->virq_pending            \
             : &__vgic->gicd.virq_pending[(vcpu)->vcpu_id][__irq_ / _bs]);\
        if (set)                                                    \
            *p |= BIT(__irq_ % 32);                                   \
        else                                                        \
            *p &= ~BIT(__irq_ % 32);                                  \
    } while (0)


#define VGIC_TEST_AND_SET_ACTIVE(vcpu, irq, bs, set)                     \
    do {                                                                 \
        struct vgicv3_dev *__vgic =                                        \
            (struct vgicv3_dev *)(vcpu)->vm->vm_irq_block.virt_priv_data;\
        uint32_t __irq_ = (irq), __bs = (bs);                             \
        uint32_t *__p = (__irq_ < VM_LOCAL_VIRQ_NR                         \
             ? &__vgic->gicr[(vcpu)->vcpu_id]->virq_active                  \
             : &__vgic->gicd.virq_active[(vcpu)->vcpu_id][__irq_ / __bs]);   \
        if (set)                                                         \
            *__p |= BIT(__irq_ % 32);                                     \
        else                                                             \
            *__p &= ~BIT(__irq_ % 32);                                    \
    } while (0)

#define VGIC_SET_ENABLE(vcpu, irq, cpu_mask)                                \
    do {                                                                    \
        struct z_vm *__vm    = (vcpu)->vm;                                  \
        uint32_t     __irq   = (irq),                                       \
                     __mask  = (cpu_mask);                                  \
        int          __i_vcpu;                                                   \
        for (__i_vcpu = 0; __i_vcpu < __vm->vcpu_num; __i_vcpu++) {                        \
            if (!(__mask & (1U << __i_vcpu)))                                     \
                continue;                                                   \
            VGIC_TEST_AND_SET_ENABLE(__vm->vcpus[__i_vcpu], __irq, 32, true);    \
        }                                                                   \
    } while (0)

#define VGIC_UNSET_ENABLE(vcpu, irq, cpu_mask)                             \
    do {                                                                   \
        struct z_vm *__vm    = (vcpu)->vm;                                 \
        uint32_t     __irq   = (irq),                                      \
                     __mask  = (cpu_mask);                                 \
        int          __i_vcpu;                                                  \
        for (__i_vcpu = 0; __i_vcpu < __vm->vcpu_num; __i_vcpu++) {                       \
            if (!(__mask & (1U << __i_vcpu)))                                   \
                continue;                                                  \
            VGIC_TEST_AND_SET_ENABLE(__vm->vcpus[__i_vcpu], __irq, 32, false);  \
        }                                                                  \
    } while (0)

#define VGIC_SET_PENDING(vcpu, irq, cpu_mask)                              \
    do {                                                                   \
        struct z_vm *__vm    = (vcpu)->vm;                                 \
        uint32_t     __irq   = (irq),                                      \
                     __mask  = (cpu_mask);                                 \
        int          __i_vcpu;                                                  \
        for (__i_vcpu = 0; __i_vcpu < __vm->vcpu_num; __i_vcpu++) {                       \
            if (!(__mask & (1U << __i_vcpu)))                                   \
                continue;                                                  \
            VGIC_TEST_AND_SET_PENDING(__vm->vcpus[__i_vcpu], __irq, 32, true);  \
        }                                                                  \
    } while (0)

#define VGIC_UNSET_PENDING(vcpu, irq, cpu_mask)                            \
    do {                                                                   \
        struct z_vm *__vm    = (vcpu)->vm;                                 \
        uint32_t     __irq   = (irq),                                      \
                     __mask  = (cpu_mask);                                 \
        int          __i_vcpu;                                                  \
        for (__i_vcpu = 0; __i_vcpu < __vm->vcpu_num; __i_vcpu++) {                       \
            if (!(__mask & (1U << __i_vcpu)))                                   \
                continue;                                                  \
            VGIC_TEST_AND_SET_PENDING(__vm->vcpus[__i_vcpu], __irq, 32, false); \
        }                                                                  \
    } while (0)

#define VGIC_SET_ACTIVE(vcpu, irq, cpu_mask)                             \
    do {                                                                 \
        struct z_vm *__vm    = (vcpu)->vm;                               \
        uint32_t     __irq   = (irq),                                    \
                     __mask  = (cpu_mask);                               \
        int          __i_vcpu;                                                \
        for (__i_vcpu = 0; __i_vcpu < __vm->vcpu_num; __i_vcpu++) {                     \
            if (!(__mask & (1U << __i_vcpu)))                                 \
                continue;                                                \
            VGIC_TEST_AND_SET_ACTIVE(__vm->vcpus[__i_vcpu], __irq, 32, true); \
        }                                                                \
    } while (0)

#define VGIC_UNSET_ACTIVE(vcpu, irq, cpu_mask)                           \
    do {                                                                 \
        struct z_vm *__vm    = (vcpu)->vm;                               \
        uint32_t     __irq   = (irq),                                    \
                     __mask  = (cpu_mask);                               \
        int          __i_vcpu;                                                \
        for (__i_vcpu = 0; __i_vcpu < __vm->vcpu_num; __i_vcpu++) {                     \
            if (!(__mask & (1U << __i_vcpu)))                                 \
                continue;                                                \
            VGIC_TEST_AND_SET_ACTIVE(__vm->vcpus[__i_vcpu], __irq, 32, false);\
        }                                                                \
    } while (0)


#define __VGIC_GET_ENABLE_BIT(vcpu, irq_nr_base, value_ptr, bit_size, vgic_priv)   \
    do {                                                                         \
        uint32_t __irq_base = (irq_nr_base);                                     \
        uint32_t __bs       = (bit_size);                                        \
        if (__irq_base) {                                                        \
            struct virt_gic_gicd *__gicd =                                       \
                (struct virt_gic_gicd *)(vgic_priv);                             \
            *(value_ptr) =                                                        \
                __gicd->virq_enabled[(vcpu)->vcpu_id][__irq_base / __bs];         \
        } else {                                                                 \
            struct virt_gic_gicr *__gicr =                                       \
                (struct virt_gic_gicr *)(vgic_priv);                             \
            *(value_ptr) = __gicr->virq_enabled;                                 \
        }                                                                        \
    } while (0)

#define __VGIC_GET_PENDING_BIT(vcpu, irq_nr_base, value_ptr, bit_size, vgic_priv)  \
    do {                                                                         \
        uint32_t __irq_base = (irq_nr_base);                                     \
        uint32_t __bs       = (bit_size);                                        \
        if (__irq_base) {                                                        \
            struct virt_gic_gicd *__gicd =                                       \
                (struct virt_gic_gicd *)(vgic_priv);                             \
            *(value_ptr) =                                                        \
                __gicd->virq_pending[(vcpu)->vcpu_id][__irq_base / __bs];         \
        } else {                                                                 \
            struct virt_gic_gicr *__gicr =                                       \
                (struct virt_gic_gicr *)(vgic_priv);                             \
            *(value_ptr) = __gicr->virq_pending;                                 \
        }                                                                        \
    } while (0)

#define __VGIC_GET_ACTIVE_BIT(vcpu, irq_nr_base, value_ptr, bit_size, vgic_priv)   \
    do {                                                                         \
        uint32_t __irq_base = (irq_nr_base);                                     \
        uint32_t __bs       = (bit_size);                                        \
        if (__irq_base) {                                                        \
            struct virt_gic_gicd *__gicd =                                       \
                (struct virt_gic_gicd *)(vgic_priv);                             \
            *(value_ptr) =                                                        \
                __gicd->virq_active[(vcpu)->vcpu_id][__irq_base / __bs];          \
        } else {                                                                 \
            struct virt_gic_gicr *__gicr =                                       \
                (struct virt_gic_gicr *)(vgic_priv);                             \
            *(value_ptr) = __gicr->virq_active;                                  \
        }                                                                        \
    } while (0)

#define VGIC_GET_ENABLED(vcpu, irq_base, bit_size, vgic)                      \
    ({                                                                         \
        uint32_t __irq_nr_base = (uint32_t)(irq_base) * 32u;                   \
        uint32_t __value;                                                      \
        if (__irq_nr_base < VM_LOCAL_VIRQ_NR) {                                \
            __VGIC_GET_ENABLE_BIT((vcpu), __irq_nr_base, &__value,            \
                                 (bit_size),                                   \
                                 (vgic)->gicr[(vcpu)->vcpu_id]);               \
        } else {                                                               \
            __VGIC_GET_ENABLE_BIT((vcpu), __irq_nr_base, &__value,            \
                                 (bit_size),                                   \
                                 &(vgic)->gicd);                              \
        }                                                                      \
        __value;                                                               \
    })

#define VGIC_GET_PENDING(vcpu, irq_base, bit_size, vgic)                      \
    ({                                                                         \
        uint32_t __irq_nr_base = (uint32_t)(irq_base) * 32u;                   \
        uint32_t __value;                                                      \
        if (__irq_nr_base < VM_LOCAL_VIRQ_NR) {                                \
            __VGIC_GET_PENDING_BIT((vcpu), __irq_nr_base, &__value,           \
                                    (bit_size),                                \
                                    (vgic)->gicr[(vcpu)->vcpu_id]);            \
        } else {                                                               \
            __VGIC_GET_PENDING_BIT((vcpu), __irq_nr_base, &__value,           \
                                    (bit_size),                                \
                                    &(vgic)->gicd);                           \
        }                                                                      \
        __value;                                                               \
    })

#define VGIC_GET_ACTIVE(vcpu, irq_base, bit_size, vgic)                       \
    ({                                                                         \
        uint32_t __irq_nr_base = (uint32_t)(irq_base) * 32u;                   \
        uint32_t __value;                                                      \
        if (__irq_nr_base < VM_LOCAL_VIRQ_NR) {                                \
            __VGIC_GET_ACTIVE_BIT((vcpu), __irq_nr_base, &__value,            \
                                   (bit_size),                                 \
                                   (vgic)->gicr[(vcpu)->vcpu_id]);             \
        } else {                                                               \
            __VGIC_GET_ACTIVE_BIT((vcpu), __irq_nr_base, &__value,            \
                                   (bit_size),                                 \
                                   &(vgic)->gicd);                            \
        }                                                                      \
        __value;                                                               \
    })

#define VGIC_IRQ_TEST_TRIGGER_TYPE(vcpu, irq_num, vgic)                     \
    (__extension__ ({                                                       \
        uint32_t __irq  = (irq_num);                                        \
        uint32_t __mask = 1u << (vcpu)->vcpu_id;                             \
        struct vgicv3_dev *__g3 = (vgic);                                   \
        (__irq >= VM_LOCAL_VIRQ_NR                                         \
            ? (((__g3)->gicd.virq_edge_trigger)[__irq] & __mask)            \
                ? VM_IRQ_EDGE_TRIGGERED                                     \
                : VM_IRQ_LEVEL_TRIGGERED                                    \
            : VM_IRQ_EDGE_TRIGGERED);                                       \
    }))


	/**
 * @brief gic vcpu interface init.
 */
int vcpu_gicv3_init(struct gicv3_vcpuif_ctxt *ctxt);

/**
 * @brief before enter vm, we need to load the vcpu interrupt state.
 */
int vgicv3_state_load(struct z_vcpu *vcpu, struct gicv3_vcpuif_ctxt *ctxt);

/**
 * @brief before exit from vm, we need to store the vcpu interrupt state.
 */
int vgicv3_state_save(struct z_vcpu *vcpu, struct gicv3_vcpuif_ctxt *ctxt);

/**
 * @brief send a virq to vm for el1 trap.
 */
int vgic_inject_virq_vcpuif(struct z_vcpu *vcpu, struct virt_irq_desc *desc);

/**
 * @brief gic redistribute vdev mem read.
 */
int vgic_gicrsgi_mem_read(struct z_vcpu *vcpu, struct virt_gic_gicr *gicr,
			uint32_t offset, uint64_t *v);

/**
 * @brief gic redistribute sgi vdev mem write
 */
int vgic_gicrsgi_mem_write(struct z_vcpu *vcpu, struct virt_gic_gicr *gicr,
			uint32_t offset, uint64_t *v);

/**
 * @brief gic redistribute rd vdev mem read
 */
int vgic_gicrrd_mem_read(struct z_vcpu *vcpu, struct virt_gic_gicr *gicr,
			uint32_t offset, uint64_t *v);

/**
 * @brief gic redistribute rd vdev mem write.
 */
int vgic_gicrrd_mem_write(struct z_vcpu *vcpu, struct virt_gic_gicr *gicr,
			uint32_t offset, uint64_t *v);

/**
 * @brief get gicr address type.
 */
struct virt_gic_gicr* get_vcpu_gicr_type(struct z_vcpu *vcpu, struct vgicv3_dev *vgic, uint32_t addr,
											uint32_t *type,  uint32_t *offset);
/**
 * @brief raise a sgi signal to a vcpu.
 */
int vgicv3_raise_sgi2vcpu(struct z_vcpu *vcpu, uint64_t sgi_value);

/**
 * @brief init vgicv3 device for the vm.
*/
struct vgicv3_dev *vgicv3_dev_init(struct z_vm *vm);

/**
 * @brief set a virtual irq to a vcpu.
 */
int vgic_set_virq_to_vcpu(struct z_vcpu *vcpu, uint32_t virq_num, uint8_t virq_level);

int virt_irq_sync_vgic(struct z_vcpu *vcpu, struct vgicv3_dev *vgic);
int virt_irq_flush_vgic(struct z_vcpu *vcpu, struct vgicv3_dev *vgic);

void virt_sync_and_flush_vcpu_vgic(struct z_vcpu *vcpu, struct vgicv3_dev *vgic);
void virt_sync_and_flush_vcpu_vgic_lock(struct z_vcpu *vcpu);

/**
 * @brief disable the vcpu internal interrupt.
 * This function is used to disable the vcpu internal interrupt
 * when the vcpu is in a state that can not recieve interrupts.
 */
void save_disable_internal_int(struct z_vcpu *vcpu, bool disable_all);

/**
 * @brief enable the vcpu internal interrupt.
 * This function is used to enable the vcpu internal interrupt
 * when the vcpu is in a state that can recieve interrupts.
 */
void load_enable_internal_int(struct z_vcpu *vcpu, bool enable_all);


/**
 * @brief Only when this irq is a pass-through irq,
 * we can enable it.
 */
static ALWAYS_INLINE int vgic_irq_enable_hw(struct z_vcpu *vcpu, uint32_t virt_irq)
{
	struct virt_irq_desc *desc;

	desc = VGIC_GET_VIRQ_DESC(vcpu, virt_irq);
	if (!desc) {
        return -ENOENT;
    }
	if (desc->pirq_num < VM_GLOBAL_VIRQ_NR) {
		if (atomic_get(&desc->bind_hwirq_flag)) {
			irq_enable(desc->pirq_num);
		}
	}
	return 0;
}

/**
 * @brief Only when this irq is a pass-through irq,
 * we can disable it.
 */
static ALWAYS_INLINE int vgic_irq_disable_hw(struct z_vcpu *vcpu, uint32_t virt_irq)
{
	struct virt_irq_desc *desc;

	desc = VGIC_GET_VIRQ_DESC(vcpu, virt_irq);
	if (!desc) {
		return -ENOENT;
	}
	if (desc->pirq_num < VM_GLOBAL_VIRQ_NR) {
		if (atomic_get(&desc->bind_hwirq_flag)) {
			irq_disable(desc->pirq_num);
		}
	}
	return 0;
}

/**
 * @brief When VM write isenable or icenable flag, we
 * should set/unset enable irq signal to VM.
*/
static ALWAYS_INLINE void __vgic_test_and_set_enable_bit(struct z_vcpu *vcpu, uint32_t irq_nr_base,
						uint32_t *value, uint32_t bit_size, bool set_flag, void *vgic_priv)
{
	int bit;
	uint32_t reg_mem_addr = (uint64_t)value;
	struct virt_gic_gicd *gicd = NULL;
	struct virt_gic_gicr *gicr = NULL;

	/* 检查当前irq是否属于spi, 非spi值为0 */
	if (irq_nr_base) {
		for (bit = 0; bit < bit_size; bit++) {
			if (sys_test_bit(reg_mem_addr, bit)) {
				gicd = (struct virt_gic_gicd *)vgic_priv;
				if (set_flag) {
					if (unlikely(irq_nr_base + bit > VM_GLOBAL_VIRQ_NR)) {
						ZVM_LOG_WARN("Set pending bit for irq %d, irq_nr_base: %d, but it exceeds the limit of irq num, vcpu: %d",
							(int)(irq_nr_base + bit), irq_nr_base, vcpu->vcpu_id);
						return;
					}
					vgic_irq_enable_hw(vcpu, irq_nr_base + bit);
					gicd->virq_enabled[vcpu->vcpu_id][(irq_nr_base) / bit_size] |= BIT(bit);
				} else {
					vgic_irq_disable_hw(vcpu, irq_nr_base + bit);
					gicd->virq_enabled[vcpu->vcpu_id][(irq_nr_base) / bit_size] &= ~BIT(bit);
				}
			}
		}
	} else {
		for (bit = 0; bit < bit_size; bit++) {
			if (sys_test_bit(reg_mem_addr, bit)) {
				gicr = (struct virt_gic_gicr *)vgic_priv;
				if (set_flag) {
					vgic_irq_enable_hw(vcpu, irq_nr_base + bit);
					gicr->virq_enabled |= BIT(bit);
				} else {
					vgic_irq_disable_hw(vcpu, irq_nr_base + bit);
					gicr->virq_enabled &= ~BIT(bit);
				}
			}
		}
	}

}

/**
 * @brief When VM write ispending or icpending flag, we
 * should set/unset pending irq signal to VM.
*/
static ALWAYS_INLINE void __vgic_test_and_set_pending_bit(struct z_vcpu *vcpu, uint32_t irq_nr_base,
						uint32_t *value, uint32_t bit_size, bool set_flag, void *vgic_priv)
{
	int bit;
	uint32_t reg_mem_addr = (uint64_t)value;
	struct virt_gic_gicd *gicd = NULL;
	struct virt_gic_gicr *gicr = NULL;

	/* 检查当前irq是否属于spi, 非spi值为0 */
	if (irq_nr_base) {
		for (bit = 0; bit < bit_size; bit++) {
			if (sys_test_bit(reg_mem_addr, bit)) {
				gicd = (struct virt_gic_gicd *)vgic_priv;
				if (set_flag) {
					if (unlikely(irq_nr_base + bit > VM_GLOBAL_VIRQ_NR)) {
						ZVM_LOG_WARN("Set pending bit for irq %d, but it exceeds the limit of irq num, vcpu: %d",
							(int)(irq_nr_base + bit), vcpu->vcpu_id);
						return;
					}
					/*@TODO: vgic_irq_pending(vcpu, irq_nr_base + bit);*/
					gicd->virq_pending[vcpu->vcpu_id][(irq_nr_base + bit) / PENDING_BIT_PER_REG] |= BIT(bit);
				} else {
					/*@TODO: vgic_irq_icpending(vcpu, irq_nr_base + bit);*/
					gicd->virq_pending[vcpu->vcpu_id][(irq_nr_base + bit) / PENDING_BIT_PER_REG] &= ~BIT(bit);
				}
			}
		}
	} else {
		for (bit = 0; bit < bit_size; bit++) {
			if (sys_test_bit(reg_mem_addr, bit)) {
				gicr = (struct virt_gic_gicr *)vgic_priv;
				if (set_flag) {
					/*@TODO: vgic_irq_pending(vcpu, irq_nr_base + bit);*/
					gicr->virq_pending |= BIT(bit);
				} else {
					/*@TODO: vgic_irq_icpending(vcpu, irq_nr_base + bit);*/
					gicr->virq_pending &= ~BIT(bit);
				}
			}
		}
	}
}

/**
 * @brief 直接操作vgic的函数，寄存器级别操作；
*/
static ALWAYS_INLINE void __vgic_test_and_set_active_bit(struct z_vcpu *vcpu, uint32_t irq_nr_base,
						uint32_t *value, uint32_t bit_size, bool set_flag, void *vgic_priv)
{
	int bit;
	uint32_t reg_mem_addr = (uint64_t)value;
	struct virt_gic_gicd *gicd = NULL;
	struct virt_gic_gicr *gicr = NULL;

	/* 检查当前irq是否属于spi, 非spi值为0 */
	if (irq_nr_base) {
		for (bit = 0; bit < bit_size; bit++) {
			if (sys_test_bit(reg_mem_addr, bit)) {
				gicd = (struct virt_gic_gicd *)vgic_priv;
				if (set_flag) {
					if (unlikely(irq_nr_base + bit > VM_GLOBAL_VIRQ_NR)) {
						ZVM_LOG_WARN("Set active bit for irq %d, but it exceeds the limit of irq num, vcpu: %d",
							(int)(irq_nr_base + bit), vcpu->vcpu_id);
						return;
					}
					/*@TODO: vgic_irq_active(vcpu, irq_nr_base + bit);*/
					gicd->virq_active[vcpu->vcpu_id][(irq_nr_base + bit) / ACTIVE_BIT_PER_REG] |= BIT(bit);
				} else {
					/*@TODO: vgic_irq_inactive(vcpu, irq_nr_base + bit);*/
					gicd->virq_active[vcpu->vcpu_id][(irq_nr_base + bit) / ACTIVE_BIT_PER_REG] &= ~BIT(bit);
				}
			}
		}
	} else {
		for (bit = 0; bit < bit_size; bit++) {
			if (sys_test_bit(reg_mem_addr, bit)) {
				gicr = (struct virt_gic_gicr *)vgic_priv;
				if (set_flag) {
					/*@TODO: vgic_irq_active(vcpu, irq_nr_base + bit);*/
					gicr->virq_active |= BIT(bit);
				} else {
					/*@TODO: vgic_irq_inactive(vcpu, irq_nr_base + bit);*/
					gicr->virq_active &= ~BIT(bit);
				}
			}
		}
	}
}

static ALWAYS_INLINE void __vgic_set_and_get_priority_bit(struct z_vcpu *vcpu, uint32_t irq_nr_base,
						uint32_t *value, uint32_t bit_size, bool set_flag, void *vgic_priv)
{
	ARG_UNUSED(vgic_priv);
	int bit, prios_per_reg, cpu_num;
	struct virt_irq_desc *desc;

	prios_per_reg = bit_size / 8; /* 每个优先级占用的bit数 */
	if (set_flag) {
		for (bit = 0; bit < prios_per_reg; bit++) {
			for (cpu_num = 0; cpu_num < vcpu->vm->vcpu_num; cpu_num++) {
				desc = VGIC_GET_VIRQ_DESC(vcpu->vm->vcpus[cpu_num], irq_nr_base + bit);
				if (!desc) {
					ZVM_LOG_ERR("Failed to get virq desc for irq %d, vcpu: %d", irq_nr_base + bit, vcpu->vcpu_id);
					return;
				}
				desc->prio = (uint32_t)(*value >> (bit * 8)) & 0xFF;
			}
		}
	}
	else {
		for (bit = 0; bit < prios_per_reg; bit++) {
			desc = VGIC_GET_VIRQ_DESC(vcpu, irq_nr_base + bit);
			if (!desc) {
				ZVM_LOG_ERR("Failed to get virq desc for irq %d, vcpu: %d", irq_nr_base + bit, vcpu->vcpu_id);
				return;
			}
			*value |= (uint32_t)(desc->prio & 0xFF) << (bit * 8);
		}
	}
}

static ALWAYS_INLINE void vgic_set_enable_lock(struct z_vcpu *vcpu, uint32_t irq, uint32_t cpu_mask)
{
	struct vgicv3_dev *vgic = (struct vgicv3_dev *)vcpu->vm->vm_irq_block.virt_priv_data;

	K_SPINLOCK(&vgic->vgic_lock) {
		VGIC_SET_ENABLE(vcpu, irq, cpu_mask);
	}
}

static ALWAYS_INLINE void vgic_unset_enable_lock(struct z_vcpu *vcpu, uint32_t irq, uint32_t cpu_mask)
{
	struct vgicv3_dev *vgic = (struct vgicv3_dev *)vcpu->vm->vm_irq_block.virt_priv_data;

	K_SPINLOCK(&vgic->vgic_lock) {
		VGIC_UNSET_ENABLE(vcpu, irq, cpu_mask);
	}
}

static ALWAYS_INLINE void vgic_set_pending_lock(struct z_vcpu *vcpu, uint32_t irq, uint32_t cpu_mask)
{
	struct vgicv3_dev *vgic = (struct vgicv3_dev *)vcpu->vm->vm_irq_block.virt_priv_data;

	K_SPINLOCK(&vgic->vgic_lock) {
		VGIC_SET_PENDING(vcpu, irq, cpu_mask);
	}
}

static ALWAYS_INLINE void vgic_unset_pending_lock(struct z_vcpu *vcpu, uint32_t irq, uint32_t cpu_mask)
{
	struct vgicv3_dev *vgic = (struct vgicv3_dev *)vcpu->vm->vm_irq_block.virt_priv_data;

	K_SPINLOCK(&vgic->vgic_lock) {
		VGIC_UNSET_PENDING(vcpu, irq, cpu_mask);
	}
}

static ALWAYS_INLINE void vgic_set_active_lock(struct z_vcpu *vcpu, uint32_t irq, uint32_t cpu_mask)
{
	struct vgicv3_dev *vgic = (struct vgicv3_dev *)vcpu->vm->vm_irq_block.virt_priv_data;

	K_SPINLOCK(&vgic->vgic_lock) {
		VGIC_SET_ACTIVE(vcpu, irq, cpu_mask);
	}
}

static ALWAYS_INLINE void vgic_unset_active_lock(struct z_vcpu *vcpu, uint32_t irq, uint32_t cpu_mask)
{
	struct vgicv3_dev *vgic = (struct vgicv3_dev *)vcpu->vm->vm_irq_block.virt_priv_data;

	K_SPINLOCK(&vgic->vgic_lock) {
		VGIC_UNSET_ACTIVE(vcpu, irq, cpu_mask);
	}
}

static ALWAYS_INLINE bool vgic_is_virq_enabled(struct z_vcpu *vcpu, uint32_t irq,
								struct vgicv3_dev *vgic, uint32_t cpu_mask)
{
	int i;
	struct virt_gic_gicd *gicd = NULL;
	struct virt_gic_gicr *gicr = NULL;

	for (i = 0; i < vcpu->vm->vcpu_num; i++) {
		if (!(cpu_mask & (1 << i))) {
			continue;
		}
		if (irq < VM_LOCAL_VIRQ_NR) {
			gicr = vgic->gicr[i];
			if (gicr->virq_enabled & BIT(irq % ENABLE_BIT_PER_REG)) {
				return true;
			}
		} else {
			gicd = &vgic->gicd;
			if (gicd->virq_enabled[i][(irq / ENABLE_BIT_PER_REG)] & BIT(irq % ENABLE_BIT_PER_REG)) {
				return true;
			}
		}
	}
	return false;
}

static ALWAYS_INLINE bool vgic_is_virq_pending(struct z_vcpu *vcpu, uint32_t irq,
								struct vgicv3_dev *vgic, uint32_t cpu_mask)
{
	int i;
	struct virt_gic_gicd *gicd = NULL;
	struct virt_gic_gicr *gicr = NULL;

	for (i = 0; i < vcpu->vm->vcpu_num; i++) {
		if (!(cpu_mask & (1 << i))) {
			continue;
		}
		if (irq < VM_LOCAL_VIRQ_NR) {
			gicr = vgic->gicr[i];
			if (gicr->virq_pending & BIT(irq % PENDING_BIT_PER_REG)) {
				return true;
			}
		} else if (irq < VM_GLOBAL_VIRQ_NR) {
			gicd = &vgic->gicd;
			if (gicd->virq_pending[i][(irq / PENDING_BIT_PER_REG)] & BIT(irq % PENDING_BIT_PER_REG)) {
				return true;
			}
		} else {
			ZVM_LOG_ERR("vgic_is_pending: irq %d exceeds the limit of irq num, vcpu: %d",
				irq, vcpu->vcpu_id);
			return false;
		}
	}
	return false;
}

static ALWAYS_INLINE bool vgic_is_virq_active(struct z_vcpu *vcpu, uint32_t irq,
								struct vgicv3_dev *vgic, uint32_t cpu_mask)
{
	int i;
	struct virt_gic_gicd *gicd = NULL;
	struct virt_gic_gicr *gicr = NULL;

	for (i = 0; i < vcpu->vm->vcpu_num; i++) {
		if (!(cpu_mask & (1 << i))) {
			continue;
		}
		if (irq < VM_LOCAL_VIRQ_NR) {
			gicr = vgic->gicr[i];
			if (gicr->virq_active & BIT(irq % ACTIVE_BIT_PER_REG)) {
				return true;
			}
		} else if (irq < VM_GLOBAL_VIRQ_NR) {
			gicd = &vgic->gicd;
			if (gicd->virq_active[i][(irq / ACTIVE_BIT_PER_REG)] & BIT(irq % ACTIVE_BIT_PER_REG)) {
				return true;
			}
		} else {
			ZVM_LOG_ERR("vgic_is_active: irq %d exceeds the limit of irq num, vcpu: %d",
				irq, vcpu->vcpu_id);
			return false;
		}
	}
	return false;
}

static ALWAYS_INLINE bool vgic_is_virq_enabled_lock(struct z_vcpu *vcpu, uint32_t irq, uint32_t cpu_mask)
{
	bool ret;
	struct vgicv3_dev *vgic = (struct vgicv3_dev *)vcpu->vm->vm_irq_block.virt_priv_data;
	K_SPINLOCK(&vgic->vgic_lock) {
		ret = vgic_is_virq_enabled(vcpu, irq, vgic, cpu_mask);
	}
	return ret;
}

static ALWAYS_INLINE bool vgic_is_virq_pending_lock(struct z_vcpu *vcpu, uint32_t irq, uint32_t cpu_mask)
{
	bool ret;
	struct vgicv3_dev *vgic = (struct vgicv3_dev *)vcpu->vm->vm_irq_block.virt_priv_data;
	K_SPINLOCK(&vgic->vgic_lock) {
		ret = vgic_is_virq_pending(vcpu, irq, vgic, cpu_mask);
	}
	return ret;
}

static ALWAYS_INLINE bool vgic_is_virq_active_lock(struct z_vcpu *vcpu, uint32_t irq, uint32_t cpu_mask)
{
	bool ret;
	struct vgicv3_dev *vgic = (struct vgicv3_dev *)vcpu->vm->vm_irq_block.virt_priv_data;
	K_SPINLOCK(&vgic->vgic_lock) {
		ret = vgic_is_virq_active(vcpu, irq, vgic, cpu_mask);
	}
	return ret;
}

static ALWAYS_INLINE void __vgic_test_and_set_trigger_type_bit(struct z_vcpu *vcpu, uint32_t *value,
					uint32_t offset, struct virt_gic_gicd *gicd)
{
	bool is_edge = false;
	uint32_t icfgr_idx = offset >> 2;
	uint32_t irq_base  = icfgr_idx * 16;
	uint32_t cpu_mask  = 1u << vcpu->vcpu_id;
	uint32_t irq, cfg;

	/* 对 16 个中断逐个处理 */
	for (int i = 0; i < 16; i++) {
		irq = irq_base + i;
		cfg = (*value >> (2 * i)) & 0x3;
		is_edge = (cfg == 0b10);

		if (irq < VM_GLOBAL_VIRQ_NR) {
			if (is_edge) {
				/* 边缘触发：置 1 */
				gicd->virq_edge_trigger[irq] |= cpu_mask;
			} else {
				/* 电平触发：清 0 */
				gicd->virq_edge_trigger[irq] &= ~cpu_mask;
			}
		}
	}
}

static ALWAYS_INLINE uint64_t vgicv3_read_lr(uint8_t register_id)
{
	switch (register_id) {
	case 0:
		return read_sysreg(ICH_LR0_EL2);
	case 1:
		return read_sysreg(ICH_LR1_EL2);
	case 2:
		return read_sysreg(ICH_LR2_EL2);
	case 3:
		return read_sysreg(ICH_LR3_EL2);
	case 4:
		return read_sysreg(ICH_LR4_EL2);
	case 5:
		return read_sysreg(ICH_LR5_EL2);
	case 6:
		return read_sysreg(ICH_LR6_EL2);
	case 7:
		return read_sysreg(ICH_LR7_EL2);
	default:
		return 0;
	}
}

static ALWAYS_INLINE void vgicv3_write_lr(uint8_t register_id, uint64_t value)
{
	switch (register_id) {
	case 0:
		write_sysreg(value, ICH_LR0_EL2);
		break;
	case 1:
		write_sysreg(value, ICH_LR1_EL2);
		break;
	case 2:
		write_sysreg(value, ICH_LR2_EL2);
		break;
	case 3:
		write_sysreg(value, ICH_LR3_EL2);
		break;
	case 4:
		write_sysreg(value, ICH_LR4_EL2);
		break;
	case 5:
		write_sysreg(value, ICH_LR5_EL2);
		break;
	case 6:
		write_sysreg(value, ICH_LR6_EL2);
		break;
	case 7:
		write_sysreg(value, ICH_LR7_EL2);
		break;
	default:
		return;
	}
}

static ALWAYS_INLINE void vgicv3_pre_set_lr(struct virt_irq_desc *desc, struct gicv3_list_reg *lr)
{
	lr->vINTID = desc->virq_num;
	lr->priority = desc->prio;

	if (desc->virq_states & VIRQ_STATE_PENDING) {
		lr->state |= VIRQ_STATE_PENDING;
	}
	if (desc->virq_states & VIRQ_STATE_ACTIVE) {
		lr->state |= VIRQ_STATE_ACTIVE;
	}

	if (desc->pirq_num != VM_INVALID_PIRQ_NUM) {
		lr->pINTID = desc->pirq_num;
		lr->hw = 1;
	} else {
		lr->pINTID = 0;
		lr->hw = 0;
	}

	lr->group = 1;
}

/**
 * @brief Get virq state from register.
 */
static ALWAYS_INLINE uint8_t vgicv3_get_lr_state(struct z_vcpu *vcpu, struct virt_irq_desc *desc)
{
	uint64_t value;

	if (desc->id >=  VGIC_TYPER_LR_NUM) {
		return 0;
	}
	value = vgicv3_read_lr(desc->id);
	value = (value >> 62) & 0x03;

	return ((uint8_t)value);
}

/**
 * @brief Find the idle list register.
*/
static ALWAYS_INLINE uint8_t vgicv3_get_idle_lr(struct z_vcpu *vcpu)
{
	uint8_t i;
	for (i = 0; i < VGIC_TYPER_LR_NUM; i++) {
		if (!VGIC_LIST_REGS_TEST(i, vcpu)) {
			return i;
		}
	}
	return -1;
}

#endif /* ZEPHYR_INCLUDE_VIRTUALIZATION_ARM_VGIC_V3_H_ */
