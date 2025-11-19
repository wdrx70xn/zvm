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
#include <zephyr/cache.h>
#include <zephyr/zvm/vdev/vgic_common.h>
#include <zephyr/zvm/vdev/vgic_v3.h>


LOG_MODULE_REGISTER(ZVM_MODULE_NAME);

struct zvm_manage_info *zvm_overall_info;       /*@TODO,This may need to replace by macro later*/
static struct zvm_dev_lists  zvm_overall_dev_lists;


/**
 * @brief zvm_hwsys_info_init aim to init zvm_info for the hypervisor
 * Two stage for this function:
 * 1. Init zvm_overall for some of para in struct
 * 2. get hareware information form dts
 * TODO: Add hardware here.
 */
static int zvm_hwsys_info_init(struct zvm_hwsys_info *z_info)
{
    int cpu_ret = -1, mem_ret = -1;
    ARG_UNUSED(cpu_ret);
    ARG_UNUSED(mem_ret);

    z_info->phy_mem_used = 0;

    return 0;
}

void zvm_ipi_handler(void)
{
    int i;
    struct z_vcpu *vcpu = _current_vcpu;

    /* vCPU thread ? */
    if (vcpu) {
        if (atomic_get(&vcpu->hcpuipi_count) > 0) {
            virt_sync_and_flush_vcpu_irq(vcpu);
            atomic_dec(&vcpu->hcpuipi_count);
        } else if (atomic_get(&vcpu->vcpuipi_count) > 0) {
            /* judge whether the ipi is send to vcpu. */
            vm_ipi_handler(vcpu->vm);
            atomic_dec(&vcpu->vcpuipi_count);
        }
        /* 删除VM的信号 */
        if (vcpu->deleting) {
            /*失能所有外设中断*/
            for (i = 32; i < VM_GLOBAL_VIRQ_NR; i++) {
                vgic_unset_enable_lock(vcpu, i, BIT(vcpu->vcpu_id));
            }
            z_barrier_dmem_fence_full();
        }
    }

#if defined(CONFIG_SOC_RK3568) || defined(CONFIG_SOC_RK3588) || \
    defined(CONFIG_SOC_E2000Q) || defined(CONFIG_SOC_D3000) || defined(CONFIG_SOC_S5000C)
        const uint64_t ipi_mpidr = GET_MPIDR();
        if(get_pcpu_cache_clean(ipi_mpidr)){
            sys_cache_data_invd_all();
            reset_pcpu_cache_clean(ipi_mpidr);
            z_barrier_isync_fence_full();
        }
#endif

}

int load_os_image(struct z_vm *vm)
{
    int ret = 0;

    switch (vm->os->info.os_type){
    case OS_TYPE_LINUX:
    case OS_TYPE_ZEPHYR:
    case OS_TYPE_FREERTOS:
        load_vm_image(vm->vmem_domain, vm->os);
        break;
    default:
        ZVM_LOG_WARN("Unsupport OS image!");
        ret = -EINVAL;
        break;
	}
    return ret;
}

/**
 * @brief This function aim to init zvm dev on zvm init stage
 * TODO: may add later
 * @return int
 */
static int zvm_dev_ops_init()
{
    return 0;
}

/**
 * @brief Init zvm overall device here
 * Two stage For this function:
 * 1. Create and init zvm_over_all struct
 * 2. Pass information from
 * @return int : the error code there
 */
