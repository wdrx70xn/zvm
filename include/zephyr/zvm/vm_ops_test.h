
/*
 * Copyright 2024-2025 HNU-ESNL: Guoqi Xie, Chenglai Xiong, Xingyu Hu and etc.
 * Copyright 2024-2025 openEuler SIG-Zephyr
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <zephyr/zvm/vm_manager.h>

#ifndef ZEPHYR_INCLUDE_VIRTUALIZATION_VM_OPS_TEST_H_
#define ZEPHYR_INCLUDE_VIRTUALIZATION_VM_OPS_TEST_H_

/**
* @brief 运行所有生成的测试用例
* @param test_flag 是否启用测试标志
* @param zephyr_vm_num Zephyr虚拟机数量
* @param linux_vm_num Linux虚拟机数量
*/
int run_all_generated_tests(bool test_flag, uint8_t zephyr_vm_num, uint8_t linux_vm_num);
 
#endif /* ZEPHYR_INCLUDE_VIRTUALIZATION_VM_OPS_TEST_H_ */
