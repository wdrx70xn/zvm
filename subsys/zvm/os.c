/*
 * Copyright 2024-2025 HNU-ESNL: Guoqi Xie, Chenglai Xiong, Xingyu Hu and etc.
 * Copyright 2024-2025 openEuler SIG-Zephyr
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include <autoconf.h>
#include <zephyr/kernel.h>
#include <getopt.h>
#include <zephyr/zvm/os.h>
#include <zephyr/zvm/zvm.h>
#include <zephyr/zvm/vm_cpu.h>

LOG_MODULE_DECLARE(ZVM_MODULE_NAME);

#define MB_SIZE     (1024 * 1024)

/**
 * Template of guest OS, now for linux and zephyr.
*/
static struct z_os_info guest_os_template[] = {
    {
        .os_type = OS_TYPE_ZEPHYR,
        .vcpu_num = ZEPHYR_VM_VCPU_NUM,
        .vm_mem_base = RTOS_VM_MEMORY_BASE,
        .vm_mem_size = RTOS_VM_MEMORY_SIZE,
        .vm_image_base = ZEPHYR_IMAGE_BASE,
        .vm_image_size = ZEPHYR_IMAGE_SIZE,
        .vm_load_base = RTOS_VM_LOAD_BASE,
        .entry_point = RTOS_VM_MEMORY_BASE,
    },
    {
        .os_type = OS_TYPE_LINUX,
        .vcpu_num = LINUX_VM_VCPU_NUM,
        .vm_mem_base = LINUX_VM_MEMORY_BASE,
        .vm_mem_size = LINUX_VM_MEMORY_SIZE,
        .vm_image_base = LINUX_IMAGE_BASE,
        .vm_image_size = LINUX_IMAGE_SIZE,
        .vm_load_base = LINUX_VM_LOAD_BASE,
        .entry_point = LINUX_VM_MEMORY_BASE,
    },
    {
        .os_type = OS_TYPE_FREERTOS,
        .vcpu_num = FREERTOS_VM_VCPU_NUM,
        .vm_mem_base = RTOS_VM_MEMORY_BASE,
        .vm_mem_size = RTOS_VM_MEMORY_SIZE,
        .vm_image_base = FREERTOS_IMAGE_BASE,
        .vm_image_size = FREERTOS_IMAGE_SIZE,
        .vm_load_base = RTOS_VM_LOAD_BASE,
        .entry_point = RTOS_VM_MEMORY_BASE,
    },

};


#define COUNT_CPY(n) +1
/** zephyr cpy */
#define ZEPHYR_CPY_NUM (0 DT_FOREACH_CHILD(DT_PATH(rtos_guest_space, zephyr_cpies), COUNT_CPY))

typedef struct {
    uint16_t vcpu_num;
    uint32_t max_mem_size;
    uint32_t vm_image_base;
    uint32_t vm_image_size;
} zephyr_cpy_config_t;

// 使用宏展开生成四元数组
#define ZEPHYR_CPY_CONFIG(node_id) \
    { DT_PROP(node_id, vcpu_num), DT_PROP(node_id, max_mem_size), \
    DT_REG_ADDR(node_id), DT_REG_SIZE(node_id) }

// 生成完整的配置数组
const zephyr_cpy_config_t zephyr_cpies_config[ZEPHYR_CPY_NUM] = {
    DT_FOREACH_CHILD_SEP(DT_PATH(rtos_guest_space, zephyr_cpies), ZEPHYR_CPY_CONFIG, (,))
};

/** FreeRTOS cpy */
#define FREERTOS_CPY_NUM (0 DT_FOREACH_CHILD(DT_PATH(rtos_guest_space, freertos_cpies), COUNT_CPY))

typedef struct {
    uint16_t vcpu_num;
    uint32_t max_mem_size;
    uint32_t vm_image_base;
    uint32_t vm_image_size;
} freertos_cpy_config_t;

// 使用宏展开生成四元数组
#define FREERTOS_CPY_CONFIG(node_id) \
    { DT_PROP(node_id, vcpu_num), DT_PROP(node_id, max_mem_size), \
    DT_REG_ADDR(node_id), DT_REG_SIZE(node_id) }

