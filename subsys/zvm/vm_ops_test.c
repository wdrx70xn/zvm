/*
 * Copyright 2024-2025 HNU-ESNL: Guoqi Xie, Chenglai Xiong, Xingyu Hu, Hang Zhao and etc.
 * Copyright 2024-2025 openEuler SIG-Zephyr
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <autoconf.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <zephyr/zvm/zvm.h>
#include <zephyr/zvm/vm_cpu.h>
#include <zephyr/zvm/vm_manager.h>
#include <zephyr/zvm/vm_ops_test.h>
#include <zephyr/zvm/os.h>
#if defined(CONFIG_HEARTBEAT) && defined(CONFIG_ZSHM)
#include <zephyr/zvm/vdev/zshm.h>
#endif

#define MAX_TEST_CMDS 512
#define MAX_ARGC 16
#define MAX_ARG_LEN 64

#define COUNT_CPU(n) +1
#define ZVM_CPU_NUM (0 DT_FOREACH_CHILD(DT_PATH(cpus), COUNT_CPU))

#define REPEAT_TEST_TIMES 10            /* 测试重复次数 */
#define ENABLE_SHUFFLE 1                /* 设为1启用打乱生命周期操作顺序 */
#define STOP_ON_FAIL 1                  /* 遇到第一个报错停止 */

typedef enum {
    VM_UNUSED = 0,
    VM_CREATED,
    VM_RUNNING,
    VM_PAUSED,
    VM_SHUTDOWN
} VMState;

const static char *vm_state_names[] = {
    "UNUSED", "CREATED", "RUNNING", "PAUSED", "SHUTDOWN"
};

typedef struct {
    char cmd_str[128];
    int cmd_id;
} TestCmd;

/* 定义OS类型 */
#define OS_TYPE_ZEPHYR_TEST 0
#define OS_TYPE_LINUX_TEST 1
const static char *oses[] = {"zephyr", "linux"};

#define GUESTS_MEM_SIZE 2560

