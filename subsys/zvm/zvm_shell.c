/*
 * Copyright 2024-2025 HNU-ESNL: Guoqi Xie, Chenglai Xiong, Xingyu Hu and etc.
 * Copyright 2024-2025 openEuler SIG-Zephyr
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/init.h>
#include <stdlib.h>
#include <zephyr/arch/cpu.h>
#include <zephyr/arch/arm64/lib_helpers.h>
#include <zephyr/sys/printk.h>
#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>
#include <zephyr/zvm/zvm.h>
#include <zephyr/zvm/os.h>
#include <zephyr/zvm/vm.h>
#include <zephyr/zvm/vm_device.h>
#include <zephyr/zvm/vm_manager.h>
#include <zephyr/zvm/vdev/vserial.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/storage/disk_access.h>
#include <zephyr/logging/log.h>
#include <stdio.h>
#ifdef CONFIG_ZVM_CLIENT
#include <zephyr/zvm/client.h>
#endif
LOG_MODULE_DECLARE(ZVM_MODULE_NAME);
#ifdef CONFIG_DISK_DRIVER_SDMMC
#define DISK_DRIVE_NAME "SDMMC"
#endif
#define SHELL_HELP_ZVM "ZVM manager command. " \
    "Some subcommand you can choice as below: \n"
#define SHELL_HELP_CREATE_NEW_VM "Create a vm. \n" \
    "You can use 'zvm create -t zephyr' or 'linux' to create a new vm. \n"
#define SHELL_HELP_NEW_VM "Deprecated, Same as command create, keep it for backward compatibility. \n" \
    "You can use 'zvm new -t zephyr' or 'linux' to create a new vm. \n"
#define SHELL_HELP_RUN_VM "Run a created vm. \n" \
    "You can use 'zvm run -n 0' to run vm with vmid equal to 0. \n"
#define SHELL_HELP_LIST_VM "List all vm info. \n" \
    "You can use 'zvm info' to list all vm info. \n" \
    "You can use 'zvm info -n 0' to list vm info with vmid equal to 0. \n"
#define SHELL_HELP_PAUSE_VM "Pause a vm. \n" \
    "You can use 'zvm pause -n 0' to pause vm with vmid equal to 0. \n"
#define SHELL_HELP_DELETE_VM "Delete a vm. \n" \
    "You can use 'zvm delete -n 0' to delete vm with vmid equal to 0. \n"
#define SHELL_HELP_UPDATE_VM "Update vm. \n" \
    "vm update is not supported now. \n"
#define SHELL_HELP_MANAGER_VM "Manager vm. \n" \
    "You can open the ZVM management software on the PC. \n"
#define SHELL_HELP_MANAGER_TCP_VM "Manager vm. \n" \
    "You can restart the sockets between ZVM management software on the PC and ZVM. \n"
#define SHELL_HELP_CONNECT_VIRTUAL_SERIAL "Switch virtual serial. \n" \
    "You can use 'zvm look 0' to connect available virtual serial. \n"

K_MUTEX_DEFINE(shell_vmops_mutex);
static int cmd_zvm_create(const struct shell *zvm_shell, size_t argc, char **argv)
{
    int ret = 0;

	k_mutex_lock(&shell_vmops_mutex, K_FOREVER);
	shell_fprintf(zvm_shell, SHELL_NORMAL, "Ready to create a new vm... \n");

    ret = zvm_vm_operation_template(CMD_CREATE_GUEST, argc, argv);
    if (ret) {
        shell_fprintf(zvm_shell, SHELL_NORMAL,
            "Create vm failured, please follow the message and try again! \n");
        k_mutex_unlock(&shell_vmops_mutex);
        return ret;
    }
    k_mutex_unlock(&shell_vmops_mutex);

    return ret;
}


static int cmd_zvm_run(const struct shell *zvm_shell, size_t argc, char **argv)
{
    /* Run vm code. */
    int ret = 0;

	k_mutex_lock(&shell_vmops_mutex, K_FOREVER);
    shell_fprintf(zvm_shell, SHELL_NORMAL, "Ready to run VM... \n");

    ret = zvm_vm_operation_template(CMD_RUN_GUEST, argc, argv);
    if (ret) {
        shell_fprintf(zvm_shell, SHELL_NORMAL,
            "Run vm failured, please follow the message and try again! \n");
        k_mutex_unlock(&shell_vmops_mutex);
        return ret;
    }
    k_mutex_unlock(&shell_vmops_mutex);

    return ret;
}