// 生成完整的配置数组
const freertos_cpy_config_t freertos_cpies_config[FREERTOS_CPY_NUM] = {
    DT_FOREACH_CHILD_SEP(DT_PATH(rtos_guest_space, freertos_cpies), ZEPHYR_CPY_CONFIG, (,))
};

// /** linux cpy */

#define LINUX_DTB_CPY_NUM \
        (0 DT_FOREACH_CHILD(DT_PATH(linux_guest_space, linux_dtb_cpies), COUNT_CPY))

typedef struct {
    uint16_t vcpu_num;
    uint32_t max_mem_size;
    uint32_t dtb_image_base;
    uint32_t dtb_image_size;
} linux_dtb_cpy_config_t;

// 使用宏展开生成四元数组
#define LINUX_DTB_CPY_CONFIG(node_id) \
    { DT_PROP(node_id, vcpu_num), DT_PROP(node_id, max_mem_size), \
    DT_REG_ADDR(node_id), DT_REG_ADDR(node_id) }

// 生成完整的配置数组
const linux_dtb_cpy_config_t linux_dtb_cpies_config[LINUX_DTB_CPY_NUM] = {
    DT_FOREACH_CHILD_SEP(DT_PATH(linux_guest_space, linux_dtb_cpies), LINUX_DTB_CPY_CONFIG, (,))
};

/* This only 1 value now*/
// 所有linux_cpy 的 image addr集合
const uint64_t linux_cpies_image_addr[] = {
    DT_PROP(DT_NODELABEL(linux_node), image_addr)
};
// 所有linux_cpy 的 image size集合
const uint64_t linux_cpies_image_size[] = {
    DT_PROP(DT_NODELABEL(linux_node), image_size)
};

// 所有linux_cpy 的 rootfs addr集合
const uint64_t linux_cpies_rootfs_addr[] = {
    DT_PROP(DT_NODELABEL(linux_node), rootfs_addr)
};
// 所有linux_cpy 的 rootfs size集合
const uint64_t linux_cpies_rootfs_size[] = {
    DT_PROP(DT_NODELABEL(linux_node), rootfs_size)
};

struct GuestMemoryPart {
    uint64_t start_addr;   // 内存块起始地址
    uint64_t size;         // 内存块大小
    bool is_allocated;     // 是否已分配
    uint16_t os_type;   // 所属OS类型
};

struct GuestMemoryManager {
    uint64_t total_memory; // 总内存大小
    uint64_t base_addr;    // 内存基地址
    struct GuestMemoryPart blk_list[16]; // 内存块列表
    uint32_t blk_count;    // 当前内存块数量
};

struct GuestMemoryManager guest_mem_manager;

void guest_memory_manager_init(uint64_t base_addr, uint64_t total_mem)
{
    memset(&guest_mem_manager, 0, sizeof(struct GuestMemoryManager));
    guest_mem_manager.base_addr = base_addr;
    guest_mem_manager.total_memory = total_mem;
    guest_mem_manager.blk_count = 1;  // 初始只有一个空闲块

    // 初始化唯一空闲块
    guest_mem_manager.blk_list[0].start_addr = base_addr;
    guest_mem_manager.blk_list[0].size = total_mem;
    guest_mem_manager.blk_list[0].is_allocated = false;

    ZVM_LOG_INFO(
        "guest_memory_manager_init: base_addr:0x%llx total_mem:0x%llx\n",
        base_addr, total_mem);
}
// 可视化打印内存块状态
static void visualize_memory_blocks(void)
{
    struct GuestMemoryManager *mm = &guest_mem_manager;

    printk("+----------------+----------------+----------------+------------+\n");
    printk("|   Start Addr   |    End Addr    |     Size       |   Status   |\n");
    printk("+----------------+----------------+----------------+------------+\n");

    for (uint32_t i = 0; i < mm->blk_count; i++) {
        struct GuestMemoryPart *blk = &mm->blk_list[i];
        uint64_t end_addr = blk->start_addr + blk->size - 1;

        printk("| 0x%-12llx | 0x%-12llx | 0x%-12llx | %-10s |\n",
                    blk->start_addr,
                    end_addr,
                    blk->size,
                    blk->is_allocated ? "ALLOC" : "FREE");
    }
    printk("+----------------+----------------+----------------+------------+\n\n");
}