/* 自动生成测试用例 */
static int generate_vm_test_cases(TestCmd *cases, int max_case_counts, int zephyr_vm_num, int linux_vm_num)
{
    int cmd_counts = 0;
    int max_vm_num = zephyr_vm_num + linux_vm_num;
    int freed_ids[max_vm_num];
    int freed_count;
    int used_bitmap[max_vm_num];
    int max_zephyr_vcpu_num, max_linux_vcpu_num;
    int create_cmd_count;

    VMState vm_state[max_vm_num];
    const char *vm_type[max_vm_num];
    memset(vm_state, 0, sizeof(vm_state));
    memset(vm_type, 0, sizeof(vm_type));

    int live_vm_ids[max_vm_num];

    ZVM_LOG_INFO("Generating VM test cases...\n");
    ZVM_LOG_INFO("Test VM number: %d, Zephyr VMs: %d, Linux VMs: %d\n",
                 max_vm_num, zephyr_vm_num, linux_vm_num);

    for (int round = 0; round < REPEAT_TEST_TIMES && cmd_counts < max_case_counts; round++) {
        int live_count = 0;

        ZVM_LOG_INFO("==== ROUND %d ====\n", round + 1);
        int zephyr_num = zephyr_vm_num;
        int linux_num = linux_vm_num;
        int chosen = 0, mem_chosen = 0;
        int max_guests_mem_size, linux_mem_size = 0;
        const char *os;
        max_zephyr_vcpu_num = CONFIG_MAX_VCPU_PERCPU * (CONFIG_MP_MAX_NUM_CPUS - MAX_GUEST_NUM_LINUX_PCPU - 1);
        max_linux_vcpu_num = (CONFIG_MP_MAX_NUM_CPUS * CONFIG_MAX_VCPU_PERCPU) - max_zephyr_vcpu_num - CONFIG_MAX_VCPU_PERCPU;
        memset(used_bitmap, 0, sizeof(used_bitmap));
        memset(freed_ids, 0, sizeof(freed_ids));
        freed_count = 0;
        max_guests_mem_size = GUESTS_MEM_SIZE;

        /* Step 1: 创建 VM */
        for (int i = 0; i < max_vm_num && cmd_counts < max_case_counts; i++) {
            int id = -1;

            if(freed_count > 0) {
                id = freed_ids[--freed_count];
            } else {
                for (int j = 0; j < max_vm_num; j++) {
                    if (!used_bitmap[j]) {
                        id = j;
                        break;
                    }
                }
            }
            if(id == -1) {
                ZVM_LOG_WARN("used_bitmap is full, cannot create more VMs!\n");
                break;
            }

            /*随机挑选一个类型guest OS, 确保每次循环都能生成不同的随机数*/
            srand(sys_clock_cycle_get_32());
            /*随机挑选启动的vCPU个数*/
            int vcpu_count = 1 + (rand() % CONFIG_MAX_VCPU_PER_VM);
            chosen = rand() & 0x01;
            if((chosen == OS_TYPE_ZEPHYR_TEST) && zephyr_num) {
                os = oses[OS_TYPE_ZEPHYR_TEST];
                max_zephyr_vcpu_num -= vcpu_count;
                max_guests_mem_size -= 128;
                if(max_zephyr_vcpu_num < 0) {
                    max_zephyr_vcpu_num += vcpu_count;
                    ZVM_LOG_WARN("Zephyr vCPU limit exceeded!\n");
                    continue;;
                } else if (max_guests_mem_size < 0) {
                    max_guests_mem_size += 128;
                    ZVM_LOG_WARN("Zephyr memory limit exceeded!\n");
                    continue;;
                }
                zephyr_num --;
            } else if(linux_num){
                os = oses[OS_TYPE_LINUX_TEST];
                max_linux_vcpu_num -= vcpu_count;
                /* 计算内存大小限制 */
                srand(sys_clock_cycle_get_32());
                mem_chosen = rand() & 0x01;
                if(mem_chosen) {
                    /* 1024 MB*/
                    max_guests_mem_size -= 1024;
                    linux_mem_size = 1024;
                } else {
                    /* 512 MB */
                    max_guests_mem_size -= 512;
                    linux_mem_size = 512;
                }

                if(max_linux_vcpu_num < 0) {
                    max_linux_vcpu_num += vcpu_count;
                    ZVM_LOG_WARN("Linux vCPU limit exceeded!\n");
                    continue;;
                } else if (max_guests_mem_size < 0) {
                    max_guests_mem_size += linux_mem_size;
                    ZVM_LOG_WARN("Linux memory limit exceeded!\n");
                    continue;;
                }
                linux_num --;
            }else if(zephyr_num) {
                os = oses[OS_TYPE_ZEPHYR_TEST];
                max_zephyr_vcpu_num -= vcpu_count;
                max_guests_mem_size -= 128;
                if(max_zephyr_vcpu_num < 0) {
                    max_zephyr_vcpu_num += vcpu_count;
                    ZVM_LOG_WARN("Zephyr vCPU limit exceeded!\n");
                    continue;;
                } else if (max_guests_mem_size < 0) {
                    max_guests_mem_size += 128;
                    ZVM_LOG_WARN("Zephyr memory limit exceeded!\n");
                    continue;;
                }
                zephyr_num --;
            }else {
                ZVM_LOG_INFO("All VMs has been created!\n");
                break;
            }

            if(chosen == OS_TYPE_ZEPHYR_TEST) {
                snprintf(cases[cmd_counts].cmd_str, sizeof(cases[cmd_counts].cmd_str),
                     "create -t %s -c %d", os, vcpu_count);
            }else {
                snprintf(cases[cmd_counts].cmd_str, sizeof(cases[cmd_counts].cmd_str),
                     "create -t %s -c %d -m %d", os, vcpu_count, linux_mem_size);
            }

            cases[cmd_counts++].cmd_id = CMD_CREATE_GUEST;

            live_vm_ids[live_count++] = id;
            used_bitmap[id] = 1;
            vm_state[id] = VM_CREATED;
            vm_type[id] = os;
            ZVM_LOG_INFO("[LOG] Created VM ID %d (type=%s), vcpu count: %d\n", id, os, vcpu_count);
        }

        create_cmd_count = cmd_counts;

        /* Step 2: 生命周期流程（每台 VM） */
        for (int i = 0; i < live_count && cmd_counts + 6 < max_case_counts; i++) {
            int id = live_vm_ids[i];
            const char *type = vm_type[id];

            /* 定义生命周期操作表 */
            typedef struct {
                const char *fmt;
                int cmd_id;
                VMState precond;
                VMState postcond;
            } LifeOp;

            LifeOp ops[] = {
                {"run -n %d",     CMD_RUN_GUEST,     VM_CREATED,  VM_RUNNING},
                {"pause -n %d",   CMD_PAUSE_GUEST,   VM_RUNNING,  VM_PAUSED},
                {"run -n %d",     CMD_RUN_GUEST,     VM_PAUSED,   VM_RUNNING},
                {"shutdown -n %d",CMD_SHUTDOWN_GUEST,VM_RUNNING,  VM_SHUTDOWN},
                {"reboot -n %d",  CMD_REBOOT_GUEST,  VM_SHUTDOWN, VM_RUNNING},
                {"delete -n %d",  CMD_DELETE_GUEST,  VM_RUNNING,  VM_UNUSED}
            };

            int op_count = sizeof(ops) / sizeof(ops[0]);
            int order[6] = {0, 1, 2, 3, 4, 5};

            /* Step 2.2: 遍历生命周期命令 */
            for (int k = 0; k < op_count && cmd_counts < max_case_counts; k++) {
                int j = order[k];
                LifeOp *op = &ops[j];

                snprintf(cases[cmd_counts].cmd_str, sizeof(cases[cmd_counts].cmd_str),
                         op->fmt, id);
                cases[cmd_counts++].cmd_id = op->cmd_id;

                ZVM_LOG_INFO("[VM %d | %s] %s -> %s\n",
                       id, type, vm_state_names[vm_state[id]], vm_state_names[op->postcond]);

                vm_state[id] = op->postcond;

                if (op->postcond == VM_UNUSED) {
                    used_bitmap[id] = 0;
                    if (freed_count < max_vm_num) {
                        freed_ids[freed_count++] = id;
                    }
                }
            }
        }

        /* 打乱测试下（乱测）检测稳定性 */
        if(ENABLE_SHUFFLE) {
            if (cmd_counts < create_cmd_count || create_cmd_count <= 0) {
                ZVM_LOG_WARN("Shuffling remaining commands...\n");
                continue;
            }

            srand(sys_clock_cycle_get_32());
            for (int ii = cmd_counts - 1; ii >= create_cmd_count; ii--) {
                /* 在 [0..i] 中选一个随机下标 */
                int jj = create_cmd_count + rand() % (ii + 1 - create_cmd_count);

                /* 交换 cases[i] 和 cases[j] */
                TestCmd tmp = cases[ii];
                cases[ii] = cases[jj];
                cases[jj] = tmp;
            }
        }

        /* Step 3: info（仅显示总信息，不带 -n） */
        snprintf(cases[cmd_counts].cmd_str, sizeof(cases[cmd_counts].cmd_str), "info");
        cases[cmd_counts++].cmd_id = CMD_INFO_GUEST;
    }

    return cmd_counts;
}

