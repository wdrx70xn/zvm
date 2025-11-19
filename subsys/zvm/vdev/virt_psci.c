/*
 * Copyright 2024-2025 HNU-ESNL: Guoqi Xie, Chenglai Xiong, Xingyu Hu and etc.
 * Copyright 2024-2025 openEuler SIG-Zephyr
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include <../drivers/pm_cpu_ops/pm_cpu_ops_psci.h>
#include <zephyr/zvm/vm_manager.h>
#include <zephyr/zvm/zvm.h>
#include <zephyr/zvm/arm/cpu.h>
#include <zephyr/zvm/vdev/virt_psci.h>
#include <zephyr/cache.h>

LOG_MODULE_DECLARE(ZVM_MODULE_NAME);

static uint32_t psci_get_function_id(struct z_vcpu *vcpu, arch_commom_regs_t *arch_ctxt)
{
    uint64_t reg_value = *find_index_reg(0, arch_ctxt);
    return reg_value & ~((uint32_t)0);
}

static void psci_system_off(struct z_vcpu *vcpu, arch_commom_regs_t *arch_ctxt)
{
	zvm_vm_ops_entry(vcpu->vm, CMD_SHUTDOWN_GUEST);
}

static void psci_system_reset(struct z_vcpu *vcpu, arch_commom_regs_t *arch_ctxt)
{
	zvm_vm_ops_entry(vcpu->vm, CMD_REBOOT_GUEST);
}

static inline void psci_set_reg(uint32_t psci_fn, struct z_vcpu *vcpu,
					arch_commom_regs_t *arch_ctxt,
					uint32_t reg, unsigned long val)
{
    uint64_t *reg_value;
    reg_value = find_index_reg(reg, arch_ctxt);
    *reg_value = (uint64_t)val;
}

uint64_t psci_vcpu_suspend(struct z_vcpu *vcpu, arch_commom_regs_t *arch_ctxt)
{
    return PSCI_RET_SUCCESS;
}

uint64_t psci_vcpu_off(struct z_vcpu *vcpu, arch_commom_regs_t *arch_ctxt)
{
    return PSCI_RET_SUCCESS;
}

uint64_t psci_vcpu_affinity_info(struct z_vcpu *vcpu, arch_commom_regs_t *arch_ctxt)
{
    return PSCI_RET_SUCCESS;
}

uint64_t psci_vcpu_migration(struct z_vcpu *vcpu, arch_commom_regs_t *arch_ctxt)
{
    ZVM_LOG_WARN("PSCI_0_2_FN_MIGRATE\n");
    ZVM_LOG_WARN("do not support now! \n");
    return -1;
}

uint64_t psci_vcpu_migration_info_type(struct z_vcpu *vcpu, arch_commom_regs_t *arch_ctxt)
{
    return PSCI_0_2_TOS_MP;
}

uint64_t psci_vcpu_other(unsigned long psci_func)
{
    ZVM_LOG_WARN("PSCI_0_2_FN_OTHER: %lx \n", psci_func);
    ZVM_LOG_WARN("do not support now! \n");
    return -1;
}

uint64_t psci_vcpu_on(struct z_vcpu *vcpu, arch_commom_regs_t *arch_ctxt)
{
	bool get_vcpu = false;
    uint64_t cpu_id;
	uint64_t context_id;
	uint64_t target_pc;
	uint64_t mpid_mask;
    struct zvm_vcpu_context *ctxt;
    struct z_vm *vm = NULL;

#if defined(CONFIG_SOC_RK3568) || defined(CONFIG_SOC_RK3588)
	mpid_mask = BIT64_MASK(12);
#elif defined(CONFIG_SOC_D3000)
	mpid_mask = BIT64_MASK(20);
#else
	mpid_mask = BIT64_MASK(4);
#endif
	vm = vcpu->vm;
    cpu_id = arch_ctxt->esf_handle_regs.x1;

	if (cpu_id == 0) {
		ZVM_LOG_ERR("psci_vcpu_on cpu_id == 0, secondary cpu_id must > 0\n");
		return -1;
	}

#ifdef CONFIG_ZVM_SMP_DEBUG_INFO
	show_cpu_node_info_list();
	show_cpu_vmpidr_el2_list();
#endif

	for (size_t i = 0; i < vm->vcpu_num; i++) {
		if (cpu_id == (vm->vcpus[i]->arch->ctxt.sys_regs[VCPU_MPIDR_EL1] & mpid_mask)) {
			ZVM_LOG_INFO("psci vcpu on: cpu_id %llx VCPU_MPIDR_EL1 %llx\n",
				cpu_id, vm->vcpus[i]->arch->ctxt.sys_regs[VCPU_MPIDR_EL1]);
			vcpu = vm->vcpus[i];
			cpu_id = vm->vcpus[i]->arch->ctxt.sys_regs[VCPU_MPIDR_EL1];
			get_vcpu = true;
			break;
		}
	}
	if (!get_vcpu) {
		return PSCI_RET_DENIED;
	}
    target_pc = arch_ctxt->esf_handle_regs.x2;
    context_id = arch_ctxt->esf_handle_regs.x3;
    ctxt = &vcpu->arch->ctxt;
    ctxt->regs.pc = target_pc;
	vcpu->arch->ctxt.regs.esf_handle_regs.x1 = arch_ctxt->esf_handle_regs.x1;
	vcpu->arch->ctxt.sys_regs[VCPU_MPIDR_EL1] = cpu_id;

    vm_vcpu_ready(vcpu);
    return PSCI_RET_SUCCESS;
}

/*
 * x0:    function_id
 * x1-x3: psci function args
 * x0-x4: ret
*/
static int psci_0_2_call(struct z_vcpu *vcpu, arch_commom_regs_t *arch_ctxt)
{
	uint32_t psci_fn = psci_get_function_id(vcpu, arch_ctxt);
	uint32_t val;

	switch (psci_fn) {
	case PSCI_0_2_FN_PSCI_VERSION:
		/*
		 * Bits[31:16] = Major Version = 0
		 * Bits[15:0] = Minor Version = 2
		 */
		val = 2;
		break;
	case PSCI_0_2_FN_CPU_SUSPEND:
	case PSCI_0_2_FN64_CPU_SUSPEND:
		val = psci_vcpu_suspend(vcpu, arch_ctxt);
		break;
	case PSCI_0_2_FN_CPU_OFF:
		psci_vcpu_off(vcpu, arch_ctxt);
		val = PSCI_RET_SUCCESS;
		break;
	case PSCI_0_2_FN_CPU_ON:
	case PSCI_0_2_FN64_CPU_ON:
		val = psci_vcpu_on(vcpu, arch_ctxt);
		break;
	case PSCI_0_2_FN_AFFINITY_INFO:
	case PSCI_0_2_FN64_AFFINITY_INFO:
		val = psci_vcpu_affinity_info(vcpu, arch_ctxt);
		break;
	case PSCI_0_2_FN_MIGRATE:
	case PSCI_0_2_FN64_MIGRATE:
		val = psci_vcpu_migration(vcpu, arch_ctxt);
		break;
	case PSCI_0_2_FN_MIGRATE_INFO_TYPE:
		/*
		 * Trusted OS is MP hence does not require migration
	         * or
		 * Trusted OS is not present
		 */
		val = psci_vcpu_migration_info_type(vcpu, arch_ctxt);
		break;
	case PSCI_0_2_FN_MIGRATE_INFO_UP_CPU:
	case PSCI_0_2_FN64_MIGRATE_INFO_UP_CPU:
		val = PSCI_RET_NOT_SUPPORTED;
		break;
	case PSCI_0_2_FN_SYSTEM_OFF:
		psci_system_off(vcpu, arch_ctxt);
		/*
		 * We should'nt be going back to guest VCPU after
		 * receiving SYSTEM_OFF request.
		 *
		 * If we accidently resume guest VCPU after SYSTEM_OFF
		 * request then guest VCPU should see internal failure
		 * from PSCI return value. To achieve this, we preload
		 * r0 (or x0) with PSCI return value INTERNAL_FAILURE.
		 */
		val = PSCI_RET_INTERNAL_FAILURE;
		break;
	case PSCI_0_2_FN_SYSTEM_RESET:
		psci_system_reset(vcpu, arch_ctxt);
		/*
		 * Same reason as SYSTEM_OFF for preloading r0 (or x0)
		 * with PSCI return value INTERNAL_FAILURE.
		 */
		val = PSCI_RET_INTERNAL_FAILURE;
		break;
	default:
        ZVM_LOG_INFO("UNKNOWN PSCI FUNCTION ID\n ");
		return -1;
	}

	if(val != PSCI_RET_INTERNAL_FAILURE) {
		psci_set_reg(psci_fn, vcpu, arch_ctxt, 0, val);
	}

	return 0;
}

int do_psci_call(struct z_vcpu *vcpu, arch_commom_regs_t *arch_ctxt)
{
    if (!vcpu || !arch_ctxt) {
		return -1;
	}
	barrier_isync_fence_full();
    /*TODO: support psci-1.0 */
	return psci_0_2_call(vcpu, arch_ctxt);
}