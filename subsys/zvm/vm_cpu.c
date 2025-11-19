/*
 * Copyright 2024-2025 HNU-ESNL: Guoqi Xie, Chenglai Xiong, Xingyu Hu and etc.
 * Copyright 2024-2025 openEuler SIG-Zephyr
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <kthread.h>
#include <ksched.h>
#include <zephyr/dt-bindings/interrupt-controller/arm-gic.h>
#include <zephyr/zvm/zvm.h>
#include <zephyr/zvm/arm/cpu.h>
#include <zephyr/zvm/arm/switch.h>
#include <zephyr/zvm/arm/timer.h>
#include <zephyr/zvm/vm_device.h>
#include <zephyr/zvm/vm_cpu.h>
#include <zephyr/shell/shell.h>
#include <zephyr/cache.h>
#include <stdio.h>
#include <zephyr/zvm/vdev/vgic_v3.h>

LOG_MODULE_DECLARE(ZVM_MODULE_NAME);

static struct vcpu_pcpu_mapping *vcpu_pcpu_map[CONFIG_MP_MAX_NUM_CPUS];

/* 检查 pCPU 上是否已有来自同一 VM 的 vCPU */
static bool pcpu_has_vm_vcpu(int pcpu, struct z_vcpu *vcpu)
{
    struct vcpu_pcpu_mapping *mapping = vcpu_pcpu_map[pcpu];
    while (mapping) {
        if (mapping->vcpu->vm == vcpu->vm) {
            return true; /* 已有来自同一 VM 的 vCPU */
        }
        mapping = mapping->next;
    }
    return false;
}

/* 添加 vCPU 到 pCPU 的映射 */
static int add_vcpu_to_pcpu(int pcpu, struct z_vcpu *vcpu)
{
    struct vcpu_pcpu_mapping *mapping = k_malloc(sizeof(struct vcpu_pcpu_mapping));
    memset(mapping, 0, sizeof(struct vcpu_pcpu_mapping));

    mapping->vcpu = vcpu;
    mapping->next = vcpu_pcpu_map[pcpu];
    vcpu_pcpu_map[pcpu] = mapping;
    zvm_overall_info->vcpu_use[pcpu]++;

    if (pcpu > CONFIG_MP_MAX_NUM_CPUS - 1 || pcpu < 0)
    {
        ZVM_LOG_INFO(
        "Allocated pCPU for VM:%s vCPU:%d failed\n",
            vcpu->vm->vm_name, vcpu->vcpu_id
        );
        return -1;
    } else {
        ZVM_LOG_INFO(
        "Allocated pCPU:%d for VM:%s vCPU:%d\n",
            pcpu, vcpu->vm->vm_name, vcpu->vcpu_id
        );
        vcpu->pre_alloc_pcpu = pcpu;
        return pcpu;
    }

}

/* 从 pCPU 移除 vCPU 映射 */
static void remove_vcpu_from_pcpu(int pcpu, struct z_vcpu *vcpu)
{
    struct vcpu_pcpu_mapping **prev = &vcpu_pcpu_map[pcpu];
    struct vcpu_pcpu_mapping *curr = vcpu_pcpu_map[pcpu];
    int *vcpu_use = zvm_overall_info->vcpu_use;
    while (curr) {
        if (curr->vcpu == vcpu) {
            *prev = curr->next;
            k_free(curr);
            vcpu_use[pcpu]--;
            return;
        }
        prev = &curr->next;
        curr = curr->next;
    }
}

static inline void get_vcpu_domain_range(bool is_rtos, int *start, int *end)
{
    /* non-RT domain: 1 - MAX_GUEST_NUM_LINUX_PCPU */
    /* RT     domain: MAX_GUEST_NUM_LINUX_PCPU - CONFIG_MP_MAX_NUM_CPUS */
    if (is_rtos) {
        *start = 1 + MAX_GUEST_NUM_LINUX_PCPU;
        *end   = CONFIG_MP_MAX_NUM_CPUS;
    } else {
        *start = 1;
        *end   = 1 + MAX_GUEST_NUM_LINUX_PCPU;
    }
}

