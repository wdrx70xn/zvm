/*
 * Copyright 2024-2025 HNU-ESNL: Guoqi Xie, Chenglai Xiong, Xingyu Hu and etc.
 * Copyright 2024-2025 openEuler SIG-Zephyr
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <string.h>
#include <ksched.h>
#include <zephyr/arch/arch_interface.h>
#include <zephyr/zvm/os.h>
#include <zephyr/zvm/zvm.h>
#include <zephyr/zvm/vdev/vgic_v3.h>
#include <zephyr/zvm/vm_cpu.h>
#include <zephyr/zvm/vm.h>
#include <zephyr/zvm/vm_manager.h>
#include <zephyr/drivers/uart.h>
#if defined(CONFIG_HEARTBEAT) && defined(CONFIG_ZSHM)
#include <zephyr/zvm/vdev/zshm.h>
#endif

LOG_MODULE_DECLARE(ZVM_MODULE_NAME);

extern struct z_vnet_manager virtio_net_manager;

static void zvm_log_vm_info(struct z_vm *vm, int cmd_guest)
{
    const char *header = NULL;

    switch (cmd_guest) {
    case CMD_CREATE_GUEST:
        header = "Create VM Successful!";
        break;
    case CMD_RUN_GUEST:
        header = "Start VM Successful!";
        break;
    case CMD_PAUSE_GUEST:
        header = "Pause VM Successful!";
        break;
    case CMD_SHUTDOWN_GUEST:
        header = "Shutdown VM Successful!";
        break;
    case CMD_REBOOT_GUEST:
        header = "Reboot VM Successful!";
        break;
    case CMD_DELETE_GUEST:
        header = "Delete VM Successful!";
        break;
    case CMD_INFO_GUEST:
        header = "VM Info:";
        break;
    default:
        header = "VM Operation Successful!";
    }

    ZVM_LOG_INFO("|*********************************************|\n");
    ZVM_LOG_INFO("|******  %-29s  ******|\n", header);
    ZVM_LOG_INFO("|******          VM INFO                ******|\n");
    ZVM_LOG_INFO("|******  VM-NAME:     %-12s      ******|\n", vm->vm_name);
    ZVM_LOG_INFO("|******  VM-ID:       %-12d      ******|\n", vm->vmid);
    ZVM_LOG_INFO("|******  VCPU NUM:    %-12d      ******|\n", vm->vcpu_num);

    if (vm->os) {
        switch (vm->os->info.os_type) {
        case OS_TYPE_LINUX:
            ZVM_LOG_INFO("|******  VMEM SIZE:   %-12d(M)   ******|\n",
                          (uint32_t)vm->os->info.vm_mem_size / (1024 * 1024));
            break;
        case OS_TYPE_ZEPHYR:
            ZVM_LOG_INFO("|******  VMEM SIZE:   %-12d(M)   ******|\n",
                          (uint32_t)vm->os->info.vm_mem_size / (1024 * 1024));
            break;
        case OS_TYPE_FREERTOS:
            ZVM_LOG_INFO("|******  VMEM SIZE:   %-12d(M)   ******|\n",
                          (uint32_t)vm->os->info.vm_mem_size / (1024 * 1024));
            break;
        default:
            ZVM_LOG_INFO("|******  OTHER VM, NO MEMORY MSG ******|\n");
        }
    } else {
        ZVM_LOG_INFO("|******  OS INFO MISSING              ******|\n");
    }

    ZVM_LOG_INFO("|*********************************************|\n");
}

static int z_create_guest(struct z_vm *vm)
{
	int ret = 0, vm_tmpid = 0;
	struct z_vm *new_vm = vm;
	if (!new_vm) {
		ZVM_LOG_WARN("The vm struct is NULL!\n");
		return -EINVAL;
	}
	vm_tmpid = new_vm->vmid;

	ret = vm_create(vm_tmpid, new_vm);
	if (ret) {
		k_free(new_vm);
		ZVM_LOG_WARN("Can not create vm struct, VM struct init failed!\n");
		return ret;
	}
	ZVM_LOG_INFO("Create VM instance successful! \n");

	ret = vm_ops_init(new_vm);
	if (ret) {
		ZVM_LOG_WARN("VM ops init failed!\n");
		return ret;
	}
	ZVM_LOG_INFO("Init VM ops successful! \n");

	ret = vm_irq_block_init(new_vm);
	if (ret < 0) {
        ZVM_LOG_WARN("Init vm's irq block error!\n");
        return ret;
    }
	ZVM_LOG_INFO("Init VM irq block successful! \n");

	ret = vm_vcpus_init(new_vm);
	if (ret < 0) {
		ZVM_LOG_WARN("create vcpu error! \n");
		return -ENXIO;
	}
	ZVM_LOG_INFO("Init VM vcpus instances successful! \n");

	ret = vm_device_init(new_vm);
	if (ret) {
		ZVM_LOG_WARN("Init vm's virtual device error! \n");
		return ret;
	}
	ZVM_LOG_INFO("Init VM devices successful! \n");

	ret = vm_mem_init(new_vm);
	if(ret < 0){
		return ret;
	}
	ZVM_LOG_INFO("Init VM memory successful! \n");

	zvm_log_vm_info(new_vm, CMD_CREATE_GUEST);

	return 0;
}

static int z_run_guest(struct z_vm *vm)
{
	int ret = 0;
	/*
	* If the number of Linux instances that can be created could exceed
	* the number of available virtio devices, consider enabling this code.
	* Currently, memory constraints ensure the number of Linux instances
	* will not surpass the virtio device capacity of ZVM, so this check
	* is unnecessary.
	*/
	/*
	if (vm->os->info.os_type == OS_TYPE_LINUX &&
		vm->vcpus[DEFAULT_VCPU]->is_first_run) {
        if (is_virtio_blk_full() || is_virtio_net_full()) {
            ZVM_LOG_ERR("The number of virtio device has reached the limit.\n");
		    return -ENODEV;
	    }
    }
	*/
