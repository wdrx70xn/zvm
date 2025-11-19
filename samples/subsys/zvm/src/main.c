/*
 * Copyright 2024-2025 HNU-ESNL: Guoqi Xie, Chenglai Xiong, Xingyu Hu and etc.
 * Copyright 2024-2025 openEuler SIG-Zephyr
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/zvm/vm_ops_test.h>
#include <zephyr/zvm/vm_device.h>

static void zvm_print_log(void)
{
	printk("\n \
█████████╗    ██╗     ██╗    ███╗    ███╗  \n \
╚════███╔╝    ██║     ██║    ████╗  ████║  \n \
   ███╔╝      ╚██╗   ██╔╝    ██╔ ████╔██║  \n \
 ██╔╝          ╚██  ██╔╝     ██║ ╚██╔╝██║  \n \
█████████╗      ╚████╔╝      ██║  ╚═╝ ██║  \n \
╚════════╝        ╚═╝        ╚═╝      ╚═╝  \n"
	);
}

#ifdef CONFIG_ZVM_OPERATION_TEST
#define STACK_SIZE 20480
#define THREAD_PRIORITY 5

static K_THREAD_STACK_DEFINE(test_stack, STACK_SIZE);
static struct k_thread test_thread_data;
static void test_thread_entry(void *p1, void *p2, void *p3)
{
	run_all_generated_tests(false, 4, 2);
}
#endif

int main(int argc, char **argv)
{
	zvm_print_log();

#ifdef CONFIG_DISK_DRIVER_SDMMC
	/* Init SDMMC for SD Card*/
	vm_sdmmc_init();
#endif

#ifdef CONFIG_ZVM_OPERATION_TEST
	k_tid_t tid = k_thread_create(
		&test_thread_data, test_stack, STACK_SIZE,
		test_thread_entry, NULL, NULL, NULL,
		THREAD_PRIORITY, 0, K_FOREVER
	);

	/* 绑定到 CPU core 0 */
	k_thread_name_set(tid, "zvm auto test threads");
	k_thread_cpu_mask_clear(tid);          // 清除所有 CPU 绑定
	k_thread_cpu_mask_enable(tid, 0);      // 启用 CPU 0 绑定
	k_thread_start(tid);
#endif
	return 0;
}