bool pre_alloc_vcpu_single(struct z_vcpu *vcpu)
{
    int domain_start, domain_end, pcpu;
    bool found = false;
    int *vcpu_use = zvm_overall_info->vcpu_use;

    get_vcpu_domain_range(vcpu->vm->os->is_rtos, &domain_start, &domain_end);

    //layer 1: find pcpu with 0 vcpu
    for (pcpu = domain_start; pcpu < domain_end; pcpu++) {
        if (vcpu_use[pcpu] == 0)
        {
            found = true;
            break;
        }
    }
    if (vcpu->vm->os->is_rtos)
    {
        goto stop_find;
    }
#if CONFIG_MAX_VCPU_PERCPU == 2
    //layer 2: find pcpu with 1 other vcpu
    if (!found)
    {
        for (pcpu = domain_start; pcpu < domain_end; pcpu++) {
            if (vcpu_use[pcpu] == 1 && !pcpu_has_vm_vcpu(pcpu, vcpu))
            {
                found = true;
                break;
            }
        }
    }
#endif
stop_find:
    if (found && add_vcpu_to_pcpu(pcpu, vcpu)) {
        return true;
    } else {
        ZVM_LOG_INFO(
            "Select pcpu for vm:%s vcpu:%d fail\n",
            vcpu->vm->vm_name, vcpu->vcpu_id
        );
        return false;
    }
}

void vcpu_domain_enable(struct z_vcpu *vcpu)
{
    int i, domain_start, domain_end;
    get_vcpu_domain_range(vcpu->vm->os->is_rtos, &domain_start, &domain_end);

    for (i = domain_start; i < domain_end; i++)
    {
        k_thread_cpu_mask_enable(vcpu->tid, i);
    }
}
/* Disable the CPU mask for domains not owned by the current vCPU. */
void vcpu_another_domain_disable(struct z_vcpu *vcpu)
{
    int i, domain_start, domain_end;
    get_vcpu_domain_range(!vcpu->vm->os->is_rtos, &domain_start, &domain_end);

    for (i = domain_start; i < domain_end; i++)
    {
        k_thread_cpu_mask_disable(vcpu->tid, i);
    }
}
void print_vcpu_usage(void)
{
    int *vcpu_use = zvm_overall_info->vcpu_use;

    ZVM_LOG_INFO("vCPU Allocation Map\n");

    /* divider */
    char divider[100] = "+-------+";
    for (int pcpu = 1; pcpu < CONFIG_MP_MAX_NUM_CPUS; pcpu++) {
        strcat(divider, "-------+");
    }
    printk("%s\n", divider);

    /* CPU header */
    char header[100] = "|       |";
    for (int pcpu = 1; pcpu < CONFIG_MP_MAX_NUM_CPUS; pcpu++) {
        snprintf(header + strlen(header), sizeof(header) - strlen(header), " CPU%-2d |", pcpu);
    }
    printk("%s\n", header);
    printk("%s\n", divider);

    /* vCPU info */
    for (int slot = 0; slot < CONFIG_MAX_VCPU_PERCPU; slot++) {
        char row[100] = {0};
        snprintf(row, sizeof(row), "| Slot%-2d|", slot);

        for (int pcpu = 1; pcpu < CONFIG_MP_MAX_NUM_CPUS; pcpu++) {
            if (slot == 1 && pcpu > MAX_GUEST_NUM_LINUX_PCPU) {
                strcat(row, "   X   |");
            } else if (slot < vcpu_use[pcpu]) {
                strcat(row, "\033[0;32m   √   \033[0m");
                strcat(row, "|");
            } else {
                strcat(row, "       |");
            }
        }
        printk("%s\n", row);
    }

    printk("%s\n", divider);

    /* domain info row */
    char domain_row[150] = "| Dom   |";
    int non_rt_width = MAX_GUEST_NUM_LINUX_PCPU * 7 + 1;
    int rt_width = (CONFIG_MP_MAX_NUM_CPUS - MAX_GUEST_NUM_LINUX_PCPU - 1) * 8 - 2;

    if (MAX_GUEST_NUM_LINUX_PCPU > 0) {
        snprintf(domain_row + strlen(domain_row), sizeof(domain_row) - strlen(domain_row),
                 "%-*s |", non_rt_width, "non-RT");
    }

    if ((CONFIG_MP_MAX_NUM_CPUS - MAX_GUEST_NUM_LINUX_PCPU - 1) > 0) {
        snprintf(domain_row + strlen(domain_row), sizeof(domain_row) - strlen(domain_row),
                 "%-*s |", rt_width, "RT");
    }
    printk("%s\n", domain_row);

    /* bottom_divider */
    char bottom_divider[100] = "+--------";
    for (int pcpu = 1; pcpu < CONFIG_MP_MAX_NUM_CPUS - 1; pcpu++) {
        strcat(bottom_divider, "--------");
    }
    strcat(bottom_divider, "-------+");
    printk("%s\n", bottom_divider);
}