#ifdef CONFIG_ZVM_OPERATION_TEST
	int64_t last_beat = 0;
#endif
	if (vm->vm_status & (VM_STATE_NEVER_RUN | VM_STATE_PAUSE)) {
		if (vm->vm_status & VM_STATE_NEVER_RUN) {
			load_os_image(vm);
		}
#ifdef CONFIG_ZVM_OPERATION_TEST
		last_beat = get_vm_heartbeat(vm->vmid);
#endif
		ret = vm_vcpus_ready(vm);
	} else if (vm->vm_status & VM_STATE_RUNNING) {
		ZVM_LOG_INFO("The VM is already running, no need to start again! \n");
	} else {
		/* Hart or reset status, no respond. */
		ZVM_LOG_WARN("The VM has a invalid status (%d), abort! \n", vm->vm_status);
    	return -ENODEV;
	}
#ifdef CONFIG_ZVM_OPERATION_TEST
	/* 启动检测 */
	int64_t start = k_uptime_get();
	int64_t timeout = vm->os->is_rtos ? VM_RTOS_START_TIMEOUT : VM_LINUX_START_TIMEOUT;
	const char *dots[] = {"", ".", "..", "...", "....", ".....", "......"};
	int dot_index = 0;
	// VM_START_TIMEOUT毫秒内每WATCH_VM_START_INTERVAL毫秒
	// 检测一次heartbeat是否不为0
	while(get_vm_heartbeat(vm->vmid) == last_beat) {
		if (k_uptime_get() - start >= timeout) {
			ZVM_LOG_ERR("Timeout waiting for vm: %s start (waited %lld ms)\n",vm->vm_name,timeout);
			return -ENODEV;
		}
		ZVM_LOG_INFO("\rWaiting for vm start%s", dots[dot_index]);
    	dot_index = (dot_index + 1) % 7;
		k_sleep(K_MSEC(WATCH_VM_START_INTERVAL));
	}

	for (int i = 0; i<vm->vcpu_num; i++) {
		ZVM_LOG_DEBUG("vm->vcpus[%d] state is %x\n", i, vm->vcpus[i]->work->vcpu_thread->base.thread_state);
	}

	if (!vm->os->is_rtos) {
		/* 5s test for virtIO in linux guest. */
		k_sleep(K_MSEC(5000));
	}
#endif

	ZVM_LOG_INFO("\n");
	if(ret == 0) {
		zvm_log_vm_info(vm, CMD_RUN_GUEST);
	} else {
		ZVM_LOG_WARN("VM start failed: vm vcpus run failed! \n");
#if CONFIG_ZVM_OPERATION_TEST
		ret = -TEST_CMD_SKIP;
#endif
	}

	return ret;
}