uint64_t allocate_guest_memory_part(uint16_t os_type, uint64_t size)
{
    struct GuestMemoryManager *mm = &guest_mem_manager;

    // 遍历查找第一个足够大的空闲块
    for (uint32_t i = 0; i < mm->blk_count; i++) {
        struct GuestMemoryPart *blk = &mm->blk_list[i];

        if (!blk->is_allocated && blk->size >= size) {
            // 记录原始块信息
            uint64_t original_start = blk->start_addr;
            uint64_t original_size = blk->size;

            // 占用这个块
            blk->start_addr = original_start;
            blk->size = size;
            blk->is_allocated = true;
            blk->os_type = os_type;

            // 如果块比需要的大，分割剩余部分
            if (original_size > size) {
                if (mm->blk_count >= 16) {
                    ZVM_LOG_ERR("No more block space available for allocation.\n");
                    return 0; // 没有更多块空间
                }

                // 在当前位置后面插入新的空闲块
                for (uint32_t j = mm->blk_count; j > i + 1; j--) {
                    mm->blk_list[j] = mm->blk_list[j-1];
                }

                // 创建新的空闲块
                mm->blk_list[i+1].start_addr = original_start + size;
                mm->blk_list[i+1].size = original_size - size;
                mm->blk_list[i+1].is_allocated = false;

                mm->blk_count++;
            }

            ZVM_LOG_INFO("Allocate guest memory part start_addr:%llx size:%llx\n",
                        blk->start_addr, blk->size);

            // 可视化打印分配后的内存状态
            ZVM_LOG_INFO("\nMemory Blocks %s (addr:0x%llx, size:0x%llx)\n", 
                    "AFTER ALLOCATION", blk->start_addr, blk->size);
            visualize_memory_blocks();

            return blk->start_addr;
        }
    }

    visualize_memory_blocks();
    return 0; // 分配失败
}

// 修改一个只判断内存的接口，但不能修改blk结构体
// 返回 1 表示可以分配
// 返回 0 表示不可以分配
bool allocate_guest_memory_part_check(uint64_t size){
    struct GuestMemoryManager *mm = &guest_mem_manager;

    // 遍历查找第一个足够大的空闲块
    for (uint32_t i = 0; i < mm->blk_count; i++) {
        struct GuestMemoryPart *blk = &mm->blk_list[i];

        if (!blk->is_allocated && blk->size >= size) {

            // 如果块比需要的大，分割剩余部分
            if (blk->size > size) {
                if (mm->blk_count >= 16) {
                    return false; // 没有更多块空间
                }

            }

            return true; // 可以分配
        }
    }

    return false; // 分配失败
}

void free_guest_memory_part(uint64_t addr)
{
    struct GuestMemoryManager *mm = &guest_mem_manager;
    int32_t index = -1;

    // 查找要释放的块
    for (uint32_t i = 0; i < mm->blk_count; i++) {
        if (mm->blk_list[i].start_addr == addr && mm->blk_list[i].is_allocated) {
            index = i;
            break;
        }
    }

    if (index == -1) {
        return; // 未找到
    }

    // 打印释放前的状态
    ZVM_LOG_INFO("Free guest memory part start_addr:%llx size:%llx\n",
                mm->blk_list[index].start_addr, mm->blk_list[index].size);


    // 标记为未分配
    mm->blk_list[index].is_allocated = false;

    // 尝试合并相邻的空闲块
    // 向后合并
    if (index < mm->blk_count - 1 && 
        !mm->blk_list[index+1].is_allocated &&
        mm->blk_list[index].start_addr + mm->blk_list[index].size == mm->blk_list[index+1].start_addr) {
        mm->blk_list[index].size += mm->blk_list[index+1].size;

        // 移动后面的块向前
        for (uint32_t i = index+1; i < mm->blk_count-1; i++) {
            mm->blk_list[i] = mm->blk_list[i+1];
        }
        mm->blk_count--;

        // ZVM_LOG_INFO("Merged with NEXT free block\n");
    }

    // 向前合并
    if (index > 0 &&
        !mm->blk_list[index-1].is_allocated &&
        mm->blk_list[index-1].start_addr + mm->blk_list[index-1].size == mm->blk_list[index].start_addr) {
        mm->blk_list[index-1].size += mm->blk_list[index].size;

        // 移动后面的块向前
        for (uint32_t i = index; i < mm->blk_count-1; i++) {
            mm->blk_list[i] = mm->blk_list[i+1];
        }
        mm->blk_count--;

        // ZVM_LOG_INFO("Merged with PREVIOUS free block\n");
    }

    // 可视化打印释放后的内存状态
    ZVM_LOG_INFO("Memory Blocks %s (addr:0x%llx, size:0x%d)\n",
                 "AFTER FREE", addr, 0);
    visualize_memory_blocks();
}