bool vcpu_margin_check(uint16_t os_type, int num)
{
    int domain_start, domain_end, available, pcpu, max_vcpu;
    int *vcpu_use = zvm_overall_info->vcpu_use;

    available = 0;
    get_vcpu_domain_range(guest_os_is_rtos(os_type), &domain_start, &domain_end);

    if (guest_os_is_rtos(os_type)) {
        max_vcpu = 1;
    } else {
        max_vcpu = CONFIG_MAX_VCPU_PERCPU;
    }

    for (pcpu = domain_start; pcpu < domain_end; pcpu++) {
        if (vcpu_use[pcpu] < max_vcpu) {
            available += 1;
        }
    }

    if (num <= available) {
        return true;
    }
    ZVM_LOG_ERR("There are not enough vCPUs.\n");
    print_vcpu_usage();
    return false;
}

void release_vcpu(struct z_vcpu *vcpu)
{
    if (vcpu->pre_alloc_pcpu != -1) {
        remove_vcpu_from_pcpu(vcpu->pre_alloc_pcpu, vcpu);
        vcpu->pre_alloc_pcpu = -1;
    }
}

/**
 * @brief Construct a new vcpu virt irq block. Setting
 * a default description.
 * TODO: all the local irq is inited here, may should be
 * init when vtimer init.
 */
static void init_vcpu_virt_irq_desc(struct vcpu_virt_irq_block *virq_block, int vcpu_id)
{
    int i;
    struct virt_irq_desc *desc;
    for(i = 0; i < VM_GLOBAL_VIRQ_NR; i++) {
        desc = &virq_block->vcpu_virt_irq_desc[i];
        desc->id = VM_INVALID_DESC_ID;
        if (i < 32) {
            desc->pirq_num = i;
        } else {
            desc->pirq_num = VM_INVALID_PIRQ_NUM;
        }
        desc->virq_num = i;
        desc->prio = 0;
        desc->src_cpu = 0;
        desc->vcpu_id = vcpu_id;
        atomic_set(&desc->bind_hwirq_flag, false);
        desc->virq_states = 0;
        desc->vm_id = DEFAULT_VM;
        desc->type = 0;
        atomic_set(&desc->level_triggered, VIRQ_LEVEL_LOW);

        sys_dnode_init(&(desc->desc_node));
    }
}

static void ALWAYS_INLINE save_vcpu_context(struct k_thread *thread)
{
    arch_vcpu_context_save(thread->vcpu_struct);
}

static void ALWAYS_INLINE load_vcpu_context(struct k_thread *thread)
{
    struct z_vcpu *vcpu = thread->vcpu_struct;
    arch_vcpu_context_load(thread->vcpu_struct);
    vcpu->resume_signal = false;
}

