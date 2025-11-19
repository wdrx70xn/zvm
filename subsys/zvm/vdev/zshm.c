/*
 * Copyright 2023 HNU-ESNL
 * Copyright 2023 openEuler SIG-Zephyr
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/arch/cpu.h>
#include <zephyr/arch/arm64/lib_helpers.h>
#include <zephyr/net/rk3588_common.h>
#include <zephyr/init.h>
#include <getopt.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/zvm/zvm.h>
#include <zephyr/zvm/vm_device.h>
#include <zephyr/zvm/vdev/vgic_common.h>
#include <zephyr/zvm/vdev/zshm.h>
#include <zephyr/timing/timing.h>
#include <zephyr/cache.h>
#include <zephyr/sys/atomic.h>
LOG_MODULE_DECLARE(ZVM_MODULE_NAME);

#define ZSHMEM_NAME ZSHM

#ifdef CONFIG_HEARTBEAT
static int64_t vm_heartbeat[CONFIG_MAX_VM_NUM] = {0};

int64_t get_vm_heartbeat(uint16_t vm_id)
{
	return vm_heartbeat[vm_id];
}

ALWAYS_INLINE void clear_vm_heartbeat(uint16_t vm_id)
{
	vm_heartbeat[vm_id] = 0;
}
#endif


static inline void cache_sync_before_read(void *addr, size_t size)
{

	dsb();
	sys_cache_data_invd_range(addr, size);
	dsb();
}

static inline void cache_sync_after_write(void *addr, size_t size)
{
	dsb();
	sys_cache_data_flush_range(addr, size);
	dsb();
}
#define DEV_DATA(dev) ((struct virt_device_data *)(dev)->data)
#define DEV_CFG(dev)  ((const struct virt_device_config *const)(dev)->config)

static const struct virtual_device_instance *zshmem_device_instance;
static volatile timing_t start_time, end_time;
static volatile uint64_t total_cycles;
static volatile uint64_t total_ns;

static uint32_t irq;
static uint64_t zshm_bitmap[ZSHM_UNIT_MAX_NUMS / 64];
static zshm_alloc_unit zshm_units[ZSHM_UNIT_MAX_NUMS];

static int vmid_to_units_index[ZSHM_UNIT_MAX_NUMS];

typedef struct zshm_vm_mappings {
	uint8_t *message_slots_addr;
	vm_priority_rings *priority_rings;
	bool is_mapped;
	struct k_mutex recv_lock;
} zshm_vm_mappings;

static zshm_vm_mappings zshm_vm_maps[ZSHM_UNIT_MAX_NUMS];

extern struct zvm_manage_info *zvm_overall_info;

static int zshm_bitmap_set(uint16_t index)
{
	if (index >= ZSHM_UNIT_MAX_NUMS) {
		ZVM_LOG_ERR("Tried to set value outside of bitmap (%d)\n", index);
		return -EINVAL;
	}

	uint16_t idx = index / 64;
	uint16_t off = index % 64;
	zshm_bitmap[idx] |= BIT(off);
	return 0;
}

static int zshm_bitmap_unset(uint16_t index)
{
	if (index >= ZSHM_UNIT_MAX_NUMS) {
		ZVM_LOG_ERR("Tried to unset value outside of bitmap (%d)\n", index);
		return -EINVAL;
	}

	uint16_t idx = index / 64;
	uint16_t off = index % 64;
	zshm_bitmap[idx] &= ~BIT(off);
	return 0;
}

static uint16_t zshm_bitmap_find_first_free()
{
	uint16_t idx, off;

	for (int i = 0; i < ZSHM_UNIT_MAX_NUMS; ++i) {
		idx = i / 64;
		off = i % 64;
		if (!(zshm_bitmap[idx] & BIT(off))) {
			return i;
		}
	}
	return ZSHM_UNIT_MAX_NUMS;
}

static struct z_virt_dev *zshm_alloc_virt_dev(struct z_vm *vm, uint16_t index, int type)
{
	struct z_virt_dev *virt_dev;

	switch (type) {
	case 0:
		virt_dev = vm_virt_dev_add(vm, "ZSHMW", false, false, zshm_units[index].zshm_paddr,
					   ZSHM_VM_SHM_WR_START_PHY_ADDR, ZSHM_UNIT_SIZE, irq, irq);
		break;
	case 1:
		virt_dev = vm_virt_dev_add(vm, "ZSHMN", false, false, zshm_units[index].zshm_notify_addr,
			ZSHM_VM_NOTIFY_SPACE_START_PHY_ADDR, ZSHM_NOTIFY_UNIT_SIZE, irq, irq);
		virt_dev->priv_data = (void *)zshmem_device_instance;
		vm_device_irq_init(vm, virt_dev);
		break;
	case 2:
		virt_dev = vm_virt_dev_add(vm, "ZSHMI", false, false,
					   zshm_units[index].zshm_notify_info_addr,
					   ZSHM_VM_NOTIFY_INFO_SPACE_START_PHY_ADDR,
					   ZSHM_NOTIFY_INFO_UNIT_SIZE, irq, irq);
		break;
	default:
		break;
	}

	return virt_dev;
}

static int zshm_alloc_read_space(struct z_vm *vm, uint16_t index)
{
	struct z_virt_dev *vm_dev;
	uint32_t paddr;
	uint32_t size;
	uint32_t vaddr;
	char devname[7] = "ZSHMR0";

	for (int i = 0; i < 2; ++i) {

		if (i == 0) {
			paddr = ZSHM_START_PHY_ADDR;
			vaddr = ZSHM_VM_SHM_RD_START_PHY_ADDR;
			size = zshm_units[index].zshm_paddr - ZSHM_START_PHY_ADDR;
		} else {
			paddr = zshm_units[index].zshm_paddr + ZSHM_UNIT_SIZE;
			vaddr = ZSHM_VM_SHM_RD_START_PHY_ADDR + zshm_units[index].zshm_paddr -
				ZSHM_START_PHY_ADDR;
			size = ZSHM_SIZE - (zshm_units[index].zshm_paddr - ZSHM_START_PHY_ADDR) -
			       ZSHM_UNIT_SIZE;
		}
		if (!(size > 0)) {
			continue;
		}

		if (i >= 0 && i <= 9) {
			devname[5] = '0' + i;
		}

		vm_dev = vm_virt_dev_add(vm, devname, false, false, paddr, vaddr, size, irq, irq);

		if (vm_dev == NULL) {
			ZVM_LOG_ERR("Failed to allocate virt_dev\n");
			return -1;
		}
	}

	return 0;
}

int zshm_alloc_vm_shm(const struct device *dev, struct z_vm *vm, struct z_virt_dev *vdev_desc)
{
	ARG_UNUSED(dev);
	ARG_UNUSED(vdev_desc);

	uint32_t attrs = 0;
	uint32_t index, ret;
	struct z_virt_dev *vm_dev;

	index = vm->vmid;
	if (index < ZSHM_UNIT_MAX_NUMS) {
		uint16_t idx = index / 64;
		uint16_t off = index % 64;
		if (!(zshm_bitmap[idx] & BIT(off))) {
			ZVM_LOG_INFO("Allocated consistent index %d for VM %d\n", index, vm->vmid);
		} else {
			index = zshm_bitmap_find_first_free();
			ZVM_LOG_WARN(
				"VM %d cannot use consistent index, allocated index %d instead\n",
				vm->vmid, index);
		}
	} else {
		index = zshm_bitmap_find_first_free();
		ZVM_LOG_WARN("VM %d vmid too large, allocated index %d instead\n", vm->vmid, index);
	}

	if (index == ZSHM_UNIT_MAX_NUMS) {
		ZVM_LOG_ERR("No memory available for allocation\n");
		return -1;
	}

	vm->zshm_id = zshm_units[index].id;
	zshm_units[index].vm_id = vm->vmid;
	zshm_bitmap_set(index);
	vmid_to_units_index[vm->vmid] = index;

	ZVM_LOG_INFO(
		"VM %d allocated index %d: paddr=0x%x, notify_addr=0x%x, notify_info_addr=0x%x\n",
		vm->vmid, index, zshm_units[index].zshm_paddr, zshm_units[index].zshm_notify_addr,
		zshm_units[index].zshm_notify_info_addr);
	vm_dev = zshm_alloc_virt_dev(vm, index, 0);
	if (vm_dev == NULL) {
		return -ENOMEM;
	}
	ZVM_LOG_INFO("zshm W paddr:%x vaddr:%x size:%x\n", vm_dev->vm_vdev_paddr,
		     vm_dev->vm_vdev_vaddr, vm_dev->vm_vdev_size);
	uint8_t *mapped_addr;
	k_mem_map_phys_bare(&mapped_addr, zshm_units[index].zshm_paddr, ZSHM_UNIT_SIZE, K_MEM_PERM_RW);

	cache_sync_before_read(mapped_addr, ZSHM_UNIT_SIZE);

	zshm_message *message_slots = (zshm_message *)mapped_addr;
	for (int slot = 0; slot < ZSHM_MESSAGE_SLOT_COUNT; slot++) {
		zshm_message *msg = &message_slots[slot];
		cache_sync_before_read(msg, sizeof(zshm_message));
		msg->priority = 0;
		atomic_set(&msg->receivers, 0);
		memset(msg->data, 0, MAX_DATA_SIZE);
		cache_sync_after_write(msg, sizeof(zshm_message));
	}

	cache_sync_after_write(mapped_addr, ZSHM_UNIT_SIZE);

	k_mem_unmap_phys_bare(mapped_addr, ZSHM_UNIT_SIZE);

	ZVM_LOG_INFO("Initialized %d message slots for VM %d\n", ZSHM_MESSAGE_SLOT_COUNT, vm->vmid);

	vm_dev = zshm_alloc_virt_dev(vm, index, 1);
	if (vm_dev == NULL) {
		return -ENOMEM;
	}
	ZVM_LOG_INFO("zshm N paddr:%x vaddr:%x size:%x\n", vm_dev->vm_vdev_paddr,
		     vm_dev->vm_vdev_vaddr, vm_dev->vm_vdev_size);

	vm_dev = zshm_alloc_virt_dev(vm, index, 2);
	if (vm_dev == NULL) {
		return -ENOMEM;
	}
	attrs = MT_VM_DEVICE_MEM;
	ZVM_LOG_INFO("zshm I paddr:%x vaddr:%x size:%x\n", vm_dev->vm_vdev_paddr,
		     vm_dev->vm_vdev_vaddr, vm_dev->vm_vdev_size);

	uint8_t *priority_rings_mapped;
	k_mem_map_phys_bare(&priority_rings_mapped, zshm_units[index].zshm_notify_info_addr,
			    sizeof(vm_priority_rings),
				K_MEM_PERM_RW | K_MEM_CACHE_NONE);
	vm_priority_rings *priority_rings = (vm_priority_rings *)priority_rings_mapped;

	cache_sync_before_read(priority_rings, sizeof(vm_priority_rings));

	for (int prio = 0; prio < PRIORITY_LEVELS; prio++) {
		priority_ring *ring = &priority_rings->rings[prio];

		cache_sync_before_read(ring, sizeof(priority_ring));

		ring->prod_tail = 0;
		ring->cons_head = 0;

		memset(ring->entries, 0, sizeof(ring->entries));

		cache_sync_after_write(ring, sizeof(priority_ring));
	}

	cache_sync_after_write(priority_rings, sizeof(vm_priority_rings));


	k_mem_unmap_phys_bare(priority_rings_mapped, sizeof(vm_priority_rings));

	ZVM_LOG_INFO("Initialized priority rings for VM %d at notify_info_addr=0x%x\n", vm->vmid,
		     zshm_units[index].zshm_notify_info_addr);

	ret = zshm_alloc_read_space(vm, index);
	if (ret == -1) {
		return -ENOMEM;
	}

	k_mem_map_phys_bare(&zshm_vm_maps[index].message_slots_addr,
			    zshm_units[index].zshm_paddr, ZSHM_UNIT_SIZE,
				K_MEM_PERM_RW );

	uint8_t *priority_rings_addr;
	k_mem_map_phys_bare(&priority_rings_addr,
			    zshm_units[index].zshm_notify_info_addr, sizeof(vm_priority_rings),
			    K_MEM_PERM_RW);
	zshm_vm_maps[index].priority_rings = (vm_priority_rings *)priority_rings_addr;

	zshm_vm_maps[index].is_mapped = true;

	ZVM_LOG_INFO("Created pre-mappings for VM %d: message_slots=0x%p, priority_rings=0x%p\n",
		     vm->vmid, zshm_vm_maps[index].message_slots_addr,
		     zshm_vm_maps[index].priority_rings);

	return 0;
}

int zshm_func_notify(struct z_virt_dev *vdev, uint64_t addr, uint64_t *notify_flag)
{
	int sender_unit_index = vmid_to_units_index[vdev->vm->vmid];

#ifdef CONFIG_HEARTBEAT
	if (*notify_flag == HEARTBEAT_FLAG) {
		vm_heartbeat[vdev->vm->vmid] = k_uptime_get();
		barrier_isync_fence_full();
		barrier_dsync_fence_full();
		barrier_dmem_fence_full();
		return 0;
	}
#endif

	if (*notify_flag == ZSHM_SYSCALL_SLOT) {
		zshm_message *message_slots =
			(zshm_message *)zshm_vm_maps[sender_unit_index].message_slots_addr;
		zshm_message *syscall_msg = &message_slots[ZSHM_SYSCALL_SLOT];
		uint8_t syscall_type = syscall_msg->data[0];
		switch(syscall_type) {
			case 255: {
				dsb();
				syscall_msg->data[1] = (uint8_t)vdev->vm->vmid;
				dsb();
				break;
			}
			default:
				ZVM_LOG_WARN("VM %d: Unknown system call type: %d\n", vdev->vm->vmid, syscall_type);
				break;
		}


		k_mutex_lock(&zshm_vm_maps[sender_unit_index].recv_lock, K_FOREVER);

		priority_ring *ring = &zshm_vm_maps[sender_unit_index].priority_rings->rings[0];

		ring->entries[ring->prod_tail].sender_addr = 255;
		ring->entries[ring->prod_tail].msg_slot = ZSHM_SYSCALL_SLOT;

		ring->prod_tail = (ring->prod_tail + 1) % RING_SIZE;

		dsb();
		k_mutex_unlock(&zshm_vm_maps[sender_unit_index].recv_lock);

		set_virq_to_vcpu(vdev->vm->vcpus[DEFAULT_VCPU], irq, true);

		return 0;
	}

	if (*notify_flag < 0 || *notify_flag > (ZSHM_SEND_SLOT_LIMIT - 1)) {
		//ZVM_LOG_WARN("VM %d: Invalid notify_flag value: %llu\n", vdev->vm->vmid, *notify_flag);
		return 0;
	}

	dsb();

	zshm_message *msg = &((zshm_message *)zshm_vm_maps[sender_unit_index].message_slots_addr)[*notify_flag];

	dsb();

	uint64_t receivers_mask = atomic_get(&msg->receivers);
	uint64_t mask = receivers_mask;
	while (mask) {
		int vm_id = __builtin_ctzll(mask);
		mask &= (mask - 1);

		bool should_decrease_refcount = false;

		if (vm_id >= ZSHM_UNIT_MAX_NUMS) {
			should_decrease_refcount = true;
			goto handle_missing_vm;
		}

		if (!(zvm_overall_info->alloced_vmid & BIT(vm_id))) {
			should_decrease_refcount = true;
			goto handle_missing_vm;
		}

		if (!zvm_overall_info->vms[vm_id] ||
		    !(zvm_overall_info->vms[vm_id]->vm_status & VM_STATE_RUNNING)) {
			should_decrease_refcount = true;
			goto handle_missing_vm;
		}

		int receiver_unit_index = vmid_to_units_index[vm_id];


		handle_missing_vm:
		if (should_decrease_refcount) {
			dsb();
			atomic_and(&msg->receivers, ~(1ULL << vm_id));
			dsb();

			continue;
		}

		priority_ring *ring = &zshm_vm_maps[receiver_unit_index].priority_rings->rings[
			msg->priority >= PRIORITY_LEVELS ? PRIORITY_LEVELS - 1 : msg->priority];

		k_mutex_lock(&zshm_vm_maps[receiver_unit_index].recv_lock, K_FOREVER);

		dsb();

		ring->entries[ring->prod_tail].sender_addr = vdev->vm->vmid;
		ring->entries[ring->prod_tail].msg_slot = *notify_flag;

		dsb();

		ring->prod_tail = (ring->prod_tail + 1) % RING_SIZE;

		dsb();

		k_mutex_unlock(&zshm_vm_maps[receiver_unit_index].recv_lock);

		if (zvm_overall_info->vms[vm_id]) {
			set_virq_to_vcpu(zvm_overall_info->vms[vm_id]->vcpus[DEFAULT_VCPU], irq,true);
		} else {
			ZVM_LOG_ERR("Failed to get VM structure for VM %d\n", vm_id);
		}

	}

	return 0;
}

static int zshm_init(void)
{
	irq = DT_IRQN(DT_NODELABEL(zshm));
	memset(zshm_bitmap, 0, sizeof(zshm_bitmap));
	memset(zshm_units, 0, sizeof(zshm_units));


	for (int i = 0; i < ZSHM_UNIT_MAX_NUMS; ++i) {
		vmid_to_units_index[i] = -1;
	}


	for (int i = 0; i < ZSHM_UNIT_MAX_NUMS; ++i) {
		zshm_vm_maps[i].message_slots_addr = NULL;
		zshm_vm_maps[i].priority_rings = NULL;
		zshm_vm_maps[i].is_mapped = false;

		k_mutex_init(&zshm_vm_maps[i].recv_lock);
	}

	zshm_units[0].id = 0;
	zshm_units[0].zshm_paddr = ZSHM_START_PHY_ADDR;
	zshm_units[0].zshm_notify_addr = ZSHM_NOTIFY_SPACE_START_PHY_ADDR;
	zshm_units[0].zshm_notify_info_addr = ZSHM_NOTIFY_INFO_SPACE_START_PHY_ADDR;

	for (int i = 1; i < ZSHM_UNIT_MAX_NUMS; ++i) {
		zshm_units[i].id = i;
		zshm_units[i].zshm_paddr = zshm_units[i - 1].zshm_paddr + ZSHM_UNIT_SIZE;
		zshm_units[i].zshm_notify_addr =
			zshm_units[i - 1].zshm_notify_addr + ZSHM_NOTIFY_UNIT_SIZE;
		zshm_units[i].zshm_notify_info_addr =
			zshm_units[i - 1].zshm_notify_info_addr + ZSHM_NOTIFY_INFO_UNIT_SIZE;
	}

	return 0;
}

static int zshm_device_init(void)
{
	int i;

	for (i = 0; i < zvm_virtual_devices_count_get(); i++) {
		const struct virtual_device_instance *virtual_device = zvm_virtual_device_get(i);
		if (strcmp(virtual_device->name, TOSTRING(ZSHMEM_NAME))) {
			continue;
		}
		DEV_DATA(virtual_device)->vdevice_type |= VM_DEVICE_PRE_KERNEL_1;
		zshmem_device_instance = virtual_device;

		zshm_init();
		break;
	}
	return 0;
}
static int vm_zshm_deinit(const struct device *dev, struct z_vm *vm, struct z_virt_dev *vdev_desc)
{
	int index = vmid_to_units_index[vm->vmid];

#ifdef CONFIG_HEARTBEAT
	clear_vm_heartbeat(vm->vmid);
#endif

	if (index != -1 && zshm_vm_maps[index].is_mapped) {
		if (zshm_vm_maps[index].message_slots_addr) {
			k_mem_unmap_phys_bare(zshm_vm_maps[index].message_slots_addr,
					      ZSHM_UNIT_SIZE);
			zshm_vm_maps[index].message_slots_addr = NULL;
		}

		if (zshm_vm_maps[index].priority_rings) {
			k_mem_unmap_phys_bare((uint8_t *)zshm_vm_maps[index].priority_rings,
					      sizeof(vm_priority_rings));
			zshm_vm_maps[index].priority_rings = NULL;
		}

		zshm_vm_maps[index].is_mapped = false;
		ZVM_LOG_INFO("Cleaned up pre-mappings for VM %d\n", vm->vmid);
	}

	zshm_bitmap_unset(index);

	zshm_units[index].vm_id = 0;

	zshm_units[index].id = index;
	zshm_units[index].zshm_paddr = ZSHM_START_PHY_ADDR + index * ZSHM_UNIT_SIZE;
	zshm_units[index].zshm_notify_addr =
		ZSHM_NOTIFY_SPACE_START_PHY_ADDR + index * ZSHM_NOTIFY_UNIT_SIZE;
	zshm_units[index].zshm_notify_info_addr =
		ZSHM_NOTIFY_INFO_SPACE_START_PHY_ADDR + index * ZSHM_NOTIFY_INFO_UNIT_SIZE;

	vmid_to_units_index[vm->vmid] = -1;

	ZVM_LOG_INFO("Cleaned up ZSHM resources for VM %d, released index %d\n", vm->vmid, index);

	vm_virt_dev_remove(vm, vdev_desc);
	return 0;
}

static struct virt_device_config zshmem_cfg = {
	.reg_base = DT_REG_ADDR(DT_NODELABEL(zshm)),
	.reg_size = DT_REG_SIZE(DT_NODELABEL(zshm)),
	.hirq_num = DT_IRQN(DT_NODELABEL(zshm)),
	.device_config = NULL,
};
static struct virt_device_data zshmem_data_port = {
	.device_data = NULL,
};

static const zshm_driver_api zshm_driver_apis = {
	.notify = zshm_func_notify,
};

static const struct virt_device_api zshm_api = {
	.init_fn = zshm_alloc_vm_shm,
	.deinit_fn = vm_zshm_deinit,
	.device_driver_api = &zshm_driver_apis,
};


ZVM_VIRTUAL_DEVICE_DEFINE(zshm_device_init, POST_KERNEL, CONFIG_ZSHM_INIT_PRIORITY, ZSHMEM_NAME,
			  zshmem_data_port, zshmem_cfg, zshm_api);
