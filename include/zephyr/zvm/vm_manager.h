/*
 * Copyright 2024-2025 HNU-ESNL: Guoqi Xie, Chenglai Xiong, Xingyu Hu and etc.
 * Copyright 2024-2025 openEuler SIG-Zephyr
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_INCLUDE_VIRTUALIZATION_VM_MANAGER_H_
#define ZEPHYR_INCLUDE_VIRTUALIZATION_VM_MANAGER_H_

#include <errno.h>
#include <zephyr/arch/arm64/cpu.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/printk.h>
#include <zephyr/posix/unistd.h>
#include <zephyr/zvm/vm.h>

#ifdef CONFIG_ZVM_OPERATION_TEST
// 启动RTOS VM等待时间，超时认为没有启动成功
#define VM_RTOS_START_TIMEOUT 10000
// 启动Linux VM等待时间，超时认为没有启动成功
#define VM_LINUX_START_TIMEOUT 45000
// 暂停RTOS VM等待时间，超时认为暂停成功
#define VM_RTOS_PAUSE_TIMEOUT 5000
// 暂停Linux VM等待时间，超时认为暂停成功
#define VM_LINUX_PAUSE_TIMEOUT 10000

// 检测VM启动的间隔，每WATCH_VM_START_INTERVAL查看一次是否启动成功
#define WATCH_VM_START_INTERVAL 2000
// 检测VM暂停的间隔，每WATCH_VM_PAUSE_INTERVAL查看一次是否暂停了
#define WATCH_VM_PAUSE_INTERVAL 2000

#endif
#define CMD_CREATE_GUEST    1
#define CMD_RUN_GUEST       2
#define CMD_PAUSE_GUEST     3
#define CMD_SHUTDOWN_GUEST  4
#define CMD_REBOOT_GUEST    5
#define CMD_DELETE_GUEST    6
#define CMD_INFO_GUEST      7

struct guest_ops {
    int (*create)(struct z_vm *vm);
    int (*run)(struct z_vm *vm);
    int (*pause)(struct z_vm *vm);
    int (*shutdown)(struct z_vm *vm);
    int (*reboot)(struct z_vm *vm);
    int (*delete)(struct z_vm *vm);
    int (*info)(struct z_vm *vm);
};

extern struct guest_ops zvm_guest_ops;

/**
 * @brief Create or find a VM struct from command line.
 * @param cmd_guest: command type: new, run, pause, delete, info
 * @param argc: command line args count
 * @param argv: command line args
 * @return struct z_vm*: pointer to the VM struct
 */
struct z_vm *zvm_get_vm_by_cmd(int cmd_guest, size_t argc, char **argv);

/**
 * @brief Parse command line for VM operations.
 * @param cmd_guest: command type: new, run, pause, delete, info
 * @param vm: pointer to the VM struct
 */
int zvm_vm_ops_entry(struct z_vm *vm, int cmd_guest);

/**
 * @brief Parse command line for VM operations.
 * @param cmd_guest: command type: new, run, pause, delete, info
 * @param argc: command line args count
 * @param argv: command line args
 * @return int: 0(success) or error code
 */
int zvm_vm_operation_template(int cmd_guest, size_t argc, char **argv);


#endif /* ZEPHYR_INCLUDE_VIRTUALIZATION_VM_MANAGER_H_ */