static void vcpu_context_switch(struct k_thread *new_thread,
            struct k_thread *old_thread)
{
    struct z_vcpu *old_vcpu = NULL, *new_vcpu = NULL;
    if (VCPU_THREAD(old_thread)) {
        old_vcpu = old_thread->vcpu_struct;
        save_vcpu_context(old_thread);
        switch (atomic_get(&old_vcpu->vcpu_state)) {
        case _VCPU_STATE_RUNNING:
            atomic_set(&old_vcpu->vcpu_state, _VCPU_STATE_READY);
            break;
        case _VCPU_STATE_RESET:
            ZVM_LOG_WARN("Do not support vm reset! \n");
            break;
        case _VCPU_STATE_PAUSED:
            vm_vdev_pause(old_vcpu);
            break;
        case _VCPU_STATE_HALTED:
            vm_vdev_pause(old_vcpu);
            break;
        default:
            break;
        }
        barrier_dsync_fence_full();
    }
    if (VCPU_THREAD(new_thread)) {
        new_vcpu = new_thread->vcpu_struct;
        if (atomic_get(&new_vcpu->vcpu_state) != _VCPU_STATE_READY && !new_vcpu->deleting) {
            ZVM_LOG_ERR("%s vCPU%d is not ready vcpu state: %lx.\n",
                new_vcpu->vm->vm_name, new_vcpu->vcpu_id, atomic_get(&new_vcpu->vcpu_state));
            ZVM_LOG_ERR("new vcpu (ID: %d) in %s, pre_alloc_pcpu: %d\n", new_vcpu->vcpu_id,
                new_vcpu->vm->vm_name, new_vcpu->pre_alloc_pcpu);
        }
        load_vcpu_context(new_thread);
        atomic_set(&new_vcpu->vcpu_state, _VCPU_STATE_RUNNING);
        barrier_dsync_fence_full();
    }
}

static int vcpu_state_to_ready(struct z_vcpu *vcpu)
{
    uint16_t cur_state = atomic_get(&vcpu->vcpu_state);
    struct k_thread *thread = vcpu->work->vcpu_thread;

    vcpu->hcpu_cycles = sys_clock_cycle_get_32();

    switch (cur_state)
    {
    case _VCPU_STATE_UNKNOWN:
    case _VCPU_STATE_READY:
        atomic_set(&vcpu->vcpu_state, _VCPU_STATE_READY);
        k_thread_start(thread);
        break;
    case _VCPU_STATE_RUNNING:
        vcpu->resume_signal = true;
        break;
    case _VCPU_STATE_RESET:
    case _VCPU_STATE_PAUSED:
        atomic_set(&vcpu->vcpu_state, _VCPU_STATE_READY);
        k_wakeup(thread);
        /* enable internal interrupt for vcpu. */
        load_enable_internal_int(vcpu, true);
        break;
    default:
        ZVM_LOG_WARN("Invalid cpu state! \n");
        return -EINVAL;
    }

    return 0;
}

static int vcpu_state_to_running(struct z_vcpu *vcpu)
{
    ARG_UNUSED(vcpu);
    ZVM_LOG_WARN("No thing to do, running state may be auto switched. \n");
    return 0;
}

static int vcpu_state_to_reset(struct z_vcpu *vcpu)
{
    uint16_t cur_state = atomic_get(&vcpu->vcpu_state);
    struct k_thread *thread = vcpu->work->vcpu_thread;

    switch (cur_state) {
    case _VCPU_STATE_READY:
        move_thread_to_end_of_prio_q(thread);
#if defined(CONFIG_SMP) &&  defined(CONFIG_SCHED_IPI_SUPPORTED)
	    arch_sched_broadcast_ipi();
#endif
        break;
    case _VCPU_STATE_RESET:
        break;
    case _VCPU_STATE_RUNNING:
    case _VCPU_STATE_PAUSED:
        arch_vcpu_init(vcpu);
        break;
    default:
        ZVM_LOG_WARN("Invalid cpu state here. \n");
        return -EINVAL;
        break;
    }
    vcpu->resume_signal = false;
    return 0;
}