static int z_pause_guest(struct z_vm *vm)
{
	int ret = 0;
    if (!(vm->vm_status == VM_STATE_RUNNING)) {
        ZVM_LOG_INFO("Not running, skip pause \n");
        return 0;
    }
	ret = vm_vcpus_pause(vm);
#ifdef CONFIG_ZVM_OPERATION_TEST
	/**暂停检测 */
	int64_t start = k_uptime_get();
	int64_t timeout = vm->os->is_rtos ? VM_RTOS_PAUSE_TIMEOUT : VM_LINUX_PAUSE_TIMEOUT;
	const char *dots[] = {"", ".", "..", "...", "....", ".....", "......"};
	int dot_index = 0;
	// VM_START_TIMEOUT毫秒内每WATCH_VM_START_INTERVAL毫秒
	// 检测一次和上次心跳的时间间隔是否长于HEARTBEAT_TIMEOUT_MS
	while (k_uptime_get() - get_vm_heartbeat(vm->vmid) < HEARTBEAT_TIMEOUT_MS) {
		if (k_uptime_get() - start >= timeout) {
			ZVM_LOG_ERR("Pause error\n");
			return -ENODEV;
		}
		ZVM_LOG_INFO("\rWaiting for vm pause%s   ", dots[dot_index]);
    	dot_index = (dot_index + 1) % 7;
		k_sleep(K_MSEC(WATCH_VM_PAUSE_INTERVAL));
	}
	for (int i = 0; i<vm->vcpu_num; i++) {
		ZVM_LOG_DEBUG("vm->vcpus[%d] state is %x\n", i, vm->vcpus[i]->work->vcpu_thread->base.thread_state);
	}
#endif
	ZVM_LOG_INFO("\n");
	if(ret == 0) {
		zvm_log_vm_info(vm, CMD_PAUSE_GUEST);
	} else {
		ZVM_LOG_WARN("VM pause failed: vm vcpus pause failed! \n");
#if CONFIG_ZVM_OPERATION_TEST
		ret = -TEST_CMD_SKIP;
#endif
	}
	return ret;
}


/*TODO: add shell*/
static int z_shutdown_guest(struct z_vm *vm)
{
	ARG_UNUSED(vm);
	ZVM_LOG_INFO("VM shutdown is not supported now! \n");
	return 0;
}

static int z_reboot_guest(struct z_vm *vm)
{
	ARG_UNUSED(vm);
	ZVM_LOG_INFO("VM reboot is not supported now! \n");
	return 0;

	// int ret;
	// ZVM_LOG_INFO("vm reboot.... \n");
	// ret = vm_vcpus_pause(vm);
	// if(ret < 0) {
	// 	ZVM_LOG_ERR("VM reboot failed: vm vcpus pause failed! \n");
	// 	return ret;
	// }
	// /*
	//  * TODO: smp
	//  */
  	// ret = vm_vcpus_reset(vm);
	// if(ret < 0){
	// 	ZVM_LOG_ERR("VM reboot failed: vm vcpus reset failed! \n");
	// 	return ret;
	// }
	// vm->reboot = true;
  	// ret = vm_vcpus_ready(vm);
	// if(ret < 0){
	// 	ZVM_LOG_ERR("VM reboot failed: vm vcpus ready failed! \n");
	// 	return ret;
	// }
	// zvm_log_vm_info(vm, CMD_REBOOT_GUEST);
	// return ret;
}

static int z_delete_guest(struct z_vm *vm)
{
	int retry = 0, i;
	int max_times = 200;
	switch (vm->vm_status) {
	case VM_STATE_RUNNING:
		ZVM_LOG_INFO("This vm is running! Try to stop and delete it!\n");
		vm->vm_status = VM_STATE_HALT;
		for (i = 0; i < vm->vcpu_num; i++) {
			vm->vcpus[i]->deleting = true;
			atomic_inc(&vm->vcpus[i]->hcpuipi_count);
			vcpu_ipi_scheduler(BIT(vm->vcpus[i]->cpu), 0);
		}

		while (atomic_get(&vm->exist_vcpu) > 0) {
			retry++;
			k_msleep(50);
			if (retry > max_times) {
				ZVM_LOG_WARN("Retry wait vcpu threads exit failed\n");
				break;
			}
		}
		vm_vcpus_halt(vm);
		vm_delete(vm);
		break;
	case VM_STATE_PAUSE:
		ZVM_LOG_INFO("This vm is paused! Just delete it!\n");
		vm_delete(vm);
		break;
	case VM_STATE_NEVER_RUN:
		ZVM_LOG_INFO("This vm is created but not run! Just delete it!\n");
		vm_delete(vm);
		break;
	default:
		ZVM_LOG_WARN("This vm status is invalid!\n");
		return -ENODEV;
	}

	zvm_log_vm_info(vm, CMD_DELETE_GUEST);
	return 0;
}

