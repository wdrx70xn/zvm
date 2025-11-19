/*
 * Copyright 2024-2025 HNU-ESNL: Guoqi Xie, Chenglai Xiong, Xingyu Hu and etc.
 * Copyright 2024-2025 openEuler SIG-Zephyr
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_ZVM_VM_CPU_H_
#define ZEPHYR_INCLUDE_ZVM_VM_CPU_H_

#include <zephyr/kernel.h>
#include <zephyr/kernel/thread.h>
#include <zephyr/kernel_structs.h>
#ifdef CONFIG_ARM64
#include <zephyr/zvm/arm/cpu.h>
#include <zephyr/zvm/arm/mmu.h>
#include <zephyr/zvm/arm/switch.h>
#include <zephyr/zvm/arm/timer.h>
#endif

/**
 * @brief We should define overall priority for zvm system.
 * A total of 15 priorities are defined by setting
 * CONFIG_NUM_PREEMPT_PRIORITIES = 15 and can be divided into three categories:
 * 1    ->   5: high real-time requirement and very critical to the system.
 * 6    ->  10: no real-time requirement and very critical to the system.
 * 10   ->  15: normal.
 */
#define RT_VM_WORK_PRIORITY         (5)
#define NORT_VM_WORK_PRIORITY       (10)

#ifdef CONFIG_PREEMPT_ENABLED
/* positive num */
#define VCPU_RT_PRIO        RT_VM_WORK_PRIORITY
#define VCPU_NORT_PRIO      NORT_VM_WORK_PRIORITY
#else
/* negetive num */
#define VCPU_RT_PRIO        K_HIGHEST_THREAD_PRIO + RT_VM_WORK_PRIORITY
#define VCPU_NORT_PRIO      K_HIGHEST_THREAD_PRIO + NORT_VM_WORK_PRIORITY
#endif

#define VCPU_IPI_MASK_ALL   (0xffffffff)

#define DEFAULT_VCPU         (0)

#if defined(CONFIG_SOC_RK3568)
    #define MAX_GUEST_NUM_LINUX_PCPU 2
#elif defined(CONFIG_SOC_RK3588) || defined(CONFIG_SOC_QEMU_MAX)
    #define MAX_GUEST_NUM_LINUX_PCPU 3
#endif

/* Tracks the vCPU assignment on each physical CPU (pCPU) */
struct vcpu_pcpu_mapping {
    struct z_vcpu *vcpu;             /* Pointer to the assigned vCPU */
    struct vcpu_pcpu_mapping *next;  /* Linked list node */
};

/**
 * @brief allocate a vcpu struct and init it.
 */
struct z_vcpu *vm_vcpu_init(struct z_vm *vm, uint16_t vcpu_id, char *vcpu_name);

/**
 * @brief Check if there is a sufficient number of vCPUs.
 */
bool vcpu_margin_check(uint16_t os_type, int num);

/**
 * @brief release vcpu struct.
 */
int vm_vcpu_deinit(struct z_vcpu *vcpu);

/**
 * @brief the vcpu has below state:
 * running: vcpu is running, and is allocated to physical cpu.
 * ready: prepare to running.
*/
int vm_vcpu_ready(struct z_vcpu *vcpu);
int vm_vcpu_pause(struct z_vcpu *vcpu);
int vm_vcpu_halt(struct z_vcpu *vcpu);
int vm_vcpu_reset(struct z_vcpu *vcpu);

/**
 * @brief Entry point for vCPU thread execution.
 * This function defines the main loop or logic executed by a virtual CPU.
 *
 * @param vcpu Pointer to the vCPU structure.
 */
int vcpu_thread_entry(struct z_vcpu *vcpu);

/**
 * @brief Switch the state of a virtual CPU thread.
 *
 * Changes the vCPU thread state to the given new state.
 *
 * @param thread Pointer to the kernel thread structure.
 * @param new_state New state to switch to.
 */
int vcpu_state_switch(struct z_vcpu *vcpu, uint16_t new_state);

/**
 * @brief Perform a context switch between two vCPU threads.
 *
 * Swaps execution from the old vCPU thread to the new one.
 *
 * @param new_thread Pointer to the new thread to be scheduled in.
 * @param old_thread Pointer to the old thread being switched out.
 */
void do_vcpu_swap(struct k_thread *new_thread, struct k_thread *old_thread);

/**
 * @brief Send IPI to trigger system scheduler to schedule vCPUs.
 *
 * This function sends an inter-processor interrupt (IPI) to notify the system
 * scheduler to schedule vCPUs specified by the CPU mask.
 *
 * @param cpu_mask Bitmask indicating target CPUs.
 * @param timeout Timeout value in milliseconds (or system-specific unit).
 * @return 0.
 */
int vcpu_ipi_scheduler(uint32_t cpu_mask, uint32_t timeout);

/**
 * @brief Print the usage status of all vCPUs.
 *
 * Displays current vCPU-to-pCPU mappings and usage information.
 */
void print_vcpu_usage(void);

/**
 * @brief Release the vCPU from its associated pCPU mapping.
 *
 * This function removes the given vCPU from the vcpu-to-pcpu mapping structure.
 *
 * @param vcpu Pointer to the vCPU to be released.
 */
void release_vcpu(struct z_vcpu *vcpu);

#endif /* ZEPHYR_INCLUDE_ZVM_VM_CPU_H_ */
