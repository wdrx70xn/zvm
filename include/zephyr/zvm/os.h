/*
 * Copyright 2024-2025 HNU-ESNL: Guoqi Xie, Chenglai Xiong, Xingyu Hu and etc.
 * Copyright 2024-2025 openEuler SIG-Zephyr
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_VIRTUALIZATION_OS_H_
#define ZEPHYR_INCLUDE_VIRTUALIZATION_OS_H_

#include <zephyr/kernel.h>
#include <stdint.h>
#include <zephyr/zvm/vm_mm.h>
#include <zephyr/zvm/zlog.h>

struct getopt_state;

#define OS_NAME_LENGTH          (32)
#define OS_TYPE_ZEPHYR          (0)
#define OS_TYPE_LINUX           (1)
#define OS_TYPE_FREERTOS        (2)
#define OS_TYPE_OTHERS          (3)
#define OS_TYPE_MAX             (4)

#define MAX_LINUX_OS_NUM        (5)
#define MAX_VIRTIO_OS_NUM       MAX_LINUX_OS_NUM

#define RTOS_VM_LOAD_BASE     DT_REG_ADDR(DT_NODELABEL(rtos_ddr))
#define RTOS_VM_LOAD_SIZE     DT_REG_SIZE(DT_NODELABEL(rtos_ddr))
#define RTOS_VM_MEMORY_BASE   DT_PROP(DT_NODELABEL(rtos_ddr), vm_reg_base)
#define RTOS_VM_MEMORY_SIZE   DT_PROP(DT_NODELABEL(rtos_ddr), vm_reg_size)

#define ZEPHYR_IMAGE_BASE       DT_REG_ADDR(DT_NODELABEL(zephyr_1_128))
#define ZEPHYR_IMAGE_SIZE       DT_REG_SIZE(DT_NODELABEL(zephyr_1_128))
#define ZEPHYR_VM_VCPU_NUM      DT_PROP(DT_NODELABEL(zephyr_1_128), vcpu_num)
#define ZEPHYR_VM_MAX_MEM_SIZE  DT_PROP(DT_NODELABEL(zephyr_1_128), max_mem_size)

#define FREERTOS_IMAGE_BASE       DT_REG_ADDR(DT_NODELABEL(freertos_1_128))
#define FREERTOS_IMAGE_SIZE       DT_REG_SIZE(DT_NODELABEL(freertos_1_128))
#define FREERTOS_VM_VCPU_NUM      DT_PROP(DT_NODELABEL(freertos_1_128), vcpu_num)
#define FREERTOS_VM_MAX_MEM_SIZE  DT_PROP(DT_NODELABEL(freertos_1_128), max_mem_size)

#define LINUX_VM_LOAD_BASE      DT_REG_ADDR(DT_NODELABEL(linux_ddr))
#define LINUX_VM_LOAD_SIZE      DT_REG_SIZE(DT_NODELABEL(linux_ddr))
#define LINUX_VM_MEMORY_BASE    DT_PROP(DT_NODELABEL(linux_ddr), vm_reg_base)
#define LINUX_VM_MEMORY_SIZE    DT_PROP(DT_NODELABEL(linux_ddr), vm_reg_size)

#define LINUX_IMAGE_BASE        DT_PROP(DT_NODELABEL(linux_node), image_addr)
#define LINUX_IMAGE_SIZE        DT_PROP(DT_NODELABEL(linux_node), image_size)
#define LINUX_VMRFS_BASE        DT_PROP(DT_NODELABEL(linux_node), rootfs_addr)
#define LINUX_VMRFS_SIZE        DT_PROP(DT_NODELABEL(linux_node), rootfs_size)
#define LINUX_VMRFS_PHY_BASE    DT_PROP(DT_NODELABEL(linux_node), rootfs_paddress)

#define LINUX_VMDTB_BASE        DT_REG_ADDR(DT_NODELABEL(dtb_addr_1_512))
#define LINUX_VMDTB_SIZE        DT_REG_SIZE(DT_NODELABEL(dtb_addr_1_512))
#define LINUX_VM_VCPU_NUM       DT_PROP(DT_NODELABEL(dtb_addr_1_512), vcpu_num)
#define LINUX_VM_MAX_MEM_SIZE   DT_PROP(DT_NODELABEL(dtb_addr_1_512), max_mem_size)

#ifdef CONFIG_VM_DTB_FILE_INPUT
#define LINUX_DTB_MEM_BASE        DT_PROP(DT_NODELABEL(linux_node), dtb_address)
#define LINUX_DTB_MEM_SIZE        DT_PROP(DT_NODELABEL(linux_node), dtb_size)
#endif /* CONFIG_VM_DTB_FILE_INPUT */

/**
 * @brief VM information structure in ZVM.
 *
 * @param os_type: the type of the operating system.
 * @param entry_point: the entry point of the vm, when
 * boot from elf file, this is not equal to vm_mem_base.
 * @param vcpu_num: the number of virtual CPUs.
 * @param vm_mem_base: the base address of the vm memory.
 * @param vm_mem_size: the size of the vm memory.
 * @param vm_image_base: the base address of the vm image in disk.
 * @param vm_image_size: the size of the vm image in disk.
 */
struct z_os_info {
    uint16_t    os_type;
    uint16_t    vcpu_num;
    uint64_t    vm_mem_base;
    uint64_t    vm_mem_size;
    uint64_t    vm_load_base;
    uint64_t    vm_image_base;
    uint64_t    vm_image_size;
    uint64_t    entry_point;
    uint64_t    linux_dtb_load_addr;
    uint64_t    linux_dtb_load_size;
};

struct z_os {
    char *name;
    bool is_rtos;
    struct z_os_info info;
};

void guest_memory_manager_init(uint64_t base_addr, uint64_t total_mem);

void free_guest_memory_part(uint64_t addr);

int get_os_type_from_args(const char *os_type);

int get_os_template_type(struct z_os_info guest_os, bool special_cpu,  bool special_mem);

int load_vm_image(struct vm_mem_domain *vmem_domain, struct z_os *os);

int vm_os_create(struct z_os* guest_os, int vm_type);

bool guest_os_is_rtos(uint16_t os_type);

bool allocate_guest_memory_part_check(uint64_t size);

#endif  /* ZEPHYR_INCLUDE_VIRTUALIZATION_OS_H_ */