static int zvm_overall_init(void)
{
    int ret = 0;

    /* First initialize zvm_overall_info->hw_info. */
    zvm_overall_info = (struct zvm_manage_info*)k_malloc  \
                            (sizeof(struct zvm_manage_info));
    if (!zvm_overall_info) {
        return -ENOMEM;
    }

    zvm_overall_info->hw_info = (struct zvm_hwsys_info*)
            k_malloc(sizeof(struct zvm_hwsys_info));
    if (!zvm_overall_info->hw_info) {
        ZVM_LOG_ERR("Allocate memory for zvm_overall_info Error.\n");
        /*
         * Too cumbersome resource release way.
         * We can use resource stack way to manage these resouce.
         */
        k_free(zvm_overall_info);
        return -ENOMEM;
    }

    ret = zvm_hwsys_info_init(zvm_overall_info->hw_info);
    if (ret) {
        k_free(zvm_overall_info->hw_info);
        k_free(zvm_overall_info);
        return ret;
    }

    memset(zvm_overall_info->vms, 0, sizeof(zvm_overall_info->vms));
    memset(zvm_overall_info->vcpu_use, 0, sizeof(int) * CONFIG_MP_MAX_NUM_CPUS);
    zvm_overall_info->alloced_vmid = 0;
    zvm_overall_info->vm_total_num = 0;
    ZVM_SPINLOCK_INIT(&zvm_overall_info->spin_zmi);
    /* The guest_memory space end in zshm start addr */
#if DT_NODE_HAS_STATUS(DT_NODELABEL(zshm), okay)
    guest_memory_manager_init(RTOS_VM_LOAD_BASE,
        DT_REG_ADDR(DT_NODELABEL(zshm)) - RTOS_VM_LOAD_BASE
    );
#else
    guest_memory_manager_init(RTOS_VM_LOAD_BASE,
        CONFIG_DEFAULT_TOTAL_GUEST_MEMSIZE
    );
#endif

#ifdef CONFIG_VM_VIRTIO_NET
    mac_pool_init();
#endif
    return ret;
}

/**
 * @brief Add all the device to the zvm_overall_list, expect passthrough device.
*/
static int zvm_init_idle_device_1(const struct device *dev, struct z_virt_dev *vdev,
                            struct zvm_dev_lists *dev_list)
{
    uint16_t name_len;
    struct z_virt_dev *vm_dev = vdev;

    /*@TODO：Determine whether to connect directly based on device type*/
    vm_dev->dev_pt_flag = true;

    if(strcmp(((struct virt_device_config *)dev->config)->device_type, "virtio") == 0) {
        vm_dev->shareable = true;
    } else {
        vm_dev->shareable = false;
    }

    name_len = strlen(dev->name);
    name_len = name_len > VIRT_DEV_NAME_LENGTH ? VIRT_DEV_NAME_LENGTH : name_len;
    strncpy(vm_dev->name, dev->name, name_len);
    vm_dev->name[name_len] = '\0';

    vm_dev->vm_vdev_paddr = ((struct virt_device_config *)dev->config)->reg_base;
    vm_dev->vm_vdev_size = ((struct virt_device_config *)dev->config)->reg_size;
    vm_dev->hirq = ((struct virt_device_config *)dev->config)->hirq_num;

    if (!strncmp(VM_DEFAULT_CONSOLE_NAME, vm_dev->name, VM_DEFAULT_CONSOLE_NAME_LEN)) {
        vm_dev->vm_vdev_vaddr = VM_DEBUG_CONSOLE_BASE;
        vm_dev->virq = VM_DEBUG_CONSOLE_IRQ;
    }
#if defined(CONFIG_VM_VIRTIO_MMIO) && defined(CONFIG_VM_VIRTIO_BLOCK)
    else if (!strncmp(VM_VIRTIO_BLK_NAME, vm_dev->name, VM_DEFAULT_VIRTIO_NAME_LEN)) {
        vm_dev->vm_vdev_vaddr = VM_DEFAULT_VIRTIO_BLK_BASE;
        vm_dev->virq = VM_DEFAULT_VIRTIO_BLK_IRQ;
    }
#endif
#if defined(CONFIG_VM_VIRTIO_MMIO) && defined(CONFIG_VM_VIRTIO_NET)
    else if (!strncmp(VM_VIRTIO_NET_NAME, vm_dev->name, VM_DEFAULT_VIRTIO_NAME_LEN)) {
        vm_dev->vm_vdev_vaddr = VM_DEFAULT_VIRTIO_NET_BASE;
        vm_dev->virq = VM_DEFAULT_VIRTIO_NET_IRQ;
    }
#endif
    else {
        vm_dev->vm_vdev_vaddr = vm_dev->vm_vdev_paddr;
        vm_dev->virq = vm_dev->hirq;
    }

    vm_dev->vm = NULL;
    vm_dev->priv_data = (void *)dev;

    // ZVM_LOG_INFO("Init idle device %s successful! \n", vm_dev->name);
    // ZVM_LOG_INFO("The device's paddress is 0x%x, paddress is 0x%x, size is 0x%x, hirq is %d, virq is %d. \n",
    //         vm_dev->vm_vdev_paddr, vm_dev->vm_vdev_vaddr, vm_dev->vm_vdev_size, vm_dev->hirq, vm_dev->virq);