ALWAYS_INLINE bool guest_os_is_rtos(uint16_t os_type)
{
    if (os_type == OS_TYPE_LINUX)
    {
        return false;
    }
    return true;
}

void list_available_config(uint16_t os_type) {
    switch (os_type) {
    case OS_TYPE_ZEPHYR:
        ZVM_LOG_WARN("Available Zephyr:\n");
        for (uint16_t i = 0; i < ZEPHYR_CPY_NUM; i++) {
            printk(
                "vCPU num:%d, Memory Size:0x%x\n",
                zephyr_cpies_config[i].vcpu_num,
                zephyr_cpies_config[i].max_mem_size
            );
        }
        break;
    case OS_TYPE_LINUX:
        ZVM_LOG_WARN("Available Linux:\n");
        for(uint16_t i = 0;i < ZEPHYR_CPY_NUM; i++)
        {
            printk(
                "vCPU num:%d, Memory Size:0x%x\n",
                linux_dtb_cpies_config[i].vcpu_num,
                linux_dtb_cpies_config[i].max_mem_size
            );
        }
        break;
    case OS_TYPE_FREERTOS:
        ZVM_LOG_WARN("Available FreeRTOS:\n");
        for(uint16_t i = 0;i < FREERTOS_CPY_NUM; i++)
        {
            printk(
                "vCPU num:%d, Memory Size:0x%x\n",
                freertos_cpies_config[i].vcpu_num,
                freertos_cpies_config[i].max_mem_size
            );
        }
        break;
    default:
        ZVM_LOG_ERR("Not a valid os type\n");
        break;
    }
}

void * guest_config_available(uint16_t vcpu_num, uint16_t os_type, uint32_t mem_size)
{
    switch (os_type){
    case OS_TYPE_ZEPHYR:
        for(uint16_t i = 0;i < ZEPHYR_CPY_NUM; i++){
            if(vcpu_num == zephyr_cpies_config[i].vcpu_num && \
                mem_size == zephyr_cpies_config[i].max_mem_size){
                return (void *)&zephyr_cpies_config[i];
            }
        }
        break;
    case OS_TYPE_LINUX:
        for(uint16_t i = 0;i < LINUX_DTB_CPY_NUM; i++){
            if(vcpu_num == linux_dtb_cpies_config[i].vcpu_num && \
                mem_size == linux_dtb_cpies_config[i].max_mem_size){
                return (void *)&linux_dtb_cpies_config[i];
            }
        }
        break;
    case OS_TYPE_FREERTOS:
        for(uint16_t i = 0;i < FREERTOS_CPY_NUM; i++){
            if(vcpu_num == freertos_cpies_config[i].vcpu_num && \
                mem_size == freertos_cpies_config[i].max_mem_size){
                return (void *)&freertos_cpies_config[i];
            }
        }
        break;
    default:
        ZVM_LOG_ERR("Not a valid os type\n");
        break;
    }
    return NULL;
}

/* Overall VM's OS info for create VM. */
static struct z_os_info z_overall_vm_info[CONFIG_MAX_VM_NUM];

int get_os_type_from_args(const char *os_type)
{
    if (strcmp(os_type, "zephyr") == 0){
        return OS_TYPE_ZEPHYR;
    }
    if (strcmp(os_type, "linux") == 0){
        return OS_TYPE_LINUX;
    }
    if (strcmp(os_type, "freertos") == 0){
        return OS_TYPE_FREERTOS;
    }

    ZVM_LOG_WARN("The VM type is not supported(Linux or zephyr). \n Please try again! \n");
    return -EINVAL;
}


