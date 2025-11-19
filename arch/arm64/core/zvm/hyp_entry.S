/*
 * Copyright 2024-2025 HNU-ESNL: Guoqi Xie, Chenglai Xiong, Xingyu Hu and etc.
 * Copyright 2024-2025 openEuler SIG-Zephyr
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/toolchain.h>
#include <zephyr/linker/sections.h>
#include <zephyr/zvm/arm/asm.h>

#include "../include/zvm_offsets_short_arch.h"
#include "../core/macro_priv.inc"

_ASM_FILE_PROLOGUE

.macro z_arm64_vect_enter_hyp
	stp	x2, x3,   [sp, #-16]!
	stp	x4, x5,   [sp, #-16]!
	stp	x6, x7,   [sp, #-16]!
	stp	x8, x9,   [sp, #-16]!
	stp	x10, x11, [sp, #-16]!
	stp	x12, x13, [sp, #-16]!
	stp	x14, x15, [sp, #-16]!
	stp	x16, x17, [sp, #-16]!
    stp	x18, lr,  [sp, #-16]!
.endm

.macro z_arm64_vect_exit_hyp
    ldp x18, lr,  [sp], #16
	ldp	x16, x17, [sp], #16
	ldp	x14, x15, [sp], #16
	ldp	x12, x13, [sp], #16
	ldp	x10, x11, [sp], #16
	ldp	x8, x9,   [sp], #16
	ldp	x6, x7,   [sp], #16
	ldp	x4, x5,   [sp], #16
	ldp	x2, x3,   [sp], #16
.endm

.macro save_registers_context base
    stp x0, x1,   [\base, #_zvm_vcpu_ctxt_arch_regs_to_esf_t_x0_x1]
    stp x2, x3,   [\base, #_zvm_vcpu_ctxt_arch_regs_to_esf_t_x2_x3]
    stp x4, x5,   [\base, #_zvm_vcpu_ctxt_arch_regs_to_esf_t_x4_x5]
    stp x6, x7,   [\base, #_zvm_vcpu_ctxt_arch_regs_to_esf_t_x6_x7]
    stp x8, x9,   [\base, #_zvm_vcpu_ctxt_arch_regs_to_esf_t_x8_x9]
    stp x10, x11, [\base, #_zvm_vcpu_ctxt_arch_regs_to_esf_t_x10_x11]
    stp x12, x13, [\base, #_zvm_vcpu_ctxt_arch_regs_to_esf_t_x12_x13]
    stp x14, x15, [\base, #_zvm_vcpu_ctxt_arch_regs_to_esf_t_x14_x15]
	stp x16, x17, [\base, #_zvm_vcpu_ctxt_arch_regs_to_esf_t_x16_x17]
    stp x18, lr,  [\base, #_zvm_vcpu_ctxt_arch_regs_to_esf_t_x18_lr]

    stp x19, x20, [\base, #_zvm_vcpu_ctxt_arch_regs_to_callee_saved_x19_x20]
    stp x21, x22, [\base, #_zvm_vcpu_ctxt_arch_regs_to_callee_saved_x21_x22]
    stp x23, x24, [\base, #_zvm_vcpu_ctxt_arch_regs_to_callee_saved_x23_x24]
    stp x25, x26, [\base, #_zvm_vcpu_ctxt_arch_regs_to_callee_saved_x25_x26]
    stp x27, x28, [\base, #_zvm_vcpu_ctxt_arch_regs_to_callee_saved_x27_x28]

    mrs x4, sp_el0
    stp x29, x4,  [\base, #_zvm_vcpu_ctxt_arch_regs_to_callee_saved_x29_sp_el0]
    mrs x4, sp_el1
    str x4,       [\base, #_zvm_vcpu_ctxt_arch_regs_to_callee_saved_sp_elx]
.endm

.macro load_registers_context base
    ldp x0, x1,   [\base, #_zvm_vcpu_ctxt_arch_regs_to_esf_t_x0_x1]
    ldp x2, x3,   [\base, #_zvm_vcpu_ctxt_arch_regs_to_esf_t_x2_x3]
    ldp x4, x5,   [\base, #_zvm_vcpu_ctxt_arch_regs_to_esf_t_x4_x5]
    ldp x6, x7,   [\base, #_zvm_vcpu_ctxt_arch_regs_to_esf_t_x6_x7]
    ldp x8, x9,   [\base, #_zvm_vcpu_ctxt_arch_regs_to_esf_t_x8_x9]
    ldp x10, x11, [\base, #_zvm_vcpu_ctxt_arch_regs_to_esf_t_x10_x11]
    ldp x12, x13, [\base, #_zvm_vcpu_ctxt_arch_regs_to_esf_t_x12_x13]
    ldp x14, x15, [\base, #_zvm_vcpu_ctxt_arch_regs_to_esf_t_x14_x15]
    ldp x16, x17, [\base, #_zvm_vcpu_ctxt_arch_regs_to_esf_t_x16_x17]
    ldp x18, lr,  [\base, #_zvm_vcpu_ctxt_arch_regs_to_esf_t_x18_lr]

    ldp x19, x20, [\base, #_zvm_vcpu_ctxt_arch_regs_to_callee_saved_x19_x20]
    ldp x21, x22, [\base, #_zvm_vcpu_ctxt_arch_regs_to_callee_saved_x21_x22]
    ldp x23, x24, [\base, #_zvm_vcpu_ctxt_arch_regs_to_callee_saved_x23_x24]
    ldp x25, x26, [\base, #_zvm_vcpu_ctxt_arch_regs_to_callee_saved_x25_x26]
    ldp x27, x28, [\base, #_zvm_vcpu_ctxt_arch_regs_to_callee_saved_x27_x28]

    ldr x4, [\base, #_zvm_vcpu_ctxt_arch_regs_to_callee_saved_sp_elx]
    msr sp_el1, x4
    ldr x4, [\base, #_zvm_vcpu_ctxt_arch_regs_to_callee_saved_x29_sp_el0 + 0x08]
    msr sp_el0, x4
	ldr x4, [\base, #_zvm_vcpu_ctxt_arch_regs_to_esf_t_x4_x5]
	ldr x29, [\base, #_zvm_vcpu_ctxt_arch_regs_to_callee_saved_x29_sp_el0]
.endm

/**
 * @brief VM entry function, where switch to vm context.
 * @x0: vcpu info.
 * @x1: host cpu context
 */