static int vcpu_state_to_paused(struct z_vcpu *vcpu)
{
    uint16_t cur_state = atomic_get(&vcpu->vcpu_state);
    struct k_thread *thread = vcpu->work->vcpu_thread;

    switch (cur_state) {
    case _VCPU_STATE_READY:
    case _VCPU_STATE_RUNNING:
        vcpu->resume_signal = false;
        save_disable_internal_int(vcpu, true);
        k_thread_suspend(thread);
        break;
    case _VCPU_STATE_RESET:
    case _VCPU_STATE_PAUSED:
    case _VCPU_STATE_UNKNOWN:
        ZVM_LOG_INFO("CPU state: %d, no need to pause.\n", cur_state);
        break;
    default:
        ZVM_LOG_INFO("Unexpected CPU state: %d\n", cur_state);
        return -EINVAL;
    }
    return 0;
}

static int vcpu_state_to_halted(struct z_vcpu *vcpu)
{
    uint16_t cur_state = atomic_get(&vcpu->vcpu_state);
    struct k_thread *thread = vcpu->work->vcpu_thread;

    switch (cur_state) {
    case _VCPU_STATE_READY:
    case _VCPU_STATE_RUNNING:
    case _VCPU_STATE_PAUSED:
        // thread->base.thread_state |= _THREAD_VCPU_NO_SWITCH;
        k_thread_abort(thread);
        k_thread_join(thread, K_FOREVER);
        break;
    case _VCPU_STATE_RESET:
    case _VCPU_STATE_UNKNOWN:
        vm_delete(vcpu->vm);
        break;
    default:
        ZVM_LOG_WARN("Invalid cpu state here. \n");
        return -EINVAL;
    }

    return 0;
}

static int vcpu_state_to_unknown(struct z_vcpu *vcpu)
{
    ARG_UNUSED(vcpu);
    return 0;
}

/**
 * @brief Vcpu scheduler for switch vcpu to different states.
 */
int vcpu_state_switch(struct z_vcpu *vcpu, uint16_t new_state)
{
    int ret = 0;
    uint16_t cur_state = atomic_get(&vcpu->vcpu_state);

    if (cur_state == new_state) {
        return ret;
    }
    switch (new_state) {
    case _VCPU_STATE_READY:
        ret = vcpu_state_to_ready(vcpu);
        break;
    case _VCPU_STATE_RUNNING:
        ret = vcpu_state_to_running(vcpu);
        break;
    case _VCPU_STATE_RESET:
        ret = vcpu_state_to_reset(vcpu);
        break;
    case _VCPU_STATE_PAUSED:
        ret = vcpu_state_to_paused(vcpu);
        break;
    case _VCPU_STATE_HALTED:
        ret = vcpu_state_to_halted(vcpu);
        break;
    case _VCPU_STATE_UNKNOWN:
        ret = vcpu_state_to_unknown(vcpu);
        break;
    default:
        ZVM_LOG_ERR("Invalid state here. \n");
        ret = -EINVAL;
        break;
    }
    if (ret < 0) {
        ZVM_LOG_WARN("vcpu state switch failed, vcpu_id: %d, cur_state: %d, new_state: %d\n",
            vcpu->vcpu_id, cur_state, new_state);
        return ret;
    }
    atomic_set(&vcpu->vcpu_state, new_state);
    k_sleep(K_MSEC(100));

    return ret;

}

void do_vcpu_swap(struct k_thread *new_thread, struct k_thread *old_thread)
{
    struct z_vcpu *vcpu;
    ARG_UNUSED(vcpu);

    if(new_thread == old_thread){
        return;
    }

#ifdef CONFIG_SMP
    vcpu_context_switch(new_thread, old_thread);
#else
    if (old_thread && VCPU_THREAD(old_thread)) {
        save_vcpu_context(old_thread);
    }
    if (new_thread && VCPU_THREAD(new_thread)) {
        load_vcpu_context(new_thread);
    }
#endif /* CONFIG_SMP */
}

int vcpu_ipi_scheduler(uint32_t cpu_mask, uint32_t timeout)
{
    ARG_UNUSED(timeout);
#ifdef CONFIG_ARCH_HAS_DIRECTED_IPIS
		arch_sched_directed_ipi(cpu_mask);
#else
		arch_sched_broadcast_ipi();
#endif
    return 0;
}