int get_os_template_type(struct z_os_info guest_os, bool special_cpu,  bool special_mem)
{
    int tmp_vmid = 0;
    uint16_t temp_vcpu_num;
    uint32_t temp_mem_size;
    uint64_t temp_load_base;
    void *cfg = NULL;

    tmp_vmid = allocate_vmid();
    if (tmp_vmid >= CONFIG_MAX_VM_NUM) {
        return -EOVERFLOW;
    }

    z_overall_vm_info[tmp_vmid].os_type = guest_os_template[guest_os.os_type].os_type;
    z_overall_vm_info[tmp_vmid].vcpu_num = guest_os_template[guest_os.os_type].vcpu_num;
    z_overall_vm_info[tmp_vmid].vm_mem_base = guest_os_template[guest_os.os_type].vm_mem_base;
    z_overall_vm_info[tmp_vmid].vm_mem_size = guest_os_template[guest_os.os_type].vm_mem_size;
    z_overall_vm_info[tmp_vmid].vm_image_base = guest_os_template[guest_os.os_type].vm_image_base;
    z_overall_vm_info[tmp_vmid].vm_image_size = guest_os_template[guest_os.os_type].vm_image_size;
    z_overall_vm_info[tmp_vmid].entry_point = guest_os_template[guest_os.os_type].entry_point;

    if (special_mem) {
        temp_mem_size = guest_os.vm_mem_size;
    } else {
        temp_mem_size = guest_os_template[guest_os.os_type].vm_mem_size;
    }

    temp_load_base = allocate_guest_memory_part(z_overall_vm_info[tmp_vmid].os_type, temp_mem_size);

    if (temp_load_base == 0) {
        ZVM_LOG_WARN("Allocate guest memory part failed (size:0x%x)\n", temp_mem_size);
        goto fail;
    }

    z_overall_vm_info[tmp_vmid].vm_load_base = temp_load_base;

    if (special_cpu) {
        temp_vcpu_num = guest_os.vcpu_num;
        if (temp_vcpu_num > CONFIG_MAX_VCPU_PER_VM) {
            goto fail;
        }
    } else {
        temp_vcpu_num = z_overall_vm_info[tmp_vmid].vcpu_num;
    }
    if (!vcpu_margin_check(guest_os.os_type, temp_vcpu_num))
    {
        goto fail;
    }
    // check if the config of [vcpu num, mem szie] is available
    cfg = \
        guest_config_available(
            temp_vcpu_num, guest_os.os_type, temp_mem_size
        );

    if (cfg != NULL)
    {
        if (guest_os.os_type == OS_TYPE_ZEPHYR)
        {
            z_overall_vm_info[tmp_vmid].vm_image_base = \
                ((zephyr_cpy_config_t *)cfg)->vm_image_base;
            z_overall_vm_info[tmp_vmid].vm_image_size = \
                ((zephyr_cpy_config_t *)cfg)->vm_image_size;
        } else if (guest_os.os_type == OS_TYPE_LINUX) {
            z_overall_vm_info[tmp_vmid].vm_image_base = \
                linux_cpies_image_addr[0];
            z_overall_vm_info[tmp_vmid].vm_image_size = \
                linux_cpies_image_size[0];
        } else if (guest_os.os_type == OS_TYPE_FREERTOS) {
            z_overall_vm_info[tmp_vmid].vm_image_base = \
                ((zephyr_cpy_config_t *)cfg)->vm_image_base;
            z_overall_vm_info[tmp_vmid].vm_image_size = \
                ((zephyr_cpy_config_t *)cfg)->vm_image_size;
        }
        z_overall_vm_info[tmp_vmid].vcpu_num = temp_vcpu_num;
        z_overall_vm_info[tmp_vmid].vm_mem_size = temp_mem_size;
    } else {
        ZVM_LOG_INFO(
            "Not a valid config for vCPU:%x memory size:0x%x\n",
            temp_vcpu_num,
            temp_mem_size
        );
        /* The invalid configuration was specified (not the default). */
        goto fail;
    }

    z_overall_vm_info[tmp_vmid].linux_dtb_load_addr =
        LINUX_DTB_MEM_BASE + LINUX_DTB_MEM_SIZE * tmp_vmid;
    z_overall_vm_info[tmp_vmid].linux_dtb_load_size =
        LINUX_DTB_MEM_SIZE;

    ZVM_LOG_INFO(
        "Create configuration of vCPU:%x memory size:0x%llx successful!\n",
        z_overall_vm_info[tmp_vmid].vcpu_num,
        z_overall_vm_info[tmp_vmid].vm_mem_size
    );

    if (guest_os.os_type == OS_TYPE_LINUX)
    {
        ZVM_LOG_INFO(
            "Linux DTB memory region addr:0x%llx size:0x%llx\n",
            z_overall_vm_info[tmp_vmid].linux_dtb_load_addr,
            z_overall_vm_info[tmp_vmid].linux_dtb_load_size
        );
    }

    return tmp_vmid;

fail:	/* Free the VMID upon failure */
    free_guest_memory_part(z_overall_vm_info[tmp_vmid].vm_load_base);
    z_overall_vm_info[tmp_vmid].os_type       = 0;
    z_overall_vm_info[tmp_vmid].vcpu_num      = 0;
    z_overall_vm_info[tmp_vmid].vm_mem_base   = 0;
    z_overall_vm_info[tmp_vmid].vm_mem_size   = 0;
    z_overall_vm_info[tmp_vmid].vm_image_base = 0;
    z_overall_vm_info[tmp_vmid].vm_image_size = 0;
    z_overall_vm_info[tmp_vmid].vm_load_base  = 0;
    z_overall_vm_info[tmp_vmid].entry_point   = 0;

    free_vmid(tmp_vmid);
    ZVM_LOG_INFO("free vmid %d\n",tmp_vmid);
    return -EINVAL;

}
#define LOAD_IMAGE_DEBUG 1