/* 分割字符串成 argc / argv */
int parse_command_string(const char *cmd_str, int *argc_out, char ***argv_out) {
    static char *argv[MAX_ARGC];
    static char arg_buf[MAX_ARGC][MAX_ARG_LEN];

    int argc = 0;
    const char *p = cmd_str;

    while (*p && argc < MAX_ARGC) {
        while (*p == ' ') p++;  // 跳过空格
        if (*p == '\0') break;

        int len = 0;
        while (*p != '\0' && *p != ' ' && len < MAX_ARG_LEN - 1) {
            arg_buf[argc][len++] = *p++;
        }
        arg_buf[argc][len] = '\0';
        argv[argc] = arg_buf[argc];
        argc++;
    }

    *argc_out = argc;
    *argv_out = argv;
    return 0;
}

/* 统一入口，给出字符串和 cmd 参数 */
int vm_test_entry(const char *cmd_str, int cmd) {
    int argc = 0;
    char **argv = NULL;

    parse_command_string(cmd_str, &argc, &argv);
    return zvm_vm_operation_template(cmd, argc, argv);
}

void print_failed_cases(const TestCmd* cmds, int end){
    ZVM_LOG_WARN("Failed cases:\n");
    for(int i = 0; i < end; i++){
        ZVM_LOG_INFO("\x1b[31m [%d/%d] zvm %s\x1b[0m\n", i, end, cmds[i].cmd_str);
    }
}