static int cmd_zvm_pause(const struct shell *zvm_shell, size_t argc, char **argv)
{
    int ret = 0;


	k_mutex_lock(&shell_vmops_mutex, K_FOREVER);
    shell_fprintf(zvm_shell, SHELL_NORMAL, "Ready to pause VM... \n");

    ret = zvm_vm_operation_template(CMD_PAUSE_GUEST, argc, argv);
    if (ret) {
        shell_fprintf(zvm_shell, SHELL_NORMAL,
            "Pause vm failured, please follow the message and try again! \n");
        k_mutex_unlock(&shell_vmops_mutex);
        return ret;
    }

    k_mutex_unlock(&shell_vmops_mutex);

    return ret;
}


static int cmd_zvm_delete(const struct shell *zvm_shell, size_t argc, char **argv)
{
    int ret = 0;

	k_mutex_lock(&shell_vmops_mutex, K_FOREVER);
    shell_fprintf(zvm_shell, SHELL_NORMAL, "Ready to delete VM... \n");
    /* Delete vm code. */
    ret = zvm_vm_operation_template(CMD_DELETE_GUEST, argc, argv);
    if (ret) {
        shell_fprintf(zvm_shell, SHELL_NORMAL,
            "Delete vm failured, please follow the message and try again! \n");
        k_mutex_unlock(&shell_vmops_mutex);
        return ret;
    }
    k_mutex_unlock(&shell_vmops_mutex);

    return ret;
}


static int cmd_zvm_info(const struct shell *zvm_shell, size_t argc, char **argv)
{
    int ret = 0;


    k_mutex_lock(&shell_vmops_mutex, K_FOREVER);

    shell_fprintf(zvm_shell, SHELL_NORMAL, "Ready to list VM... \n");
    ret = zvm_vm_ops_entry(NULL, CMD_INFO_GUEST);
    if (ret) {
        shell_fprintf(zvm_shell, SHELL_NORMAL,
            "List vm failured. \n There may no vm in the system! \n");
        k_mutex_unlock(&shell_vmops_mutex);
        return ret;
    }
    k_mutex_unlock(&shell_vmops_mutex);

    return 0;
}


static int cmd_zvm_update(const struct shell *zvm_shell, size_t argc, char **argv)
{
    /* Update vm code. */
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    shell_fprintf(zvm_shell, SHELL_NORMAL,
            "Update vm is not support now, Please try other command. \n");
    return 0;
}