int load_vm_image(struct vm_mem_domain *vmem_domain, struct z_os *os)
{
    int ret = 0;
    uint64_t *src_hva, des_hva;
    uint64_t num_m = os->info.vm_image_size / MB_SIZE;
    uint64_t src_hpa = os->info.vm_image_base;
    uint64_t des_hpa = os->info.vm_load_base;
    uint64_t per_size = MB_SIZE;
#if LOAD_IMAGE_DEBUG
    ZVM_LOG_INFO("OS Image Loading ...\n");
    ZVM_LOG_INFO("Image_size = %lld MB\n", num_m);
    ZVM_LOG_INFO("Image_src_hpa = 0x%llx \n", src_hpa);
    ZVM_LOG_INFO("Image_des_hpa = 0x%llx \n", des_hpa);
#endif
    while(num_m) {
        k_mem_map_phys_bare((uint8_t **)&src_hva, (uintptr_t)src_hpa, per_size, K_MEM_CACHE_NONE | K_MEM_PERM_RW);
        k_mem_map_phys_bare((uint8_t **)&des_hva, (uintptr_t)des_hpa, per_size, K_MEM_CACHE_NONE | K_MEM_PERM_RW);
        memcpy((void *)des_hva, src_hva, per_size);
        k_mem_unmap_phys_bare((uint8_t *)src_hva, per_size);
        k_mem_unmap_phys_bare((uint8_t *)des_hva, per_size);
        des_hpa += per_size;
        src_hpa += per_size;
        num_m--;
    }

    if (os->info.os_type != OS_TYPE_LINUX){
        // ZVM_LOG_INFO("OS Image Loaded, No need other file!\n");
        return ret;
    }

    num_m = os->info.linux_dtb_load_size / MB_SIZE;
    // src_hpa = LINUX_VMDTB_BASE;
    src_hpa = ((linux_dtb_cpy_config_t *)\
        guest_config_available(os->info.vcpu_num, os->info.os_type, os->info.vm_mem_size)
    )->dtb_image_base;

    des_hpa = os->info.linux_dtb_load_addr;
#if LOAD_IMAGE_DEBUG
    ZVM_LOG_INFO("DTB Image Loading ...\n");
    ZVM_LOG_INFO("DTB_size = %lld MB\n", num_m);
    ZVM_LOG_INFO("DTB_src_hpa = 0x%llx\n", src_hpa);
    ZVM_LOG_INFO("DTB_des_hpa = 0x%llx\n", des_hpa);
#endif
    while(num_m) {
        k_mem_map_phys_bare((uint8_t **)&src_hva, (uintptr_t)src_hpa, per_size, K_MEM_CACHE_NONE | K_MEM_PERM_RW);
        k_mem_map_phys_bare((uint8_t **)&des_hva, (uintptr_t)des_hpa, per_size, K_MEM_CACHE_NONE | K_MEM_PERM_RW);
        memcpy((void *)des_hva, src_hva, per_size);
        k_mem_unmap_phys_bare((uint8_t *)src_hva, per_size);
        k_mem_unmap_phys_bare((uint8_t *)des_hva, per_size);
        des_hpa += per_size;
        src_hpa += per_size;
        num_m--;
    }

    num_m = LINUX_VMRFS_SIZE / MB_SIZE;
    src_hpa = LINUX_VMRFS_BASE;
    des_hpa = \
        os->info.vm_load_base + 0x9000000;

    /* only 1 rootfs now*/
    src_hpa = linux_cpies_rootfs_addr[0];
    num_m = linux_cpies_rootfs_size[0] / MB_SIZE;

#if LOAD_IMAGE_DEBUG
    ZVM_LOG_INFO("FS Image Loading ...\n");
    ZVM_LOG_INFO("FS_size = %lld MB\n", num_m);
    ZVM_LOG_INFO("FS_src_hpa = 0x%llx\n", src_hpa);
    ZVM_LOG_INFO("FS_des_hpa = 0x%llx\n", des_hpa);
#endif
    while(num_m) {
        k_mem_map_phys_bare((uint8_t **)&src_hva, (uintptr_t)src_hpa, per_size, K_MEM_CACHE_NONE | K_MEM_PERM_RW);
        k_mem_map_phys_bare((uint8_t **)&des_hva, (uintptr_t)des_hpa, per_size, K_MEM_CACHE_NONE | K_MEM_PERM_RW);
        memcpy((void *)des_hva, src_hva, per_size);
        k_mem_unmap_phys_bare((uint8_t *)src_hva, per_size);
        k_mem_unmap_phys_bare((uint8_t *)des_hva, per_size);
        des_hpa += per_size;
        src_hpa += per_size;
        num_m--;
    }
#if LOAD_IMAGE_DEBUG
    ZVM_LOG_INFO("Linux FS Image Loaded !\n");
#endif
    return ret;
}