static int z_info_guest(struct z_vm *vm)
{
	ARG_UNUSED(vm);
	int ret = 0;

	if (zvm_overall_info->vm_total_num > 0) {
		list_all_vms_info();
	}else{
		ret = 0;
		ZVM_LOG_INFO("No vm exits\n");
	}

	return ret;
}

struct guest_ops zvm_guest_ops = {
		.create = z_create_guest,
		.run = z_run_guest,
		.pause = z_pause_guest,
		.shutdown = z_shutdown_guest,
		.reboot = z_reboot_guest,
		.delete = z_delete_guest,
		.info = z_info_guest,
};

int zvm_vm_ops_entry(struct z_vm *vm, int cmd_guest)
{
	int ret = 0;

	switch (cmd_guest) {
	case CMD_CREATE_GUEST:
		ret = zvm_guest_ops.create(vm);
		break;
	case CMD_RUN_GUEST:
		ret = zvm_guest_ops.run(vm);
		break;
	case CMD_PAUSE_GUEST:
		ret = zvm_guest_ops.pause(vm);
		break;
	case CMD_SHUTDOWN_GUEST:
		ret = zvm_guest_ops.shutdown(vm);
		break;
	case CMD_REBOOT_GUEST:
		ret = zvm_guest_ops.reboot(vm);
		break;
	case CMD_DELETE_GUEST:
		ret = zvm_guest_ops.delete(vm);
		break;
	case CMD_INFO_GUEST:
		ret = zvm_guest_ops.info(vm);
		break;
	default:
		ZVM_LOG_WARN("Input error! \n");
		ZVM_LOG_WARN("Please input \" zvm new -t + os_name \" command to new a vm! \n");
		return -EINVAL;
	}

	return ret;
}

static uint16_t get_vmid_by_id(size_t argc, char **argv)
{
    uint16_t vm_id =  CONFIG_MAX_VM_NUM;
    int opt;
    char *optstring = "t:n:";
    struct getopt_state *state;

    /* Initialize the global state */
    getopt_init();
    /* Get Current getopt_state */
    state = getopt_state_get();

    while ((opt = getopt(argc, argv, optstring)) != -1) {
		switch (opt) {
		case 'n':
			char *endptr = NULL;
			int val = strtol(state->optarg, &endptr, 10);
			if (*endptr != '\0' || val < 0 || val > CONFIG_MAX_VM_NUM -1 ) {
					ZVM_LOG_WARN("Invalid VM ID: %s\n", state->optarg);
					return -EINVAL;
			}
			vm_id = (uint16_t)val;
			break;
		default:
			ZVM_LOG_WARN("Input number invalid, Please input a valid vmid after \"-n\" command! \n");
			return -EINVAL;
		}
	}
    return vm_id;
}

static int handle_vm_create_cmd(size_t argc, char **argv)
{
	int ret = 0;
    int opt;
    char *optstring = "t:c:m:";
	bool special_cpu = false, special_mem = false;
    struct getopt_state *state;
	struct z_os_info guest_os = {
		.os_type=UINT16_MAX,
		.vcpu_num=0,
		.vm_mem_base=0,
		.vm_mem_size=0,
		.vm_load_base=0,
		.vm_image_base=0,
		.vm_image_size=0,
		.entry_point=0,
	};

    /* Initialize the global state */
	getopt_init();
    /* Get Current getopt_state */
	state = getopt_state_get();
	char *endptr = NULL;
	long val = -1;

	while ((opt = getopt(argc, argv, optstring)) != -1) {
		switch (opt) {
		case 't':
			ZVM_LOG_INFO("[t] state->optarg is %s\n",state->optarg);
			val = get_os_type_from_args(state->optarg);
			if(val < 0) {
				ZVM_LOG_WARN("Get os type error! \n");
				return -EINVAL;
			}
			guest_os.os_type = val;
    		continue;
		case 'c':
			ZVM_LOG_INFO("[c] state->optarg is %s\n",state->optarg);
			endptr = NULL;
			val = strtol(state->optarg, &endptr, 10);
			if (*endptr != '\0' || val <= 0 || val > CONFIG_MAX_VCPU_PER_VM
				|| !vcpu_margin_check(guest_os.os_type, val)
			) {
				ZVM_LOG_WARN("Invalid vcpu num: %s\n", state->optarg);
				return -EINVAL;
			}
			guest_os.vcpu_num = (uint16_t)val;
			special_cpu = true;
			continue;
		case 'm':
			ZVM_LOG_INFO("[m] state->optarg is %s\n",state->optarg);
			endptr = NULL;
			val = strtol(state->optarg, &endptr, 10);
			if (*endptr != '\0' || val <= 0 || val > 1024 ) {
				ZVM_LOG_WARN(
					"Invalid memory size: %sMB, maximux: %dMB\n",
					state->optarg, DT_REG_SIZE(DT_NODELABEL(sram0)) / 1048576
				);
				return -EINVAL;
			}
			guest_os.vm_mem_size = (uint32_t)(val * 1024 * 1024);
			special_mem = true;
			continue;
		default:
			ZVM_LOG_WARN("Input error! \n");
			ZVM_LOG_WARN(
				"Please input \" zvm new -t + os_name \" command to new a vm! \n"
			);
			return -EINVAL;
		}
	}
	if(guest_os.os_type == UINT16_MAX){
		ZVM_LOG_WARN("no os type specified\n");
		return -EINVAL;
	}

	ret = get_os_template_type(guest_os, special_cpu, special_mem);
	if (ret < 0) {
		ZVM_LOG_WARN("Get template error! \n");
		return ret;
	}

		return ret;
}