#ifdef CONFIG_ZVM_CLIENT
#define MANAGER_THREAD_STACKSIZE 40960
static int cmd_zvm_manager(const struct shell *zvm_shell, size_t argc, char **argv)
{
    /* Unused. */
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    if (g_client_status.is_zvm_manager_probe) {
        ZVM_LOG_WARN("ZVM Manager is already running !\n");
        return 0;
    }

    k_mutex_lock(&shell_vmops_mutex, K_FOREVER);

    // 动态分配线程对象+栈
    struct k_thread *thread = k_malloc(sizeof(struct k_thread));
    if (!thread){
        printk("Failed to allocate thread structure\n");
        k_mutex_unlock(&shell_vmops_mutex);
        return -ENOMEM;
    }

    k_thread_stack_t *stack = k_aligned_alloc(16, MANAGER_THREAD_STACKSIZE);
    if (!stack){
        printk("manager thread create failed.\n");
        k_free(thread);
        k_mutex_unlock(&shell_vmops_mutex);
        return -ENOMEM;
    }

    // 创建线程执行 ZVM_manager_init()
	k_tid_t tid = k_thread_create(
        thread, stack, MANAGER_THREAD_STACKSIZE, 
		(k_thread_entry_t) ZVM_manager_init, NULL, NULL, NULL, 
		11, 0, K_MSEC(100)
    );
    if (!tid) {
        printk("Failed to create thread\n");
        k_free(thread);
        k_free(stack);
        k_mutex_unlock(&shell_vmops_mutex);
        return -ENOMEM;
    }

    k_thread_name_set(tid, "zvm_manager");

	// 绑定线程在cpu-0上执行
	k_thread_cpu_mask_clear(tid);
	k_thread_cpu_mask_enable(tid, 0);

    // k_thread_start(tid);

    k_mutex_unlock(&shell_vmops_mutex);
    g_client_status.is_zvm_manager_probe = true;
    return 0;
}

static void cmd_zvm_manager_tcp_rebuild(const struct shell *zvm_shell, size_t argc, char **argv)
{
    if (!g_client_status.is_zvm_manager_probe) {
        ZVM_LOG_WARN("Please Run zvm manager to start manager service first!\n");
    }
    g_client_status.is_reload_tcp = true;
    ZVM_LOG_INFO("Ready to Reset Sockets!\n");
}
#endif

#ifdef CONFIG_DISK_DRIVER_SDMMC