int vm_os_create(struct z_os* guest_os, int vm_tmpid)
{
    guest_os->name = (char *)k_malloc(sizeof(char)*OS_NAME_LENGTH);
    memset(guest_os->name, '\0', OS_NAME_LENGTH);

    switch(z_overall_vm_info[vm_tmpid].os_type){
    case OS_TYPE_LINUX:
        strcpy(guest_os->name, "linux_os");
        guest_os->is_rtos = false;
        break;
    case OS_TYPE_ZEPHYR:
        strcpy(guest_os->name, "zephyr_os");
        guest_os->is_rtos = true;
        break;
    case OS_TYPE_FREERTOS:
        strcpy(guest_os->name, "freertos_os");
        guest_os->is_rtos = true;
        break;
    default:
        return -ENXIO;
    }
    guest_os->info.os_type = z_overall_vm_info[vm_tmpid].os_type;
    guest_os->info.vm_mem_base = z_overall_vm_info[vm_tmpid].vm_mem_base;
    guest_os->info.vm_mem_size = z_overall_vm_info[vm_tmpid].vm_mem_size;
    guest_os->info.vm_image_base = z_overall_vm_info[vm_tmpid].vm_image_base;
    guest_os->info.vm_image_size = z_overall_vm_info[vm_tmpid].vm_image_size;
    guest_os->info.vcpu_num = z_overall_vm_info[vm_tmpid].vcpu_num;
    guest_os->info.entry_point = z_overall_vm_info[vm_tmpid].entry_point;
    guest_os->info.vm_load_base = z_overall_vm_info[vm_tmpid].vm_load_base;
    guest_os->info.linux_dtb_load_addr = z_overall_vm_info[vm_tmpid].linux_dtb_load_addr;
    guest_os->info.linux_dtb_load_size = z_overall_vm_info[vm_tmpid].linux_dtb_load_size;
    return 0;
}