    sys_dnode_init(&vm_dev->vdev_node);
    sys_dlist_append(&dev_list->dev_idle_list, &vm_dev->vdev_node);

    return 0;
}

/**
 * @brief Scan the device list and get the device by name.
 */
static int zvm_devices_list_init(void)
{
    struct z_virt_dev *vm_dev;

    sys_dlist_init(&zvm_overall_dev_lists.dev_idle_list);
    sys_dlist_init(&zvm_overall_dev_lists.dev_used_list);

    /* scan the host dts and get the device list */
    STRUCT_SECTION_FOREACH(device, dev) {
        /**
         * through the `init_res` to judge whether the pass-through device
         * is ready to allocate to vm.
         */
		if (dev->state->init_res == VM_DEVICE_INIT_RES) {
            vm_dev = (struct z_virt_dev*)k_malloc(sizeof(struct z_virt_dev));
            memset(vm_dev, 0, sizeof(struct z_virt_dev));
            if (vm_dev == NULL) {
                return -ENOMEM;
            }
            zvm_init_idle_device_1(dev, vm_dev, &zvm_overall_dev_lists);
		}
	}

    return 0;
}

/**
 * @brief Get the zvm dev lists object
 * @return struct zvm_dev_lists*
 */
struct zvm_dev_lists* get_zvm_dev_lists(void)
{
    return &zvm_overall_dev_lists;
}

/**
 * @brief the vm cpus cache clean options
 */
void set_all_pcpu_cache_clean(void)
{
    set_all_cache_clean();
}

void set_pcpu_cache_clean(uint64_t cpu_id)
{
    set_cpu_cache_clean(cpu_id);
}

void reset_pcpu_cache_clean(uint64_t cpu_mpidr)
{
    uint64_t pcpu_id = (cpu_mpidr & 0xFFF) >> 8;
    reset_cache_clean(pcpu_id);
}

int get_pcpu_cache_clean(uint64_t cpu_mpidr)
{
    int pcpu_id = -1;

#if defined(CONFIG_SOC_QEMU_MAX) || defined(CONFIG_SOC_QEMU_CORTEX_A53)
    pcpu_id = (int)(cpu_mpidr & 0xF);
#elif defined(CONFIG_SOC_RK3568) || defined(CONFIG_SOC_RK3588)
    pcpu_id = (int)((cpu_mpidr & 0xF00) >> 8);
#elif defined(CONFIG_SOC_S5000C)
    pcpu_id = (int)((cpu_mpidr & 0xF0000) >> 16);
#else

    // for other boards,mpidr is discontinuous
    for(int i = 0; i < CONFIG_MP_MAX_NUM_CPUS; i++){
        if((cpu_vmpidr_el2_list[i] & 0x1FFFF) == cpu_mpidr & 0x1FFFF){
            pcpu_id = i;
            break;
        }
    }
    if(pcu_id == -1){
        ZVM_LOG_ERR("cpu_mpidr (0x%016llx) fail to match anyone in cpu_vmpidr_el2_list, "
            "please check the reg value of cpu node in dtsi\n",cpu_mpidr);
        return 0;
    }

#endif
    return get_cache_clean(pcpu_id);
}

/*
 * @brief Main work of this function is to initialize zvm module.
 *
 * All works include:
 * 1. Checkout hardware support for  hypervisor；
 * 2. Initialize struct variable "zvm_overall_info";
 * 3. TODO: Init zvm dev and opration function.
 */
static int zvm_init(void)
{
    int ret = 0;
    void *op = NULL;

    ret = zvm_arch_init(op);
    if (ret) {
       ZVM_LOG_ERR("zvm_arch_init failed here ! \n");
       return ret;
    }

    ret = zvm_overall_init();
    if (ret) {
        ZVM_LOG_ERR("Init zvm_overall struct error. \n ZVM init failed ! \n");
        return ret;
    }

    ret = zvm_devices_list_init();
    if (ret) {
        ZVM_LOG_ERR("Init zvm_dev_list struct error. \n ZVM init failed ! \n");
        return ret;
    }

    /*TODO: ready to init zvm_dev and it's ops */
    zvm_dev_ops_init();

    return ret;
}

/* For using device mmap, the level will switch to APPLICATION. */
SYS_INIT(zvm_init, APPLICATION, CONFIG_ZVM_INIT_PRIORITY);