static void cmd_zvm_sd(const struct shell *zvm_shell, size_t argc, char **argv)
{
    static const char *disk_pdrv = DISK_DRIVE_NAME;
    uint32_t block_count, block_size;
    int ret;

    // 测试参数配置
    const uint32_t start_block = 4096;  // 起始块号
    const uint32_t test_blocks = 1024;  // 测试块数量
    const uint32_t write_chunk_size = 1024; // 每次写入的块数
    const uint32_t read_chunk_size = 1024; // 每次读取的块数
    // 初始化SD卡
    if (disk_access_ioctl(disk_pdrv, DISK_IOCTL_CTRL_INIT, NULL) != 0) {
        printk("[SD] Init failed\n");
        return;
    }

    // 获取SD卡参数
    if (disk_access_ioctl(disk_pdrv, DISK_IOCTL_GET_SECTOR_COUNT, &block_count) ||
        disk_access_ioctl(disk_pdrv, DISK_IOCTL_GET_SECTOR_SIZE, &block_size)) {
        printk("[SD] Get info failed\n");
        return;
    }

    printk("[SD] Test %u blocks from block %u (size=%uB)\n", 
          test_blocks, start_block, block_size);

    // 检查测试范围
    if (start_block + test_blocks > block_count) {
        printk("[SD] Exceeds card capacity\n");
        return;
    }


    uint8_t *write_buf = k_malloc(block_size * write_chunk_size);
    uint8_t *read_buf = k_malloc(block_size * read_chunk_size);
    if (!write_buf || !read_buf) {
        printk("[SD] Buffer alloc failed\n");
    }

    uint32_t error_count = 0;

    // 多块写入操作
    for (uint32_t chunk_start = 0; chunk_start < test_blocks; chunk_start += write_chunk_size) {
        uint32_t blocks_in_chunk = MIN(write_chunk_size, test_blocks - chunk_start);

        // 准备测试数据
        for (uint32_t i = 0; i < blocks_in_chunk; i++) {
            uint32_t block_id = chunk_start + i;
            uint8_t *block_buf = write_buf + (i * block_size);

            memset(block_buf, block_id & 0xFF, block_size);
            block_buf[0] = 0xEE;  // Header marker
            block_buf[1] = 0xAD;
            block_buf[2] = (block_id >> 8) & 0xFF;
            block_buf[3] = block_id & 0xFF;
        }

        // 多块写入
        uint32_t start_block_id = start_block + chunk_start;
        if ((ret = disk_access_write(disk_pdrv, write_buf, start_block_id, blocks_in_chunk))) {
            printk("[SD] Multi-block write failed at block %u (%u blocks): %d\n",
                  start_block_id, blocks_in_chunk, ret);
            error_count += blocks_in_chunk;
            continue;
        }
    }

    for (uint32_t i = 0; i < test_blocks; i += read_chunk_size) {
        uint32_t blocks_to_read = (test_blocks - i) >= read_chunk_size ?
                                read_chunk_size : (test_blocks - i);
        uint32_t start_sec = i + start_block;

        // 多块读取
        if ((ret = disk_access_read(disk_pdrv, read_buf, start_sec, blocks_to_read))) {
            printk("[SD] Read failed at block %u (count %u): %d\n", 
                start_block, blocks_to_read, ret);
            error_count += blocks_to_read;
            continue;
        }

        // 逐个验证每个块的数据
        for (uint32_t j = 0; j < blocks_to_read; j++) {
            uint32_t block_id = j+i;
            uint8_t *block_data = read_buf + (j * block_size);  // 假设SD_BLOCK_SIZE是块大小(通常是512)

            // 打印和验证数据
            printk("  [BLK %3u] W: 0xEE 0xAD %02X %02X | R: %02X %02X %02X %02X",
                block_id,
                (block_id >> 8) & 0xFF, block_id & 0xFF,
                block_data[0], block_data[1], block_data[2], block_data[3]);

            // 验证数据
            bool header_ok = (block_data[0] == 0xEE && block_data[1] == 0xAD);
            bool id_ok = (block_data[3] == (block_id & 0xFF));

            if (!(header_ok && id_ok)) {
                error_count++;
                printk("  [FAIL]\n");
            } else {
                printk("  [OK]\n");
            }
        }
    }
    // 打印统计结果
    printk("\n[SD] Test completed. Errors: %u/%u (%.1f%%)\n",
          error_count, test_blocks,
          (error_count * 100.0) / test_blocks);

    k_free(write_buf);
    k_free(read_buf);
}
#endif
/* Add subcommand for Root0 command zvm. */
SHELL_STATIC_SUBCMD_SET_CREATE(m_sub_zvm,
    SHELL_CMD(create, NULL, SHELL_HELP_CREATE_NEW_VM, cmd_zvm_create),
    SHELL_CMD(new, NULL, SHELL_HELP_NEW_VM, cmd_zvm_create),
    SHELL_CMD(run, NULL, SHELL_HELP_RUN_VM, cmd_zvm_run),
    SHELL_CMD(pause, NULL, SHELL_HELP_PAUSE_VM, cmd_zvm_pause),
    SHELL_CMD(delete, NULL, SHELL_HELP_DELETE_VM, cmd_zvm_delete),
    SHELL_CMD(info, NULL, SHELL_HELP_LIST_VM, cmd_zvm_info) ,
    SHELL_CMD(update, NULL, SHELL_HELP_UPDATE_VM, cmd_zvm_update),
#ifdef CONFIG_ZVM_CLIENT
    SHELL_CMD(manager, NULL, SHELL_HELP_MANAGER_VM, cmd_zvm_manager),
    SHELL_CMD(tcpreload, NULL, SHELL_HELP_MANAGER_TCP_VM, cmd_zvm_manager_tcp_rebuild),
#endif
#ifdef CONFIG_DISK_DRIVER_SDMMC
    SHELL_CMD(sd, NULL, SHELL_HELP_UPDATE_VM, cmd_zvm_sd),
#endif
#ifdef CONFIG_VM_VSERIAL
    SHELL_CMD(look, NULL, SHELL_HELP_CONNECT_VIRTUAL_SERIAL, switch_virtual_serial_handler),
#endif
    SHELL_SUBCMD_SET_END
);

/* Add command for hypervisor. */
SHELL_CMD_REGISTER(zvm, &m_sub_zvm, SHELL_HELP_ZVM, NULL);