static void flush_local_cpu_vm_context(void)
{
    __asm__ volatile (
        "dsb    ishst\n"
        "tlbi   vmalle1\n"
        "tlbi   alle2\n"
        "tlbi   alle1\n"
        "ic     iallu\n"
        "dsb    ish\n"
        "isb\n"
        ::: "memory"
    );
}

int vcpu_thread_entry(struct z_vcpu *vcpu)
{
    bool halted, no_pending = true;
    int ret = 0, i;

    flush_local_cpu_vm_context();
    while (true) {
        ret = arch_vcpu_run(vcpu);

        halted = (ret < 0 || vcpu->vm->vm_status == VM_STATE_HALT);
        if (halted) {
            for (i = 0; i < VM_GLOBAL_VIRQ_NR; i++) {
                if (vgic_is_virq_pending_lock(vcpu, i, BIT(vcpu->cpu))) {
                    no_pending = false;
                    break;
                }
            }

            if (halted && no_pending) {
                ZVM_LOG_INFO("vCPU-%d exit\n", vcpu->vcpu_id);
                break;
            }
            no_pending = true;
        }
    };

    atomic_dec(&vcpu->vm->exist_vcpu);
    flush_local_cpu_vm_context();
    return ret;
}

struct z_vcpu *vm_vcpu_init(struct z_vm *vm, uint16_t vcpu_id, char *vcpu_name)
{
    uint16_t vm_prio;
    struct z_vcpu *vcpu;
    struct vcpu_work *vwork;

    vcpu = (struct z_vcpu *)k_malloc(sizeof(struct z_vcpu));
    memset(vcpu, 0, sizeof(struct z_vcpu));
    if (!vcpu) {
        ZVM_LOG_ERR("Allocate vcpu space failed");
        return NULL;
    }

    vcpu->arch = (struct vcpu_arch *)k_malloc(sizeof(struct vcpu_arch));
    memset(vcpu->arch, 0, sizeof(struct vcpu_arch));
    if (!vcpu->arch) {
        ZVM_LOG_ERR("Init vcpu->arch failed");
        k_free(vcpu);
        return  NULL;
    }

    /* init vcpu virt irq block. */
    atomic_set(&vcpu->vcpu_state, 0);
    vcpu->virq_block.vwfi.priv = NULL;
    vcpu->virq_block.vwfi.state = false;
    vcpu->virq_block.vwfi.yeild_count = 0;
    ZVM_SPINLOCK_INIT(&vcpu->virq_block.vwfi.wfi_lock);
    sys_dlist_init(&vcpu->virq_block.pending_irqs);
    sys_dlist_init(&vcpu->virq_block.active_irqs);
    ZVM_SPINLOCK_INIT(&vcpu->virq_block.spinlock);
    init_vcpu_virt_irq_desc(&vcpu->virq_block, vcpu_id);
    ZVM_SPINLOCK_INIT(&vcpu->vcpu_lock);

    if (vm->os->is_rtos) {
        vm_prio = VCPU_RT_PRIO;
    }else{
        vm_prio = VCPU_NORT_PRIO;
    }
    vcpu->vm = vm;

    /* vt_stack must be aligned, So we allocate memory with aligned block */
    vwork = (struct vcpu_work *)k_aligned_alloc(0x10, sizeof(struct vcpu_work));
    memset(vwork, 0, sizeof(struct vcpu_work));
    if (!vwork) {
        ZVM_LOG_ERR("Create vwork error!");
        return NULL;
    }