int z_parse_vm_cmd(int cmd_guest, size_t argc, char **argv)
{
    int ret = 0;

	switch (cmd_guest) {
	case CMD_CREATE_GUEST:
		ret = handle_vm_create_cmd(argc,argv);
		break;
	case CMD_RUN_GUEST:
	case CMD_PAUSE_GUEST:
	case CMD_SHUTDOWN_GUEST:
	case CMD_REBOOT_GUEST:
	case CMD_DELETE_GUEST:
		ret = get_vmid_by_id(argc, argv);
		if (ret < 0) {
			ZVM_LOG_WARN("Can not get vm id! \n");
			return ret;
		}
		break;
	case CMD_INFO_GUEST:
		return 0;
	break;
	default:
		ZVM_LOG_WARN("Input error! \n");
		ZVM_LOG_WARN("Please input \" zvm new -t + os_name \" command to new a vm! \n");
		return -EINVAL;
	}
  return ret;
}

struct z_vm *zvm_get_vm_by_cmd(int cmd_guest, size_t argc, char **argv)
{
	struct z_vm *vm = NULL;
	int ret;
	uint16_t vm_id = 0;

	ret = z_parse_vm_cmd(cmd_guest, argc, argv);
	if (ret < 0) {
		ZVM_LOG_WARN("Can not get vm id! \n");
		return NULL;
	}
	vm_id = ret;

	switch (cmd_guest) {
	case CMD_CREATE_GUEST:
		vm = (struct z_vm *)k_malloc(sizeof(struct z_vm));
		memset(vm, 0, sizeof(struct z_vm));
		if (!vm) {
			ZVM_LOG_WARN("Allocation memory for VM struct Error!\n");
			return NULL;
		}
		vm->vmid = vm_id;
		break;
	case CMD_RUN_GUEST:
	case CMD_PAUSE_GUEST:
	case CMD_SHUTDOWN_GUEST:
	case CMD_REBOOT_GUEST:
	case CMD_DELETE_GUEST:
	case CMD_INFO_GUEST:
		if (!(BIT(vm_id) & zvm_overall_info->alloced_vmid)) {
			ZVM_LOG_INFO("This vmid is not exist! Please input zvm info to show info!\n");
			return NULL;
		}
		vm = zvm_overall_info->vms[vm_id];
		if (!vm) {
			ZVM_LOG_WARN("This vm is not exist! Please input zvm info to list vms!\n");
			return NULL;
		}
		break;
	default:
		ZVM_LOG_WARN(
			"Please input \" zvm create -t + os_name \" command to new a vm! \n"
		);
		return NULL;
	}

	return vm;
}


int zvm_vm_operation_template(int cmd_guest, size_t argc, char **argv)
{
	int ret = 0;
	struct z_vm *vm = NULL;

	vm = zvm_get_vm_by_cmd(cmd_guest, argc, argv);
	if (!vm && cmd_guest != CMD_INFO_GUEST) {
#ifdef CONFIG_ZVM_OPERATION_TEST
		ZVM_LOG_INFO("Can not get vm struct! \n");
		return -TEST_CMD_SKIP;
#else
		ZVM_LOG_WARN("Can not get vm struct!\n");
		return -EINVAL;
#endif
	}

	ret = zvm_vm_ops_entry(vm, cmd_guest);
	if (ret < 0) {
		ZVM_LOG_WARN("Guest operation failed! \n");
		return ret;
	}

	return ret;
}
