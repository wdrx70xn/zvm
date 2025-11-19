# Copyright 2024-2025 HNU-ESNL: Guoqi Xie, Chenglai Xiong, Xingyu Hu and etc.
# Copyright 2024-2025 openEuler SIG-Zephyr
# SPDX-License-Identifier: Apache-2.0

zephyr_sources_ifdef(
    CONFIG_ZVM
    virt_psci.c
)

zephyr_sources_ifdef(
    CONFIG_ARM64
    vgic_common.c
    vgic_v3.c
)

zephyr_sources_ifdef(
    CONFIG_VM_VSERIAL
    vserial.c
    vpl011.c
)

zephyr_sources_ifdef(
    CONFIG_VM_FIQ_DEBUGGER
    fiq_debugger.c
)

zephyr_sources_ifdef(
    CONFIG_VM_SYSCON_CONTROLLER
    virt_syscon.c
)

zephyr_sources_ifdef(
    CONFIG_VM_CLOCK_SYSTEM_CONTROLLER
    virt_clock_syscon.c
)

zephyr_sources_ifdef(
    CONFIG_SOC_QEMU_MAX
    pt_device_qemu_max.c
)

zephyr_sources_ifdef(
    CONFIG_SOC_FAMILY_ROCKCHIP
    rk_linuxdev.c
)

zephyr_sources_ifdef(
    CONFIG_SOC_RK3568
    pt_device_roc_rk3568_pc.c
)

zephyr_sources_ifdef(
    CONFIG_ZSHM
    zshm.c
)