    /* init tast_vcpu_thread struct here */
    vwork->vcpu_thread = (struct k_thread *)k_malloc(sizeof(struct k_thread));
    memset(vwork->vcpu_thread, 0, sizeof(struct k_thread));
    if (!vwork->vcpu_thread) {
        ZVM_LOG_ERR("Init thread struct error here!");
        return  NULL;
    }
    /*TODO: In this stage, the thread is marked as a kernel thread,
    For system safe, we will modified it later.*/
    k_tid_t tid = k_thread_create(vwork->vcpu_thread, vwork->vt_stack,
            VCPU_THREAD_STACKSIZE, (void *)vcpu_thread_entry, vcpu, NULL, NULL,
			vm_prio, 0, K_FOREVER);
    vcpu->tid = tid;
    vcpu->vcpu_id = vcpu_id;
    strcpy(tid->name, vcpu_name);

    /* SMP support*/
#ifdef CONFIG_SCHED_CPU_MASK
    /**
     * Due to the default 'new_thread->base.cpu_mask=1',
     * BIT(0) must be cleared when enable other mask bit
     * when CONFIG_SCHED_CPU_MASK_PIN_ONLY=y.
    */
    k_thread_cpu_mask_disable(tid, 0);

    #ifdef CONFIG_SCHED_CPU_MASK_PIN_ONLY
    k_thread_cpu_mask_clear(tid);
    #endif

    if(!pre_alloc_vcpu_single(vcpu)) {
        ZVM_LOG_WARN("No suitable idle cpu for Guest OS! \n");
        k_free(vwork->vcpu_thread);
        return  NULL;
    }

    /* Enable running on 1 pCPU */
    k_thread_cpu_mask_enable(tid, vcpu->pre_alloc_pcpu);

    ZVM_LOG_INFO(
        "Enable vCPU %d running on pCPU %d \n",
        vcpu->vcpu_id,
        vcpu->pre_alloc_pcpu
    );
    /* disable non-self vcpu-domain */
    vcpu_another_domain_disable(vcpu);

    #ifndef CONFIG_SCHED_CPU_MASK_PIN_ONLY
    /* enable self vcpu-domain */
    vcpu_domain_enable(vcpu);
    #endif

    vcpu->cpu = vcpu->pre_alloc_pcpu;
#endif /* CONFIG_SCHED_CPU_MASK */

    /* create a new thread and store it in work struct */
    vwork->vcpu_thread->vcpu_struct = vcpu;

    vcpu->work = vwork;
    /* init vcpu timer*/
    vcpu->hcpu_cycles = 0;
    vcpu->paused_cycles = 0;
    vcpu->exit_type = 0;
    atomic_set(&vcpu->vcpu_state, _VCPU_STATE_UNKNOWN);
    atomic_set(&vcpu->vcpuipi_count, 0);
    atomic_set(&vcpu->hcpuipi_count, 0);
    vcpu->resume_signal = false;
    vcpu->deleting = false;
    vcpu->is_first_run = true;
    vcpu->remain_exist_irq = false;

    if (arch_vcpu_init(vcpu)) {
        k_free(vcpu);
        return  NULL;
    }

    return vcpu;
}

int vm_vcpu_deinit(struct z_vcpu *vcpu)
{
    int ret = 0;
    ret = arch_vcpu_deinit(vcpu);
    if (ret) {
        ZVM_LOG_WARN("Deinit arch vcpu error!");
        return ret;
    }
    k_thread_abort(vcpu->work->vcpu_thread);

    K_SPINLOCK(&z_thread_monitor_lock) {
        k_free(vcpu->work->vcpu_thread);
    }
    k_free(vcpu->work);
    k_free(vcpu->arch);
    k_free(vcpu);

    return ret;
}

int vm_vcpu_ready(struct z_vcpu *vcpu)
{
    return vcpu_state_switch(vcpu, _VCPU_STATE_READY);
}

int vm_vcpu_pause(struct z_vcpu *vcpu)
{
    return vcpu_state_switch(vcpu, _VCPU_STATE_PAUSED);
}

int vm_vcpu_halt(struct z_vcpu *vcpu)
{
    return vcpu_state_switch(vcpu, _VCPU_STATE_HALTED);
}

int vm_vcpu_reset(struct z_vcpu *vcpu)
{
    return vcpu_state_switch(vcpu, _VCPU_STATE_RESET);
}