void print_test_arg() {
    ZVM_LOG_INFO("========== Test Configuration ==========\n");
    ZVM_LOG_INFO("| %-25s | %-10s |\n", "Parameter", "Value");
    ZVM_LOG_INFO("-----------------------------------------\n");
    ZVM_LOG_INFO("| %-25s | %-10d |\n", "REPEAT_TEST_TIMES", REPEAT_TEST_TIMES);
    ZVM_LOG_INFO("| %-25s | %-10d |\n", "ENABLE_SHUFFLE", ENABLE_SHUFFLE);
    ZVM_LOG_INFO("| %-25s | %-10d |\n", "STOP_ON_FAIL", STOP_ON_FAIL);

    ZVM_LOG_INFO("| %-25s | %-10d |\n", "VM_RTOS_START_TIMEOUT", VM_RTOS_START_TIMEOUT);
    ZVM_LOG_INFO("| %-25s | %-10d |\n", "VM_LINUX_START_TIMEOUT", VM_LINUX_START_TIMEOUT);
    ZVM_LOG_INFO("| %-25s | %-10d |\n", "VM_RTOS_PAUSE_TIMEOUT", VM_RTOS_PAUSE_TIMEOUT);
    ZVM_LOG_INFO("| %-25s | %-10d |\n", "VM_LINUX_PAUSE_TIMEOUT", VM_LINUX_PAUSE_TIMEOUT);

    ZVM_LOG_INFO("| %-25s | %-10d |\n", "WATCH_VM_START_INTERVAL", WATCH_VM_START_INTERVAL);
    ZVM_LOG_INFO("| %-25s | %-10d |\n", "WATCH_VM_PAUSE_INTERVAL", WATCH_VM_PAUSE_INTERVAL);
    ZVM_LOG_INFO("| %-25s | %-10d |\n", "HEARTBEAT_TIMEOUT_MS", HEARTBEAT_TIMEOUT_MS);
    ZVM_LOG_INFO("=========================================\n");
}

int run_all_generated_tests(bool test_flag, uint8_t zephyr_vm_num, uint8_t linux_vm_num)
{

    if(!test_flag || (zephyr_vm_num <= 0 && linux_vm_num <= 0)) {
        ZVM_LOG_INFO("Warning! Skip tests\n");
        return 0;
    }
    if(zephyr_vm_num + linux_vm_num > CONFIG_MAX_VM_NUM) {
        ZVM_LOG_WARN("Total VM number exceeds maximum limit (%d)\n", CONFIG_MAX_VM_NUM);
        return 0;
    }

    ZVM_LOG_INFO("ZVM test start...\n");
    print_test_arg();
    TestCmd cmds[MAX_TEST_CMDS];
    srand(k_uptime_get());
    int ret = 0;

    int total = generate_vm_test_cases(cmds, MAX_TEST_CMDS, zephyr_vm_num, linux_vm_num);
    ZVM_LOG_INFO("Ready to run all test commads\n");
    for(int i = 0; i<total ;i++){
        ZVM_LOG_INFO("cmd[%02d]: zvm %s\n",i,cmds[i].cmd_str);
    }
    for (int i = 0; i < total; i++) {
        //ZVM_LOG_INFO("[TEST %02d] zvm %s\n", i + 1, cmds[i].cmd_str);
        ZVM_LOG_INFO("\033[0;32m[TEST %02d/%d] zvm %s\033[0m\n", i, total, cmds[i].cmd_str);
        ret = vm_test_entry(cmds[i].cmd_str, cmds[i].cmd_id);
        if (ret != 0) {
            if (ret == -TEST_CMD_SKIP) {
                ZVM_LOG_INFO("\033[0;33m[SKIP] %s test skipped.\033[0m\n", cmds[i].cmd_str);
                continue;
            }
            ZVM_LOG_INFO("\033[0;31m✘ [FAIL] %s test failed. Return code = %d\033[0m\n",
                            cmds[i].cmd_str, ret);
            if(STOP_ON_FAIL){
                ZVM_LOG_INFO("Test stopped because a failure occurs\n");
                print_failed_cases(cmds,i+1);
                return -1;
            }
        } else {
            ZVM_LOG_INFO("\033[0;32m✔ [PASS] %s test passed.\033[0m\n",
                            cmds[i].cmd_str);
        }
        k_sleep(K_MSEC(5000)); /* 延迟5s */
        ZVM_LOG_INFO("--------------------------------------------------\n");
        ZVM_LOG_INFO("--------------------------------------------------\n");
    }
    if(STOP_ON_FAIL){
        ZVM_LOG_INFO("\033[0;32m✔ [PASS] All test passed.\033[0m\n");
    }

    if(STOP_ON_FAIL){
        ZVM_LOG_INFO("\033[0;32m✔ [PASS] All test passed.\033[0m\n");
    }
    return ret;
}