GTEXT(guest_vm_entry)
SECTION_SUBSEC_FUNC(TEXT, __hyp_section, guest_vm_entry)

	/* save hyp context */
    save_registers_context x1

	/* load guest context */
    add x29, x0, 	#_vcpu_arch_to_ctxt
    ldr x29, [x29]
    load_registers_context x29
	isb
    eret


GTEXT(guest_vm_exit_sync)
SECTION_SUBSEC_FUNC(TEXT, __hyp_section, guest_vm_exit_sync)

    stp x0, x1,   [sp, #-16]!
	z_arm64_vect_enter_hyp
	bl z_vm_asm_get_vcpu
    z_arm64_vect_exit_hyp

    add x0, x0, #_vcpu_arch_to_ctxt
	ldr x0, [x0]

	/* store guest context */
	save_registers_context x0
	ldp x4, x5,   [sp], #16
	stp	x4, x5,   [x0, #_zvm_vcpu_ctxt_arch_regs_to_esf_t_x0_x1]

    mov x1, #ARM_VM_EXCEPTION_SYNC
    mrs	x0, isr_el1
	cbz	x0, vm_isr_in_sync
	mov x1, #ARM_VM_EXCEPTION_IRQ_IN_SYNC

vm_isr_in_sync:
	/* Save exception type for next usage. */
	mov x0, x1
	stp	x0, x1, [sp, #-16]!
	bl get_zvm_host_context
	mov x1, x0

	/* load host context */
    load_registers_context x1
	ldp x0, x1,   [sp], #16
    isb
	ret


GTEXT(guest_vm_exit_irq)
SECTION_SUBSEC_FUNC(TEXT, __hyp_section, guest_vm_exit_irq)

    stp x0, x1,   [sp, #-16]!
	z_arm64_vect_enter_hyp
	bl z_vm_asm_get_vcpu
    z_arm64_vect_exit_hyp

    add x0, x0, #_vcpu_arch_to_ctxt
	ldr x0, [x0]

	/* store guest context */
	save_registers_context x0
	ldp x4, x5,   [sp], #16
	stp	x4, x5,   [x0, #_zvm_vcpu_ctxt_arch_regs_to_esf_t_x0_x1]

	/* Save exception type for next usage. */
    mov x1, #ARM_VM_EXCEPTION_IRQ
	mov x0, x1
	stp	x0, x1, [sp, #-16]!
	bl get_zvm_host_context
	mov x1, x0

	/* load host context */
    load_registers_context x1
	ldp x0, x1,   [sp], #16
    isb
	ret
